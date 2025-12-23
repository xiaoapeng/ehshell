/**
 * @file ehshell_core.c
 * @brief 
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-11-09
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>

#include <eh.h>
#include <eh_mem.h>
#include <eh_error.h>
#include <eh_ringbuf.h>
#include <eh_debug.h>

#include <eh_formatio.h>
#include <eh_signal.h>

#include <ehshell.h>
#include <ehshell_internal.h>
#include <ehshell_escape_char.h>


#ifndef EH_DBG_MODEULE_LEVEL_EHSHELL
#define EH_DBG_MODEULE_LEVEL_EHSHELL EH_DBG_INFO
#endif

static ehshell_t *ehshell_default_shell = NULL;

static void ehshell_stream_write(void *ctx, const uint8_t *buf, size_t len){
    struct stream_function_no_cache *stream = (struct stream_function_no_cache *)ctx;
    ehshell_t *shell = eh_container_of(stream, ehshell_t, stream);
    shell->config->write(shell, (const char *)buf, len);
}

static void ehshell_stream_finish(void *ctx){
    struct stream_function_no_cache *stream = (struct stream_function_no_cache *)ctx;
    ehshell_t *shell = eh_container_of(stream, ehshell_t, stream);
    shell->config->finish(shell);
}   

static void ehshell_print_welcome(ehshell_t *shell){
    shell->config->write(shell, EHSHELL_CONFIG_WELCOME, sizeof(EHSHELL_CONFIG_WELCOME) - 1);
}
static void ehshell_print_prompt(ehshell_t *shell){
    if(shell->linebuf_data_len){
        if(shell->linebuf_pos != shell->linebuf_data_len){
            eh_stream_printf((struct stream_base *)&shell->stream, "root@%s $ %.*s\x1B[%dD", 
                shell->config->host, shell->linebuf_data_len, ehshell_linebuf(shell), shell->linebuf_data_len-shell->linebuf_pos);
        }else{
            eh_stream_printf((struct stream_base *)&shell->stream, "root@%s $ %.*s",
                shell->config->host, shell->linebuf_data_len, ehshell_linebuf(shell));
        }
    }else{
        eh_stream_printf((struct stream_base *)&shell->stream, "root@%s $ ", shell->config->host);
    }
}

static void ehshell_input_reset(ehshell_t *shell){
    shell->linebuf_pos = 0;
    shell->linebuf_data_len = 0;
    shell->escape_char_match_state = 0;
}


static int _ehshell_command_run_form_string(ehshell_t *ehshell, char *cmd_str)
{
    const char *argv[EHSHELL_CONFIG_ARGC_MAX] = {0};
    int argc = 0;

    char *p = cmd_str;
    int in_quote = 0;
    char quote_char = 0;

    while (*p != '\0') {

        /* 跳过前导空白 */
        while (isspace((unsigned char)*p))
            p++;

        if (*p == '\0')
            break;

        if (argc >= EHSHELL_CONFIG_ARGC_MAX){
            eh_stream_printf((struct stream_base *)&ehshell->stream, "command \"%s\" argc overflow %d\r\n", cmd_str, argc);
            eh_stream_finish((struct stream_base *)&ehshell->stream);
            return EH_RET_INVALID_PARAM;
        }

        /* 记录当前参数起点 */
        argv[argc++] = p;

        /* token 内部解析 */
        char *w = p;  /* 写指针，负责就地压缩   */
        while (*p != '\0') {

            if (*p == '\\') {
                /* 转义：删除 '\'，保留后一个字符 */
                p++;
                if (*p == '\0') {
                    break;
                }
                *w++ = *p++;
                continue;
            }

            if (!in_quote && (*p == '"' || *p == '\'')) {
                /* 进入引号 */
                in_quote = 1;
                quote_char = *p;
                p++;
                continue;
            }

            if (in_quote && *p == quote_char) {
                /* 结束引号 */
                in_quote = 0;
                quote_char = 0;
                p++;
                continue;
            }

            /* 如果未在引号中，遇到空白即 token 结束 */
            if (!in_quote && isspace((unsigned char)*p)) {
                p++;
                break;
            }

            /* 普通字符，复制 */
            *w++ = *p++;
        }

        /* token 结束，写 '\0' */
        *w = '\0';
    }

    /* 检查引号是否闭合 */
    if (in_quote) {
        eh_stream_printf((struct stream_base *)&ehshell->stream, "command %s quote not close\r\n", cmd_str);
        eh_stream_finish((struct stream_base *)&ehshell->stream);
        return EH_RET_INVALID_PARAM;  /* 引号未闭合 */
    }
    /* 调用执行函数 */
    return ehshell_command_run(ehshell, argc, argv);
}
static size_t ehshell_command_common_prefix_len(const char *s1, size_t s1_len, const char *s2) {
    size_t prefix_len = 0;
    while (prefix_len < s1_len && s2[prefix_len] != '\0' && s1[prefix_len] == s2[prefix_len]) {
        prefix_len++;
    }
    return prefix_len;
}
static void ehshell_command_auto_complete(ehshell_t *shell){
    char *linebuf = ehshell_linebuf(shell);
    size_t linebuf_pos = shell->linebuf_pos;
    char *cmd = linebuf;
    const char *first_completion = NULL;
    bool is_multi_match = false;
    size_t completion_len = 0;
    size_t diff;
    const struct ehshell_command_info** command_info_tab = ehshell_command_info_tab(shell);
    if(linebuf_pos == 0){
        return;
    }
    /* 如果中间有空格，就返回 */
    for(size_t i=0; i < linebuf_pos; i++){
        if(isspace((unsigned char)cmd[i])){
            return;
        }
    }
    for(size_t i=0; i < shell->command_count; i++){
        const struct ehshell_command_info *command_info = command_info_tab[i];
        if(strncmp(cmd, command_info->command, linebuf_pos) == 0){
            if(first_completion == NULL){
                first_completion = command_info->command;
                completion_len = strlen(first_completion);
                continue;
            }
            completion_len = ehshell_command_common_prefix_len(first_completion, completion_len, command_info->command);
            if(is_multi_match == false){
                is_multi_match = true;
                eh_stream_puts((struct stream_base *)&shell->stream, "\r\n");
                eh_stream_puts((struct stream_base *)&shell->stream, first_completion);
                eh_stream_puts((struct stream_base *)&shell->stream, "\t\t");
            }
            eh_stream_puts((struct stream_base *)&shell->stream, command_info->command);
            eh_stream_puts((struct stream_base *)&shell->stream, "\t\t");
        }
    }
        if(is_multi_match){
        eh_stream_puts((struct stream_base *)&shell->stream, "\r\n");
        ehshell_print_prompt(shell);
    }
    
    if(first_completion == NULL)
        return ;
    diff = completion_len - linebuf_pos;
    if(diff == 0 || (shell->linebuf_data_len + diff) >= shell->config->input_linebuf_size)
        return;
    if(linebuf_pos < shell->linebuf_data_len){
        int n = (int)(shell->linebuf_data_len - linebuf_pos);
        memmove(linebuf + completion_len, linebuf + linebuf_pos, (size_t)n);
        eh_stream_printf((struct stream_base *)&shell->stream, "%.*s%.*s\x1B[%dD", 
            (int)diff, first_completion + linebuf_pos, n, linebuf + completion_len, n);
    }else{
        eh_stream_printf((struct stream_base *)&shell->stream, "%.*s", diff, first_completion + linebuf_pos);
    }
    shell->linebuf_pos += (uint16_t)diff;
    shell->linebuf_data_len += (uint16_t)diff;
        strncpy(linebuf + linebuf_pos, first_completion + linebuf_pos, diff);
}

