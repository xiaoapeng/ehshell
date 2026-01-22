/**
 * @file ehshell_builtin_commands.c
 * @brief 内建命令实现
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-11-09
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */

#include <stdint.h>
#include <string.h>
#include <eh_error.h>
#include <eh_formatio.h>
#include <ehshell.h>
#include <ehshell_module.h>
#include <ehshell_internal.h>
#include <eh_ringbuf.h>

static void do_help(ehshell_cmd_context_t *cmd_context, int argc, const char *argv[]){
    size_t command_count;
    if(argc > 2){
        /* 参数太多 */
        eh_stream_printf(ehshell_command_stream(cmd_context), "Parameter too many.\r\n");
        goto quit;
    }
    if(argc == 2){
        const struct ehshell_command_info* command_info = ehshell_command_find(ehshell_command_get_shell(cmd_context), argv[1]);
        if(command_info == NULL){
            eh_stream_printf(ehshell_command_stream(cmd_context), "Command %s not found.\r\n", argv[1]);
            goto quit;
        }
        eh_stream_printf(ehshell_command_stream(cmd_context), "%s:\t%s\r\n", command_info->command, command_info->description);
        eh_stream_printf(ehshell_command_stream(cmd_context), "\t%s\r\n", command_info->usage);
        goto quit;
    }
    /* 打印所有命令 */
    command_count = ehshell_commands_count();
    for(size_t i = 0; i < command_count; i++){
        eh_stream_printf(ehshell_command_stream(cmd_context), "%16s:\t\t\t%s\r\n", ehshell_command_get(i)->command, ehshell_command_get(i)->description);
    }
quit:
    eh_stream_finish(ehshell_command_stream(cmd_context));
    ehshell_command_finish(cmd_context);
}

static void do_exit_mainloop(ehshell_cmd_context_t *cmd_context, int argc, const char *argv[]){
    (void)argc;
    (void)argv;
    eh_signal_dispatch_loop_request_quit_from_task(eh_task_main());
    ehshell_command_finish(cmd_context);
}

static void do_quit(ehshell_cmd_context_t *cmd_context, int argc, const char *argv[]){
    (void)argc;
    (void)argv;
    eh_stream_printf(ehshell_command_stream(cmd_context), "Quit the current terminal session.\r\n\x03");
    eh_stream_finish(ehshell_command_stream(cmd_context));
    ehshell_command_finish(cmd_context);
}




#ifdef CONFIG_PACKAGE_EHSHELL_USE_PASSWORD
#define  EHSHELL_LOGIN_FNV1A_64_HASH_INIT 14695981039346656037ULL
static uint64_t s_password_hash = CONFIG_PACKAGE_EHSHELL_PASSWORD_HASH;

static void login_fnv1a_64_hash_char(uint64_t *hash, uint8_t ch);

int ehshell_set_password(const char *password){
    s_password_hash = EHSHELL_LOGIN_FNV1A_64_HASH_INIT;
    while(*password != '\0')
        login_fnv1a_64_hash_char(&s_password_hash, *password++);
    return 0;
}

static void login_fnv1a_64_hash_char(uint64_t *hash, uint8_t ch){
    *hash ^= ch;
    *hash *= 1099511628211ULL;
}


#define EHSHELL_LOGIN_FLAG_LAST_CHAR_LT     (1U << 16) /* 登录时上一个字符是否为\n */
#define EHSHELL_LOGIN_FLAG_LAST_CHAR_CR     (1U << 17) /* 登录时上一个字符是否为\r */

static void do_login(ehshell_cmd_context_t *cmd_context, int argc, const char *argv[]){
    (void)argc;
    (void)argv;
    ehshell_t* shell = ehshell_command_get_shell(cmd_context);
    struct stream_base * stream = ehshell_command_stream(cmd_context);
    if(cmd_context->flags & (EHSHELL_CMD_CONTEXT_FLAG_BACKGROUND)){
        ehshell_command_finish(cmd_context);
        return;
    }
    if(CONFIG_PACKAGE_EHSHELL_PASSWORD[0] == '\0'){
        eh_stream_printf(stream, "Login to the system.\r\n");
        eh_stream_finish(stream);
        ehshell_command_finish(cmd_context);
        return;
    }
    eh_stream_printf(stream, "Password: ");
    eh_stream_finish(stream);
    shell->login_hash = EHSHELL_LOGIN_FNV1A_64_HASH_INIT;
    cmd_context->flags &= (~(EHSHELL_LOGIN_FLAG_LAST_CHAR_LT | EHSHELL_LOGIN_FLAG_LAST_CHAR_CR));
}

