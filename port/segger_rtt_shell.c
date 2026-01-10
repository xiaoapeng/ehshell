/**
 * @file rtt_shell_io.c
 * @brief 实现shell io
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2026-01-05
 * 
 * @copyright Copyright (c) 2026  simon.xiaoapeng@gmail.com
 * 
 */

#include <eh.h>
#include <eh_ringbuf.h>
#include <eh_module.h>
#include <eh_debug.h>
#include <eh_platform.h>
#include <ehshell.h>
#include <ehshell_module.h>
#include <stdint.h>
#include <sys/_intsup.h>

#include <SEGGER_RTT.h>

#include <autoconf.h>

static ehshell_t *s_shell = NULL;

static void rtt_shell_write(ehshell_t* ehshell, const char *buf, size_t len){
    (void) ehshell;
    SEGGER_RTT_Write(CONFIG_PACKAGE_EHSHELL_BUILTIN_SEGGER_RTT_UP_CHANNEL_NUMBER, buf, len);
}

static void rtt_shell_read_poll_task(void* arg){
    (void)arg;
    unsigned int rl;
    uint8_t rtt_buf[16];
    eh_ringbuf_t* input_ringbuf = ehshell_input_ringbuf(s_shell);
    int32_t free_size = eh_ringbuf_free_size(input_ringbuf);
    if(free_size <= 0)
        return ;
    rl = SEGGER_RTT_HasData(CONFIG_PACKAGE_EHSHELL_BUILTIN_SEGGER_RTT_DOWN_CHANNEL_NUMBER);
    if(rl <= 0)
        return ;
    free_size = free_size > (int32_t)sizeof(rtt_buf) ? (int32_t)sizeof(rtt_buf) : free_size;
    rl = SEGGER_RTT_ReadNoLock(CONFIG_PACKAGE_EHSHELL_BUILTIN_SEGGER_RTT_DOWN_CHANNEL_NUMBER, rtt_buf, (unsigned)free_size);
    if(rl <= 0)
        return ;
    eh_ringbuf_write(input_ringbuf, rtt_buf, (int32_t)rl);
    ehshell_notify_processor(s_shell);
}

static const struct ehshell_config shell_config = {
    .host = "eventos-rtt",
    .input_linebuf_size = CONFIG_PACKAGE_EHSHELL_BUILTIN_SHELL_LINE_BUFFER_SIZE,
    .input_ringbuf_size = CONFIG_PACKAGE_EHSHELL_BUILTIN_SHELL_INPUT_BUFFER_SIZE,
    .stream_write = rtt_shell_write,
    .stream_finish = NULL,
    .input_ringbuf_process_finish = NULL,
    .max_command_count = 10,
};

static eh_loop_poll_task_t s_shell_read_char_poll_task = {
    .poll_task = rtt_shell_read_poll_task,
    .arg = NULL,
    .list_node = EH_LIST_HEAD_INIT(s_shell_read_char_poll_task.list_node)
};

int __init rtt_shell_io_init(void){
    eh_loop_poll_task_add(&s_shell_read_char_poll_task);
    s_shell = ehshell_create(&shell_config);
    if(eh_ptr_to_error(s_shell) < 0){
        eh_merrfl(RTT_SHELL_IO, "ehshell_create failed %d", eh_ptr_to_error(s_shell));
        eh_loop_poll_task_del(&s_shell_read_char_poll_task);
        return -1;
    }
    return 0;
}

void __exit rtt_shell_io_exit(void){
    ehshell_destroy(s_shell);
    eh_loop_poll_task_del(&s_shell_read_char_poll_task);
}

ehshell_module_default_shell_export(rtt_shell_io_init, rtt_shell_io_exit);