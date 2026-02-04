/**
 * @file ehshell.h
 * @brief ehshell header file
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2025-11-09
 * 
 * @copyright Copyright (c) 2025  simon.xiaoapeng@gmail.com
 * 
 */

#ifndef _EHSHELL_H_
#define _EHSHELL_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

typedef struct ehshell ehshell_t;
typedef struct ehshell_cmd_context ehshell_cmd_context_t;
typedef struct eh_ringbuf eh_ringbuf_t;
typedef struct eh_event_flags eh_event_flags_t;
struct stream_base;

enum ehshell_quit_result{
    EHSHELL_QUIT_SUCCESS = 0,
    EHSHELL_QUIT_REJECTED,
};

struct ehshell_config{
    void (*stream_write)(ehshell_t* ehshell, const char *buf, size_t len);
    void (*stream_finish)(ehshell_t* ehshell);
    void (*input_ringbuf_process_finish)(ehshell_t* ehshell);
    const char *host;
    /**
     * @brief 退出ehshell信号函数,请在该函数中，清理ehshell实例占用的资源，否则请返回EHSHELL_QUIT_REJECTED
     * @param ehshell  ehshell实例指针
     * @return enum ehshell_quit_result 退出结果
     */
    enum ehshell_quit_result (*quit_shell)(ehshell_t* ehshell);
    uint16_t input_ringbuf_size;
    uint16_t input_linebuf_size;
};

enum ehshell_event{
    EHSHELL_EVENT_SHELL_EXIT = (1 << 0),                /* 退出ehshell */
    EHSHELL_EVENT_SIGINT_REQUEST_QUIT = (1 << 1),       /* 请求外部请求退出命令 */
    EHSHELL_EVENT_RECEIVE_INPUT_DATA = (1 << 2),        /* 接收输入数据事件 */
};


struct ehshell_command_info{
    const char *command;
    const char *description;
    const char *usage;
#define EHSHELL_COMMAND_REDIRECT_INPUT (1 << 0)        /* 命令行重定向输入到本命令 */
    uint32_t   flags;

    /**
     * @brief                   命令处理函数
     * @param  cmd_context      命令上下文指针
     * @param  argc             命令参数数量
     * @param  argv             命令参数数组
     */
    void (*do_function)(ehshell_cmd_context_t *cmd_context, int argc, const char *argv[]);
    
    /**
     * @brief                   事件处理函数
     * @param  event_flags      事件标志位指针
     */
    void (*do_event_function)(ehshell_cmd_context_t *cmd_context, enum ehshell_event ehshell_event);
};

#define EHSHELL_EVENT_FLAGS_SIGINT (1 << 0)

/**
 * @brief                   创建ehshell实例,ehshell对象具有任务亲和性，只能在创建它的任务中进行destroy
 * @param  static_config    配置参数,注意static_config的生命周期必须大于等于ehshell实例的生命周期
 *                          建议直接使用 static 关键字定义
 * @return ehshell_t*       返回ehshell实例指针,错误值由 eh_ptr_to_error() 获取
 */
extern ehshell_t *ehshell_create(const struct ehshell_config *static_config);

/**
 * @brief                   销毁ehshell实例
 * @param  ehshell          ehshell实例指针
 */
extern void ehshell_destroy(ehshell_t *ehshell);

/**
 * @brief                   设置ehshell用户数据,用户数据可以在ehshell命令处理函数中使用
 * @param  ehshell          ehshell实例指针
 * @param  user_data        用户数据指针
 */
extern void ehshell_set_userdata(ehshell_t *ehshell, void *user_data);

/**
 * @brief                   获取ehshell用户数据
 * @param  ehshell          ehshell实例指针
 * @return void*            返回用户数据指针
 */
extern void *ehshell_get_user_data(ehshell_t *ehshell);


/**
 * @brief                   设置ehshell登录密码,该函数只有使用启用密码登录时才有效
 * @param  password         密码字符串
 * @return int              成功返回0, 失败返回负数
 */
extern int ehshell_set_password(const char *password);