static void ehshell_processor_input_ringbuf_redirect_init(ehshell_t *shell){
    ehshell_cmd_context_t *cmd_current;
    cmd_current = shell->cmd_current;
    if(eh_unlikely(cmd_current == NULL)){
        eh_mwarnfl(EHSHELL, "redirect input without command context");
        goto status_reset;
    }
    if(eh_unlikely(cmd_current->flags & EHSHELL_CMD_CONTEXT_FLAG_BACKGROUND)){
        eh_mwarnfl(EHSHELL, "redirect input to background command");
        goto status_reset;
    }
    if(eh_unlikely(!(cmd_current->command_info->flags & EHSHELL_COMMAND_REDIRECT_INPUT))){
        /* 本命令不支持重定向 */
        eh_mwarnfl(EHSHELL, "command %s not support redirect input", cmd_current->command_info->command);
        goto status_reset;
    }
    ehshell_input_reset(shell);
    shell->redirect_input_escape_parse_pos = shell->input_ringbuf->r;
status_reset:
    shell->state = EHSHELL_STATE_RESET;
    ehshell_notify_processor(shell);
}

static void ehshell_processor_input_ringbuf_redirect(ehshell_t *shell){
    const char *input_buf[2] = {NULL, NULL};
    size_t input_buf_len[2] = {0, 0};
    int32_t rl = 0, pl = 0, chars_count;
    eh_ringbuf_t peek_ringbuf;
    bool is_request_quit = false;
    peek_ringbuf = *shell->input_ringbuf;
    peek_ringbuf.r = shell->redirect_input_escape_parse_pos;
    chars_count = eh_ringbuf_size(&peek_ringbuf);
    if(chars_count == 0)
        return;
    rl = 0;
    input_buf[0] = (const char *)eh_ringbuf_peek(&peek_ringbuf, 0, NULL, &rl);
    input_buf_len[0] = (size_t)rl;
    rl = 0;
    input_buf[1] = (const char *)eh_ringbuf_peek(&peek_ringbuf, (int32_t)input_buf_len[0], NULL, &rl);
    input_buf_len[1] = (size_t)rl;
    for(size_t i = 0; i < 2; i++){
        for(size_t j = 0; j < input_buf_len[i]; j++){
            char input = input_buf[i][j];
            enum ehshell_escape_char escape_char;
            pl++;
            escape_char = ehshell_escape_char_parse(shell, input);
            if(escape_char == ESCAPE_CHAR_CTRL_C_SIGINT){
                is_request_quit = true;
                goto next;
            }
        }
    }
next:
    eh_ringbuf_read_skip(&peek_ringbuf, (int32_t)pl);
    shell->redirect_input_escape_parse_pos = peek_ringbuf.r;
    if(shell->cmd_current->command_info->do_event_function){
        shell->cmd_current->command_info->do_event_function(
            shell->cmd_current, EHSHELL_EVENT_RECEIVE_INPUT_DATA | (is_request_quit ? EHSHELL_EVENT_SIGINT_REQUEST_QUIT : 0));
    }
    if(is_request_quit){
        ehshell_notify_processor(shell);
    }
}

