/**
 * @file ehshell_escape_char.c
 * @brief 处理转义字符
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-11-13
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */
#include <string.h>
#include <eh.h>
#include <eh_module.h>
#include <eh_debug.h>
#include <ehshell.h>
#include <ehshell_internal.h>
#include <ehshell_escape_char.h>

static inline int is_csi_final(uint8_t c){ return c >= 0x40 && c <= 0x7E; }
static inline int is_middle_byte(uint8_t c){ return c >= 0x20 && c <= 0x2F; }
static inline int is_param_byte(uint8_t c){ return (c >= '0' && c <= '9') || c == ';' || c == '?' || c == '>' || c == '<'; }

#define EHSHELL_ESCAPE_CHAR_MAX_LEN  (64U)

/**
 * @brief                   尝试匹配转义字符
 * @param  shell            shell实例
 * @param  input            输入字符
 * @return uint32_t 
 */
enum ehshell_escape_char ehshell_escape_char_parse(struct ehshell* shell, const char input){
    switch (shell->escape_char_match_state){
        case EHSHELL_ESCAPE_MATCH_NONE:{
            if(input == 0x1B){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_ESC;
                shell->escape_char_parse_buf[0] = '\0';
                break;
            }
            return (enum ehshell_escape_char)input;
        }
        case EHSHELL_ESCAPE_MATCH_ESC:{
            if(input == '['){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_ESC_CSI;
                break;
            }else if(input == ']'){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_ESC_OSC;
                break;
            }else if(input == 'P'){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_ESC_DCS;
                break;
            }else if(input == '^'){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_ESC_PM;
                break;
            }else if(input == '_'){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_ESC_APC;
                break;
            }else if(input == 'O'){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_ESC_SS3;
                break;
            }
            /* 双字节转义字符，目前没有我们想用的，直接忽略返回 */
            shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_NONE;
            break;
        }
        case EHSHELL_ESCAPE_MATCH_ESC_OSC:
        case EHSHELL_ESCAPE_MATCH_ESC_DCS:
        case EHSHELL_ESCAPE_MATCH_ESC_PM :
        case EHSHELL_ESCAPE_MATCH_ESC_APC:{
            if(input == 0x07){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_NONE;
                break;
            }else if(input == 0x1B){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_ESC_STRING_WAIT_ST;
                break;
            }
            shell->escape_char_parse_buf[0]++;
            if( (uint32_t)shell->escape_char_parse_buf[0] >= EHSHELL_ESCAPE_CHAR_MAX_LEN)
                goto reset;
            /* 一个和多个字符序列，直接忽略 */
            break;
        }
        case EHSHELL_ESCAPE_MATCH_ESC_SS3:{
            /* 匹配到任意字符后，直接忽略，正常来说会匹配 0x40..0x7E */
            if(is_csi_final((uint8_t)input)){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_NONE;
                break;
            }
            goto reset;
        }
        case EHSHELL_ESCAPE_MATCH_ESC_CSI:{
            if(is_csi_final((uint8_t)input)){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_NONE;
                if(shell->escape_char_parse_buf[0] < EHSHELL_ESCAPE_CHAR_PARSE_BUF_SIZE -1){
                    shell->escape_char_parse_buf[shell->escape_char_parse_buf[0]+1] = input;
                }
                shell->escape_char_parse_buf[0]++;
                if(shell->escape_char_parse_buf[0] >= EHSHELL_ESCAPE_CHAR_PARSE_BUF_SIZE - 1){
                    /* 容量有限，未存储完整字符忽略解析 */
                    shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_NONE;
                    break;
                }
                /* 开始解析存有的字符 */
                if(shell->escape_char_parse_buf[0] == 2 && shell->escape_char_parse_buf[2] == '~'){
                    switch (shell->escape_char_parse_buf[1]) {
                        case '1':
                            return ESCAPE_CHAR_CTRL_HOME;
                        case '3':
                            return ESCAPE_CHAR_CTRL_DELETE;
                        case '4':
                            return ESCAPE_CHAR_CTRL_END;
                        default:
                            return ESCAPE_CHAR_NUL;
                    }
                }else if(shell->escape_char_parse_buf[0] == 1){
                    switch (shell->escape_char_parse_buf[1]) {
                        case 'A':
                            return ESCAPE_CHAR_CTRL_UP;
                        case 'B':
                            return ESCAPE_CHAR_CTRL_DOWN;
                        case 'C':
                            return ESCAPE_CHAR_CTRL_RIGHT;
                        case 'D':
                            return ESCAPE_CHAR_CTRL_LEFT;
                        case 'F':
                            return ESCAPE_CHAR_CTRL_END;
                        case 'H':
                            return ESCAPE_CHAR_CTRL_HOME;
                        default:
                            return ESCAPE_CHAR_NUL;
                    }
                }

                break;
            }
            if(!is_middle_byte((uint8_t)input) && !is_param_byte((uint8_t)input))
                goto reset;
            if(shell->escape_char_parse_buf[0] < EHSHELL_ESCAPE_CHAR_PARSE_BUF_SIZE -1){
                shell->escape_char_parse_buf[shell->escape_char_parse_buf[0]+1] = input;
            }
            shell->escape_char_parse_buf[0]++;
            if( (uint32_t)shell->escape_char_parse_buf[0] >= EHSHELL_ESCAPE_CHAR_MAX_LEN)
                goto reset;
            break;
        }
        case EHSHELL_ESCAPE_MATCH_ESC_STRING_WAIT_ST:{
            if (input == '\\'){
                shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_NONE;
                break;
            }
            goto reset;
        }
    
    }

    return ESCAPE_CHAR_NUL;
reset:
    shell->escape_char_match_state = EHSHELL_ESCAPE_MATCH_NONE;
    return ESCAPE_CHAR_CTRL_RESET;
}