/**
 * @brief                   获取ehshell输入环形缓冲区,可用于在中断或者任务中写入数据，
 *                          数据写入完成后应该调用ehshell_notify_process通知ehshell处理数据
 * @param  ehshell          ehshell实例指针
 * @return eh_ringbuf_t*    返回ehshell输入环形缓冲区指针
 */
extern eh_ringbuf_t* ehshell_input_ringbuf(ehshell_t *ehshell);

/**
 * @brief                   通知ehshell处理输入环形缓冲区,或者通知处理其他任务
 * @param  ehshell          ehshell实例指针
 */
extern void ehshell_notify_processor(ehshell_t *ehshell);

/**
 * @brief                   运行ehshell命令
 * @param  cmd_context      命令上下文指针
 * @param  argc             命令参数数量
 * @param  argv             命令参数数组
 * @return int              成功返回0, 失败返回负数
 */
extern int ehshell_command_run(ehshell_t *ehshell, int argc, const char *argv[]);


/**
 * @brief                   运行ehshell命令字符串
 * @param  ehshell          ehshell实例指针
 * @param  cmd_str          命令字符串
 * @return int              成功返回0, 失败返回负数
 */
extern int ehshell_command_run_form_string(ehshell_t *ehshell, const char *cmd_str);

/**
 * @brief                   获取ehshell命令处理流
 * @param  cmd_context      命令上下文指针
 * @return struct stream_base* 返回ehshell命令处理流指针
 */
extern struct stream_base *ehshell_command_stream(ehshell_cmd_context_t *cmd_context);

/**
 * @brief                   获取ehshell命令使用说明字符串
 * @param  cmd_context      命令上下文指针
 * @return const char*      返回ehshell命令使用说明字符串指针
 */
extern const char *ehshell_command_usage(ehshell_cmd_context_t *cmd_context);


/**
 * @brief                   获取ehshell命令输入环形缓冲区
 * @param  cmd_context      命令上下文指针
 * @param  readable_size    可读取数据大小指针,命令必须使用此处给出的大小进行读取
 * @return eh_ringbuf_t*    返回ehshell命令输入环形缓冲区指针,失败返回NULL
 */
extern eh_ringbuf_t* ehshell_command_input_ringbuf(ehshell_cmd_context_t *cmd_context, int32_t *readable_size);

/**
 * @brief                   获取ehshell实例
 * @param  cmd_context      命令上下文指针
 * @return ehshell_t*       返回ehshell实例指针
 */
extern ehshell_t* ehshell_command_get_shell(ehshell_cmd_context_t *cmd_context);

/**
 * @brief                   通知ehshell命令处理完成，在命令处理完成后必须进行调用
 * @param  cmd_context      命令上下文指针
 */
extern void ehshell_command_finish(ehshell_cmd_context_t *cmd_context);

/**
 * @brief                   设置ehshell命令上下文用户数据
 * @param  cmd_context      命令上下文指针
 * @param  user_data        用户数据指针
 */
extern void ehshell_command_set_userdata(ehshell_cmd_context_t *cmd_context, void *user_data);

/**
 * @brief                   获取ehshell命令上下文用户数据
 * @param  cmd_context      命令上下文指针
 * @return void*            返回用户数据指针
 */
extern void *ehshell_command_get_userdata(ehshell_cmd_context_t *cmd_context);


/**
 * @brief                   获取ehshell命令上下文命令信息
 * @param  cmd_context      命令上下文指针
 * @return const struct ehshell_command_info* 返回ehshell命令上下文命令信息指针
 */
extern const struct ehshell_command_info * ehshell_command_getcommand_info(ehshell_cmd_context_t *cmd_context);

/**
 * @brief                   注册命令
 * @param  command_info     命令信息指针, 必须是静态数组,或者生命周期大于ehshell实例生命周期
 * @param  command_info_num 命令信息数组大小
 * @return int              成功返回0, 失败返回负数
 */
extern int ehshell_register_commands(const struct ehshell_command_info *command_info, size_t command_info_num);







#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif // _EHSHELL_H_