static void ehshell_processor_input_ringbuf(ehshell_t *shell){
    const char *input_buf[2] = {NULL, NULL};
    size_t input_buf_len[2] = {0, 0};
    int32_t rl,pl = 0, chars_count = 0;
    eh_ringbuf_t *tmp_ringbuf;
    char *linebuf = ehshell_linebuf(shell);
    eh_ringbuf_t peek_ringbuf;

    if(shell->cmd_current){
        /* 
        * 因为我们足够了解eh_ringbuf的实现，
        * 所以我们可以直接修改tmp_ringbuf的r指针，
        * 这样可以方便的读取回显位置之后的字符 
        */
        peek_ringbuf = *shell->input_ringbuf;
        peek_ringbuf.r = shell->echo_pos;
        tmp_ringbuf = &peek_ringbuf;
    }else{
        tmp_ringbuf = shell->input_ringbuf;
    }
    chars_count = eh_ringbuf_size(tmp_ringbuf);
    if(chars_count == 0)
        return;
    /* 
     *  因为我们是环形缓冲区，所以读两次，必然可以零拷贝并取出所需数据
     */
    rl = 0;
    input_buf[0] = (const char *)eh_ringbuf_peek(tmp_ringbuf, 0, NULL, &rl);
    input_buf_len[0] = (size_t)rl;
    rl = 0;
    input_buf[1] = (const char *)eh_ringbuf_peek(tmp_ringbuf, (int32_t)input_buf_len[0], NULL, &rl);
    input_buf_len[1] = (size_t)rl;
    for(size_t i = 0; i < 2; i++){
        for(size_t j = 0; j < input_buf_len[i]; j++){
            char input = input_buf[i][j];
            enum ehshell_escape_char escape_char;
            escape_char = ehshell_escape_char_parse(shell, input);
            pl++;
            if(isprint((int)escape_char)){
                int diff;
                if(shell->cmd_current){
                    /* 如果当前有命令在执行，就直接回显 */
                    eh_stream_putc((struct stream_base *)&shell->stream, (char)escape_char);
                    continue;
                }
                /* 判断命令行缓冲区是否有空间，如果没有空间就直接丢弃 */
                if(shell->linebuf_data_len >= (shell->config->input_linebuf_size - 1))
                    continue;
                eh_stream_putc((struct stream_base *)&shell->stream, (char)escape_char);
                diff = shell->linebuf_data_len - shell->linebuf_pos;
                /* 存储命令 */
                if(diff > 0){
                    /* 后移一位 */
                    memmove(linebuf + shell->linebuf_pos + 1, linebuf + shell->linebuf_pos, shell->linebuf_data_len - shell->linebuf_pos);
                    int diff = shell->linebuf_data_len - shell->linebuf_pos;
                    eh_stream_printf((struct stream_base *)&shell->stream, "%.*s\x1B[%dD", 
                        diff, linebuf + shell->linebuf_pos + 1, diff);
                }
                linebuf[shell->linebuf_pos] = (char)escape_char;
                shell->linebuf_pos++;
                shell->linebuf_data_len++;
                continue;
            }
            if(shell->cmd_current){
                if(escape_char == ESCAPE_CHAR_NUL)
                    continue;
                switch (escape_char) {
                    case ESCAPE_CHAR_CTRL_J_LF:
                    case ESCAPE_CHAR_CTRL_M_CR:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "\r\n");
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_BACKSPACE_0:
                    case ESCAPE_CHAR_CTRL_BACKSPACE_1:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "\b \b");
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_HOME:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "^[[H");
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_END:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "^[[F");
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_LEFT:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "^[[D");
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_RIGHT:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "^[[C");
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_UP:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "^[[A");
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_DOWN:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "^[[B");
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_DELETE:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "^[[3~");
                        continue;
                    }
                    default:
                        break;
                }
                /* ctrl+a -> ctrl+z*/
                if(escape_char >= ESCAPE_CHAR_CTRL_A && escape_char <= ESCAPE_CHAR_CTRL_Z){
                    eh_stream_putc((struct stream_base *)&shell->stream, '^');
                    eh_stream_putc((struct stream_base *)&shell->stream, (char)(escape_char - ESCAPE_CHAR_CTRL_A + 'A'));
                    if(escape_char == ESCAPE_CHAR_CTRL_C_SIGINT && pl == chars_count){
                        /* 发送SIGINT信号 */
                        if(shell->cmd_current->command_info->do_event_function){
                            shell->cmd_current->command_info->do_event_function(shell->cmd_current, EHSHELL_EVENT_SIGINT_REQUEST_QUIT);
                        }
                        goto status_refresh;
                    }
                    continue;
                }
            }else{
                switch (escape_char){
                    case ESCAPE_CHAR_NUL:
                        continue;
                    case ESCAPE_CHAR_CTRL_C_SIGINT:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "^C\r\n");
                        ehshell_input_reset(shell);
                        ehshell_print_prompt(shell);
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_BACKSPACE_0:
                    case ESCAPE_CHAR_CTRL_BACKSPACE_1:{
                        int diff;
                        if(shell->linebuf_pos == 0)
                            continue;
                        if(shell->linebuf_pos == shell->linebuf_data_len){
                            shell->linebuf_pos = --shell->linebuf_data_len;
                            eh_stream_puts((struct stream_base *)&shell->stream, "\b \b");
                            continue;
                        }
                        memmove(linebuf + shell->linebuf_pos -1, linebuf + shell->linebuf_pos, shell->linebuf_data_len - shell->linebuf_pos);
                        shell->linebuf_data_len--;
                        shell->linebuf_pos--;
                        diff = shell->linebuf_data_len - shell->linebuf_pos;
                        eh_stream_printf((struct stream_base *)&shell->stream, "\b%.*s \x1B[%dD", 
                            diff, linebuf + shell->linebuf_pos, diff+1);
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_TAB:{
                        /* tab建自动补全 */
                        ehshell_command_auto_complete(shell);
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_J_LF:
                    case ESCAPE_CHAR_CTRL_M_CR:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "\r\n");
                        linebuf[shell->linebuf_data_len] = '\0';
                        if(shell->linebuf_data_len && _ehshell_command_run_form_string(shell, linebuf) == 0){
                            /* 运行命令,由于一些状态信息更新，需要重新进处理 */
                            goto status_refresh;
                        }
                        ehshell_input_reset(shell);
                        ehshell_print_prompt(shell);
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_L_CLS:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "\x1B[2J\x1B[H");
                        ehshell_print_prompt(shell);
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_U_DEL_LINE:
                    case ESCAPE_CHAR_CTRL_RESET:{
                        eh_stream_puts((struct stream_base *)&shell->stream, "\x0e\r");
                        ehshell_input_reset(shell);
                        ehshell_print_prompt(shell);
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_HOME:{
                        if(shell->linebuf_pos){
                            eh_stream_printf((struct stream_base *)&shell->stream, "\x1B[%dD", shell->linebuf_pos);
                            shell->linebuf_pos = 0;
                        }
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_END:{
                        if(shell->linebuf_data_len - shell->linebuf_pos){
                            eh_stream_printf((struct stream_base *)&shell->stream, "\x1B[%dC", shell->linebuf_data_len - shell->linebuf_pos);
                            shell->linebuf_pos = shell->linebuf_data_len;
                        }
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_LEFT:{
                        if(shell->linebuf_pos){
                            eh_stream_puts((struct stream_base *)&shell->stream, "\x1B[D");
                            shell->linebuf_pos--;
                        }
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_RIGHT:{
                        if(shell->linebuf_pos < shell->linebuf_data_len){
                            eh_stream_puts((struct stream_base *)&shell->stream, "\x1B[C");
                            shell->linebuf_pos++;
                        }
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_UP:{
                        /* TODO */
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_DOWN:{
                        /* TODO */
                        continue;
                    }
                    case ESCAPE_CHAR_CTRL_DELETE:{
                        if(shell->linebuf_pos < shell->linebuf_data_len){
                            int diff;
                            memmove(linebuf + shell->linebuf_pos, linebuf + shell->linebuf_pos + 1, (size_t)(shell->linebuf_data_len - shell->linebuf_pos - 1));
                            shell->linebuf_data_len--;
                            diff = shell->linebuf_data_len - shell->linebuf_pos;
                            eh_stream_printf((struct stream_base *)&shell->stream, "%.*s \x1B[%dD", 
                                diff, linebuf + shell->linebuf_pos, diff+1);
                        }
                        continue;
                    }
                    default:{
                        continue;
                    }
                }
            }
        }
    }
status_refresh:
    ehshell_notify_processor(shell);
// quit:
    eh_stream_finish((struct stream_base *)&shell->stream);
    eh_ringbuf_read_skip(tmp_ringbuf, pl);
    if(shell->cmd_current){
        shell->echo_pos = tmp_ringbuf->r;
    }
    return ;
}

static void ehshell_processor(eh_event_t *e, void *slot_param){
    (void)e;
    ehshell_t *shell = (ehshell_t *)slot_param;
    switch (shell->state) {
        case EHSHELL_INIT:
            ehshell_print_welcome(shell);
            _fallthrough;
        case EHSHELL_STATE_RESET:
            ehshell_input_reset(shell);
            ehshell_print_prompt(shell);
            eh_stream_finish((struct stream_base *)&shell->stream);
            shell->state = EHSHELL_STATE_WAIT_INPUT;
            _fallthrough;
        case EHSHELL_STATE_WAIT_INPUT:
            ehshell_processor_input_ringbuf(shell);
            break;
        case EHSHELL_STATE_REDIRECT_INPUT_INIT:
            ehshell_processor_input_ringbuf_redirect_init(shell);
            shell->state = EHSHELL_STATE_REDIRECT_INPUT;
            _fallthrough;
        case EHSHELL_STATE_REDIRECT_INPUT:
            ehshell_processor_input_ringbuf_redirect(shell);
            break;
    }
}

static ehshell_cmd_context_t *ehshell_cmd_context_create(ehshell_t *ehshell, const struct ehshell_command_info *command_info, bool is_background){
    ehshell_cmd_context_t *ctx;
    int idx = -1;
    if(is_background){
        for(int i = 0; i < EHSHELL_CONFIG_MAX_BACKGROUND_COMMAND_SIZE; i++){
            if(!ehshell->cmd_background[i]){
                idx = i;
                break;
            }
        }
        if(idx < 0){
            eh_stream_printf((struct stream_base *)&ehshell->stream, "ehshell: background command overflow %d, command %s\r\n", EHSHELL_CONFIG_MAX_BACKGROUND_COMMAND_SIZE, command_info->command);
            eh_stream_finish((struct stream_base *)&ehshell->stream);
            return eh_error_to_ptr(EH_RET_INVALID_PARAM);
        }
    }else{
        if(ehshell->cmd_current){
            eh_stream_printf((struct stream_base *)&ehshell->stream, "ehshell: current command %s is running, command %s\r\n", ehshell->cmd_current->command_info->command, command_info->command);
            eh_stream_finish((struct stream_base *)&ehshell->stream);
            return eh_error_to_ptr(EH_RET_INVALID_PARAM);
        }
    }
    ctx = eh_malloc(sizeof(ehshell_cmd_context_t));
    if(!ctx)
        return eh_error_to_ptr(EH_RET_MALLOC_ERROR);
    ctx->ehshell = ehshell;
    ctx->command_info = command_info;
    ctx->user_data = NULL;
    ctx->flags = is_background ? EHSHELL_CMD_CONTEXT_FLAG_BACKGROUND : 0;
    if(is_background){
        ehshell->cmd_background[idx] = ctx;
        ehshell->state = EHSHELL_STATE_RESET;
    }else{
        ehshell->cmd_current = ctx;
        if(command_info->flags & EHSHELL_COMMAND_REDIRECT_INPUT)
            ehshell->state = EHSHELL_STATE_REDIRECT_INPUT_INIT;
    }
    return ctx;
}

const struct ehshell_command_info* ehshell_command_find(ehshell_t *ehshell, const char *command){
    const struct ehshell_command_info** command_info_tab = ehshell_command_info_tab(ehshell);
    size_t start_pos = 0;
    size_t end_pos;
    size_t pos;
    int cmp = -1;
    if( ehshell == NULL || command == NULL ){
        return NULL;
    }
    end_pos = ehshell->command_count;
    pos = (start_pos + end_pos) / 2;
    while(start_pos < end_pos){
        cmp = strcmp(command, command_info_tab[pos]->command);
        if(cmp == 0){
            /* 找到命令 */
            break;
        }else if(cmp < 0){
            end_pos = pos;
        }else{
            start_pos = pos + 1;
        }
        pos = (start_pos + end_pos) / 2;
    }
    if(cmp != 0){
        return NULL;
    }
    return command_info_tab[pos];
}

int ehshell_command_run(ehshell_t *ehshell, int argc, const char *argv[]){
    const struct ehshell_command_info* command_info;
    bool is_background = false;
    if(argc < 1 || !argv[0]){
        eh_stream_printf((struct stream_base *)&ehshell->stream, "ehshell: command argc overflow %d, command %s\r\n", argc, argv[0]);
        eh_stream_finish((struct stream_base *)&ehshell->stream);
        return EH_RET_INVALID_PARAM;
    }
    command_info = ehshell_command_find(ehshell, argv[0]);
    if(!command_info){
        eh_stream_printf((struct stream_base *)&ehshell->stream, "ehshell: command not found: %s\r\n", argv[0]);
        eh_stream_finish((struct stream_base *)&ehshell->stream);
        return EH_RET_NOT_EXISTS;
    }

    if(argc > 1 && strcmp(argv[argc - 1], "&") == 0){
        is_background = true;
        argc--;
    }

    ehshell_cmd_context_t *ctx = ehshell_cmd_context_create(ehshell, command_info, is_background);
    if(eh_ptr_to_error(ctx) < 0){
        eh_stream_printf((struct stream_base *)&ehshell->stream, "ehshell: command context create failed %d, command %s\r\n", eh_ptr_to_error(ctx), argv[0]);
        eh_stream_finish((struct stream_base *)&ehshell->stream);
        return eh_ptr_to_error(ctx);
    }
    ctx->command_info->do_function(ctx, argc, argv);
    return 0;
}


void ehshell_command_set_user_data(ehshell_cmd_context_t *cmd_context, void *user_data){
    cmd_context->user_data = user_data;
}


void *ehshell_command_get_user_data(ehshell_cmd_context_t *cmd_context){
    return cmd_context->user_data;
}


void ehshell_command_finish(ehshell_cmd_context_t *cmd_context){
    ehshell_t *ehshell = cmd_context->ehshell;
    if(ehshell){
        if(cmd_context->flags & EHSHELL_CMD_CONTEXT_FLAG_BACKGROUND){
            for(int i = 0; i < EHSHELL_CONFIG_MAX_BACKGROUND_COMMAND_SIZE; i++){
                if(ehshell->cmd_background[i] == cmd_context){
                    ehshell->cmd_background[i] = NULL;
                    break;
                }
            }
        }else{
            if(ehshell->cmd_current == cmd_context){
                ehshell->cmd_current = NULL;
                ehshell->state = EHSHELL_STATE_RESET;
                ehshell_notify_processor(ehshell);
            }else{
                eh_mwarnfl( EHSHELL,"ehshell: command context not found %p, command %s", cmd_context, cmd_context->command_info->command);
            }
        }
    }
    eh_free(cmd_context);
}


int ehshell_command_run_form_string(ehshell_t *ehshell, const char *cmd_str){
    char *linebuf = eh_malloc(strlen(cmd_str) + 1);
    int ret;
    if(!linebuf)
        return EH_RET_MALLOC_ERROR;
    strcpy(linebuf, cmd_str);
    ret = _ehshell_command_run_form_string(ehshell, linebuf);
    eh_free(linebuf);
    return ret;
}

ehshell_t *ehshell_create(const struct ehshell_config *static_config){
    int ret;
    ehshell_t *shell;
    if(!static_config || !static_config->input_linebuf_size  || !static_config->write)
        return eh_error_to_ptr(EH_RET_INVALID_PARAM);
    if(static_config->input_ringbuf_size < sizeof(uint32_t) * 2){
        eh_merrfl( EHSHELL,"input_ringbuf_size %d is too small, sizeof(uint32_t) * 2 %d", static_config->input_ringbuf_size, sizeof(uint32_t) * 2);
        return eh_error_to_ptr(EH_RET_INVALID_PARAM);
    }
    if(static_config->max_command_count < ehshell_builtin_commands_count()){
        eh_merrfl( EHSHELL,"max_command_size %d is too small, builtin commands count %d", static_config->max_command_count, ehshell_builtin_commands_count());
        return eh_error_to_ptr(EH_RET_INVALID_PARAM);
    }
    
    shell = eh_malloc(sizeof(ehshell_t) + static_config->max_command_count * sizeof(struct ehshell_command_info*) + static_config->input_linebuf_size);
    if(!shell)
        return eh_error_to_ptr(EH_RET_MALLOC_ERROR);
    bzero(shell, sizeof(ehshell_t));
    shell->config = static_config;
    shell->input_ringbuf = eh_ringbuf_create(static_config->input_ringbuf_size, NULL);
    ret = eh_ptr_to_error(shell->input_ringbuf);
    if(ret < 0){
        goto err_input_ringbuf_create;
    }
    shell->command_count = 0;
    shell->linebuf_pos = 0;
    shell->linebuf_data_len = 0;
    shell->state = EHSHELL_INIT;

    eh_stream_function_no_cache_init(&shell->stream, ehshell_stream_write, ehshell_stream_finish);

    ret = ehshell_register_builtin_commands(shell);
    if(ret < 0){
        goto err_ehshell_register_builtin_commands;
    }

    eh_signal_init(&shell->sig_notify_process);
    eh_signal_slot_init(&shell->sig_notify_process_slot, ehshell_processor, shell);

    ret = eh_signal_slot_connect(&shell->sig_notify_process, &shell->sig_notify_process_slot);
    if(ret < 0){
        goto err_eh_signal_slot_connect;
    }
    ehshell_notify_processor(shell);
    if(!ehshell_default_shell)
        ehshell_default_shell = shell;
    return shell;
err_eh_signal_slot_connect:
err_ehshell_register_builtin_commands:
    eh_ringbuf_destroy(shell->input_ringbuf);
err_input_ringbuf_create:
    eh_free(shell);
    return (ehshell_t *)eh_error_to_ptr(ret);
}

void ehshell_destroy(ehshell_t *ehshell){
    if(!ehshell)
        return;
    if(ehshell_default_shell == ehshell)
        ehshell_default_shell = NULL;
    for(size_t i = 0; i < EHSHELL_CONFIG_MAX_BACKGROUND_COMMAND_SIZE; i++){
        if(ehshell->cmd_background[i]){
            if(ehshell->cmd_background[i]->command_info->do_event_function){
                ehshell->cmd_background[i]->command_info->do_event_function(ehshell->cmd_background[i], EHSHELL_EVENT_SHELL_EXIT);
            }
            ehshell->cmd_background[i]->ehshell = NULL;
        }
    }
    if(ehshell->cmd_current){
        if(ehshell->cmd_current->command_info->do_event_function){
            ehshell->cmd_current->command_info->do_event_function(ehshell->cmd_current, EHSHELL_EVENT_SHELL_EXIT);
        }
        ehshell->cmd_current->ehshell = NULL;
    }
    eh_signal_slot_disconnect(&ehshell->sig_notify_process, &ehshell->sig_notify_process_slot);
    eh_ringbuf_destroy(ehshell->input_ringbuf);
    eh_free(ehshell);
}


eh_ringbuf_t* ehshell_input_ringbuf(ehshell_t *ehshell){
    if(!ehshell)
        return NULL;
    return ehshell->input_ringbuf;
}


void ehshell_notify_processor(ehshell_t *ehshell){
    eh_signal_notify(&ehshell->sig_notify_process);
}

struct stream_base *ehshell_command_stream(ehshell_cmd_context_t *cmd_context){
    if(cmd_context == NULL || cmd_context->ehshell == NULL)
        return NULL;
    return (struct stream_base *)&cmd_context->ehshell->stream;
}


const char *ehshell_command_usage(ehshell_cmd_context_t *cmd_context){
    if(!cmd_context || !cmd_context->command_info)
        return NULL;
    return cmd_context->command_info->usage;
}


eh_ringbuf_t* ehshell_command_input_ringbuf(ehshell_cmd_context_t *cmd_context, int32_t *readable_size){
    eh_ringbuf_t tmp_ringbuf;
    ehshell_t *ehshell = cmd_context->ehshell;
    if( !cmd_context || 
        !(cmd_context->command_info->flags & EHSHELL_COMMAND_REDIRECT_INPUT) ||
        !ehshell ||
        ehshell->state != EHSHELL_STATE_REDIRECT_INPUT ||
        ehshell->cmd_current != cmd_context)
        return NULL;
    tmp_ringbuf = *ehshell->input_ringbuf;
    tmp_ringbuf.w = ehshell->redirect_input_escape_parse_pos;
    *readable_size = eh_ringbuf_size(&tmp_ringbuf);
    return ehshell->input_ringbuf;
}


ehshell_t* ehshell_command_get_shell(ehshell_cmd_context_t *cmd_context){
    if(!cmd_context)
        return NULL;
    return cmd_context->ehshell;
}

ehshell_t *ehshell_default(void){
    return ehshell_default_shell;
}

int ehshell_register_commands(ehshell_t *ehshell, const struct ehshell_command_info *command_info, size_t command_info_num){
    const struct ehshell_command_info **commands;
    if(!ehshell || !command_info || command_info_num == 0)
        return EH_RET_INVALID_PARAM;
    if(ehshell->command_count + command_info_num > ehshell->config->max_command_count){
        eh_merrfl( EHSHELL,"command_info_num %d is too large, max_command_size %d", command_info_num, ehshell->config->max_command_count);
        return EH_RET_INVALID_PARAM;
    }
    commands = ehshell_command_info_tab(ehshell);
    /* 循环插入有序数组中 */
    for(size_t i = 0; i < command_info_num; i++){
        size_t j;
        for(j = 0; j < ehshell->command_count; j++){
            if(strcmp(command_info[i].command, commands[j]->command) < 0){
                break;
            }
        }
        memmove(commands + j + 1, commands + j, (ehshell->command_count - j) * sizeof(struct ehshell_command_info*));
        commands[j] = &command_info[i];
        ehshell->command_count++;
    }
    return EH_RET_OK;
}