static void do_login_event(ehshell_cmd_context_t *cmd_context, enum ehshell_event ehshell_event){
    int32_t data_len;
    struct stream_base * stream = ehshell_command_stream(cmd_context);
    ehshell_t* shell = ehshell_command_get_shell(cmd_context);
    eh_ringbuf_t* ringbuf;
    char ch;
    switch (ehshell_event) {
        case EHSHELL_EVENT_SHELL_EXIT:
            ehshell_command_finish(cmd_context);
            return;
        case EHSHELL_EVENT_SIGINT_REQUEST_QUIT:
            return;
        default:
            break;
    }
    ringbuf = ehshell_command_input_ringbuf(cmd_context, &data_len);
    if(data_len <= 0)
        return;
    /* 
     * 输入密码必然是性能不敏感的时候吧，得学会偷点懒
     */
    for(int32_t i = 0; i < data_len; i++){
        eh_ringbuf_read(ringbuf, (uint8_t*)&ch, 1);
        if(ch == '\r' || ch == '\n'){
            if(ch == '\r' && cmd_context->flags & EHSHELL_LOGIN_FLAG_LAST_CHAR_LT){
                cmd_context->flags &= (uint16_t)(~EHSHELL_LOGIN_FLAG_LAST_CHAR_LT);
                continue;
            }
            if(ch == '\n' && cmd_context->flags & EHSHELL_LOGIN_FLAG_LAST_CHAR_CR){
                cmd_context->flags &= (uint16_t)(~EHSHELL_LOGIN_FLAG_LAST_CHAR_CR);
                continue;
            }
            cmd_context->flags = (uint16_t)(cmd_context->flags | ((ch == '\r') ? 
                (uint16_t)EHSHELL_LOGIN_FLAG_LAST_CHAR_CR : (uint16_t)EHSHELL_LOGIN_FLAG_LAST_CHAR_LT));

            /* 处理回车，进行password校验 */
            if(shell->login_hash != s_password_hash){
                /* 密码错误 */
                goto password_error;
            }
            /* 密码正确 */
            eh_stream_printf(stream, "\r\nLogin success.\r\n");
            eh_stream_finish(stream);
            ehshell_command_finish(cmd_context);
            return;
        }else if(ch == '\x3'){
            eh_stream_printf(stream, "^C\r\n");
            eh_stream_printf(stream, "Password: ");
            shell->login_hash = EHSHELL_LOGIN_FNV1A_64_HASH_INIT;
        }else{
            login_fnv1a_64_hash_char(&shell->login_hash, ch);
            eh_stream_printf(stream, "*");
        }
        continue;
    password_error:
        /* 密码错误 */
        eh_stream_printf(stream, "\r\nPassword error.\r\n");
        eh_stream_printf(stream, "Password: ");
        shell->login_hash = EHSHELL_LOGIN_FNV1A_64_HASH_INIT;
    }
    eh_stream_finish(stream);
}

#else
int ehshell_set_password(const char *password){
    (void)password;
    return EH_RET_NOT_SUPPORTED;
}
#endif

static struct ehshell_command_info ehshell_command_info_tbl[] = {
    {
        .command = "help",
        .description = "Show help information.",
        .usage = "help [command]",
        .flags = 0,
        .do_function = do_help,
        .do_event_function = NULL
    },{
        .command = "exit-mainloop",
        .description = "Exit the eventhub os.",
        .usage = "exit-mainloop",
        .flags = 0,
        .do_function = do_exit_mainloop,
        .do_event_function = NULL
    },{
        .command = "quit",
        .description = "Exit the current terminal session.",
        .usage = "quit",
        .flags = 0,
        .do_function = do_quit,
        .do_event_function = NULL
    },
    
#ifdef CONFIG_PACKAGE_EHSHELL_USE_PASSWORD
    {
        .command = "login",
        .description = "Login to the system.",
        .usage = "login",
        .flags = EHSHELL_COMMAND_REDIRECT_INPUT,
        .do_function = do_login,
        .do_event_function = do_login_event,
    },
#endif

};

eh_static_assert(EH_ARRAY_SIZE(ehshell_command_info_tbl) < CONFIG_PACKAGE_EHSHELL_MAX_COMMAND_SIZE, "ehshell_command_info_tbl size exceeds CONFIG_PACKAGE_EHSHELL_MAX_COMMAND_SIZE");

static int __init  builtin_commands_register_init(void){
    return ehshell_register_commands( ehshell_command_info_tbl, EH_ARRAY_SIZE(ehshell_command_info_tbl));
}
ehshell_module_command_export(builtin_commands_register_init, NULL);

