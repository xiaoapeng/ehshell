/**
 * @file ehshell_module.h
 * @brief 定义ehshell模块的接口
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-11-27
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */
#ifndef _EHSHELL_MODULE_H_
#define _EHSHELL_MODULE_H_

#include <eh_module.h>

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#define ehshell_module_default_shell_export(_init__func_, _exit__func_)     _eh_define_module_export(_init__func_, _exit__func_, "3.0.0")
#define ehshell_module_default_command_export(_init__func_, _exit__func_)     _eh_define_module_export(_init__func_, _exit__func_, "3.0.1")

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif // _EHSHELL_MODULE_H_