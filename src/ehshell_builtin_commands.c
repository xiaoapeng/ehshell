/**
 * @file ehshell_builtin_commands.c
 * @brief 内建命令实现
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-11-09
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */

#include <eh_error.h>
#include <eh_formatio.h>
#include <ehshell.h>
#include <ehshell_internal.h>

static void do_help(ehshell_cmd_context_t *cmd_context, int argc, const char *argv[]){
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
    const struct ehshell_command_info** command_info_tab = ehshell_command_info_tab(ehshell_command_get_shell(cmd_context));
    for(size_t i = 0; i < ehshell_command_get_shell(cmd_context)->command_count; i++){
        eh_stream_printf(ehshell_command_stream(cmd_context), "%16s:\t\t\t%s\r\n", command_info_tab[i]->command, command_info_tab[i]->description);
    }
quit:
    ehshell_command_finish(cmd_context);
}

static void do_exit_mainloop(ehshell_cmd_context_t *cmd_context, int argc, const char *argv[]){
    (void)argc;
    (void)argv;
    eh_signal_dispatch_loop_request_quit_from_task(eh_task_main());
    ehshell_command_finish(cmd_context);
}

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
    }
};

size_t ehshell_builtin_commands_count(void){
    return EH_ARRAY_SIZE(ehshell_command_info_tbl);
}

int ehshell_register_builtin_commands(ehshell_t* ehshell){
    int ret;
    ret = ehshell_register_commands(ehshell, ehshell_command_info_tbl, EH_ARRAY_SIZE(ehshell_command_info_tbl));
    if(ret < 0)
        return ret;
    return 0;
}



