/**
 * @file ehshell_config.h
 * @brief ehshell config
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-11-09
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */
#ifndef _EHSHELL_CONFIG_H_
#define _EHSHELL_CONFIG_H_

#include <eh_debug.h>

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */


/**
 * @brief 最大后台运行的命令数量
 */
#ifndef EHSHELL_CONFIG_MAX_BACKGROUND_COMMAND_SIZE
#define EHSHELL_CONFIG_MAX_BACKGROUND_COMMAND_SIZE (4)
#endif

#ifndef EHSHELL_CONFIG_WELCOME
#define EHSHELL_CONFIG_WELCOME \
    "====================================\r\n" \
    "+ ███████╗██╗  ██╗███████╗██╗  ██╗ +\r\n" \
    "+ ██╔════╝██║  ██║██╔════╝██║  ██║ +\r\n" \
    "+ █████╗  ███████║███████╗███████║ +\r\n" \
    "+ ██╔══╝  ██╔══██║╚════██║██╔══██║ +\r\n" \
    "+ ███████╗██║  ██║███████║██║  ██║ +\r\n" \
    "+ ╚══════╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝ +\r\n" \
    "+    e h s h e l l   r e a d y.    +\r\n" \
    "====================================\r\n"
#endif

#ifndef EHSHELL_CONFIG_ARGC_MAX
#define EHSHELL_CONFIG_ARGC_MAX                    (8)
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif // _EHSHELL_CONFIG_H_