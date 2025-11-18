/**
 * @file ehshell_internal.h
 * @brief 内部接口
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-11-09
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */

#ifndef _EHSHELL_INTERNAL_H_
#define _EHSHELL_INTERNAL_H_

#include <stdint.h>
#include <eh.h>
#include <eh_signal.h>
#include <eh_formatio.h>
#include "ehshell_config.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

typedef struct eh_ringbuf eh_ringbuf_t;
struct ehshell_config ;
struct ehshell_command_info;
typedef struct ehshell ehshell_t;
enum ehshell_escape_char;

enum ehshell_state{
    EHSHELL_INIT = 0,
    EHSHELL_STATE_RESET,
    EHSHELL_STATE_WAIT_INPUT,
};


typedef struct ehshell_cmd_context{
    void                                *user_data;
    const struct ehshell_command_info   *command_info;
    ehshell_t                           *ehshell;
#define EHSHELL_CMD_CONTEXT_FLAG_BACKGROUND     (1 << 0)
    uint32_t                             flags;
}ehshell_cmd_context_t;

struct ehshell{
    const struct ehshell_config *config;
    eh_ringbuf_t *input_ringbuf;
    ehshell_cmd_context_t *cmd_background[EHSHELL_CONFIG_MAX_BACKGROUND_COMMAND_SIZE];
    ehshell_cmd_context_t *cmd_current;
    struct stream_function_no_cache stream;
    eh_signal_base_t    sig_notify_process;
    eh_signal_slot_t    sig_notify_process_slot;
    enum ehshell_state state;
    union{
        struct{
            uint16_t  linebuf_pos;
            uint16_t  linebuf_data_len;
        };
        uint32_t echo_pos;
    };
    uint16_t  command_count;
    uint16_t  escape_char_match_state;
#define EHSHELL_ESCAPE_CHAR_PARSE_BUF_SIZE 4
    char      escape_char_parse_buf[EHSHELL_ESCAPE_CHAR_PARSE_BUF_SIZE];
};



#define ehshell_command_info_tab(ehshell) ((const struct ehshell_command_info**)(ehshell + 1))

#define ehshell_linebuf(ehshell) ((char*)(ehshell + 1) + sizeof(struct ehshell_command_info*) * ehshell->config->max_command_count)

extern int ehshell_register_builtin_commands(ehshell_t* ehshell);

extern size_t ehshell_builtin_commands_count(void);

extern enum ehshell_escape_char ehshell_escape_char_parse(struct ehshell* shell, const char input);

const struct ehshell_command_info* ehshell_command_find(ehshell_t *ehshell, const char *command);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif // _EHSHELL_INTERNAL_H_