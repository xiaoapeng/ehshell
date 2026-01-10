/**
 * @file ehshell_escape_char.h
 * @brief 处理转义字符
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-11-13
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */
#ifndef _EHSHELL_ESCAPE_CHAR_H_
#define _EHSHELL_ESCAPE_CHAR_H_


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

enum ehshell_escape_char{
    ESCAPE_CHAR_NUL                 = 0x00,
    ESCAPE_CHAR_CTRL_A              = 0x01,
    ESCAPE_CHAR_CTRL_C_SIGINT       = 0x03,     /* 发送退出信号 */
    ESCAPE_CHAR_CTRL_BACKSPACE_0    = 0x08,     /* 删除前一个字符 */
    ESCAPE_CHAR_CTRL_TAB            = 0x09,     /* 可用于TAB补全 */
    ESCAPE_CHAR_CTRL_J_LF           = 0x0A,     /* 换行 Enter*/
    ESCAPE_CHAR_CTRL_L_CLS          = 0x0C,     /* 清除屏 */
    ESCAPE_CHAR_CTRL_M_CR           = 0x0D,     /* 回车 */
    ESCAPE_CHAR_CTRL_U_DEL_LINE     = 0x0E,     /* 删除当前行 */
    ESCAPE_CHAR_CTRL_W_DEL_WORD     = 0x0F,     /* 删除当前单词 */
    ESCAPE_CHAR_CTRL_Z              = 0x1A,     /* 发送退出信号 */
    ESCAPE_CHAR_CTRL_BACKSPACE_1    = 0x7f,     /* 删除前一个字符 */
    ESCAPE_CHAR_CTRL_UTF8_START     = 0x80,     /* UTF-8 多字节字符开始 */
    ESCAPE_CHAR_CTRL_NOSTD_START    = 0xFF,     /* 非标准转义字符 */
    ESCAPE_CHAR_CTRL_RESET,                     /* 重置终端 */
    ESCAPE_CHAR_CTRL_HOME,                      /* 移动光标到行首 */
    ESCAPE_CHAR_CTRL_END,                       /* 移动光标到行尾 */
    ESCAPE_CHAR_CTRL_LEFT,                      /* 移动光标左 */
    ESCAPE_CHAR_CTRL_RIGHT,                     /* 移动光标右 */
    ESCAPE_CHAR_CTRL_UP,                        /* 移动光标上 */
    ESCAPE_CHAR_CTRL_DOWN,                      /* 移动光标下 */
    ESCAPE_CHAR_CTRL_DELETE,                    /* 删除光标后面的字符 */
};



#define EHSHELL_ESCAPE_MATCH_INCOMPLETE             0xfe
#define EHSHELL_ESCAPE_MATCH_FAIL                   0xff


#define EHSHELL_ESCAPE_MATCH_NONE                   ((uint16_t)0x00)
#define EHSHELL_ESCAPE_MATCH_ESC                    ((uint16_t)0x01)        
#define EHSHELL_ESCAPE_MATCH_ESC_OSC                ((uint16_t)0x02)        // after ESC ]
#define EHSHELL_ESCAPE_MATCH_ESC_DCS                ((uint16_t)0x03)        // after ESC P    
#define EHSHELL_ESCAPE_MATCH_ESC_PM                 ((uint16_t)0x04)        // after ESC ^    
#define EHSHELL_ESCAPE_MATCH_ESC_APC                ((uint16_t)0x05)        // after ESC _    
#define EHSHELL_ESCAPE_MATCH_ESC_SS3                ((uint16_t)0x06)        // after ESC O    
#define EHSHELL_ESCAPE_MATCH_ESC_CSI                ((uint16_t)0x07)        // after ESC [
#define EHSHELL_ESCAPE_MATCH_ESC_STRING_WAIT_ST     ((uint16_t)0x08)        // expecting ESC '\' terminator


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif // _EHSHELL_ESCAPE_CHAR_H_