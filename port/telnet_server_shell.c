/**
 * @file telnet_server_shell.c
 * @brief telnet server shell
 * @author simon.xiaoapeng (simon.xiaoapeng@gmail.com)
 * @date 2026-01-22
 * 
 * @copyright Copyright (c) 2026  simon.xiaoapeng@gmail.com
 * 
 */

#include <stddef.h>
#include <string.h>
#include <eh.h>
#include <eh_mem.h>
#include <eh_error.h>
#include <eh_ringbuf.h>
#include <eh_module.h>
#include <eh_debug.h>
#include <eh_platform.h>
#include <eh_debug.h>
#include <eh_signal.h>
#include <eh_comp_timer.h>
#include <ehip-ipv4/tcp.h>
#include <ehip-ipv4/ip.h>

#include <ehshell.h>
#include <ehshell_module.h>
#include <autoconf.h>

struct telnet_server_client{
    tcp_pcb_t pcb;
    union{
        ehshell_t *shell;
        uint32_t  timer_downcnt;
    };
    eh_signal_slot_t    slot_timerout;
};

static tcp_server_pcb_t telnet_server = NULL;
static struct telnet_server_client *telnet_client_pcbs[CONFIG_PACKAGE_EHSHELL_BUILTIN_TELNET_SERVER_SHELL_MAX_CLIENTS] = {0};
static int telent_server_get_free_client_index(void){
    for(int i = 0; i < CONFIG_PACKAGE_EHSHELL_BUILTIN_TELNET_SERVER_SHELL_MAX_CLIENTS; i++){
        if(telnet_client_pcbs[i] == NULL)
            return i;
    }
    return -1;
}
static int telent_server_get_index(struct telnet_server_client *client){
    for(int i = 0; i < CONFIG_PACKAGE_EHSHELL_BUILTIN_TELNET_SERVER_SHELL_MAX_CLIENTS; i++){
        if(telnet_client_pcbs[i] == client)
            return i;
    }
    return -1;
}

static void telent_server_ehshell_clean_client(int client_index){
    struct telnet_server_client *client = telnet_client_pcbs[client_index];
    if(!client)
        return ;
    ehip_tcp_client_delete(client->pcb);
    if(eh_signal_slot_is_connected(&client->slot_timerout)){
        eh_signal_slot_disconnect(&signal_eh_comp_timer_100ms, &client->slot_timerout);
    }else{
        ehshell_destroy(client->shell);
    }
    eh_free(client);
    telnet_client_pcbs[client_index] = NULL;
}

static int telnet_server_ehshell_auto_recv(struct telnet_server_client *client){
    eh_ringbuf_t* input_ringbuf = ehshell_input_ringbuf(client->shell);
    int32_t free_size;
    eh_ringbuf_t *rx_ringbuf = ehip_tcp_client_get_recv_ringbuf(client->pcb);
    int32_t wl = 0;
    int32_t len;
    uint8_t *data_ptr;
    if((free_size = eh_ringbuf_free_size(input_ringbuf)) == 0)
        return 0;
    len = 0;
    data_ptr = (uint8_t *)eh_ringbuf_peek(rx_ringbuf, 0, NULL, &len);
    eh_debugfl("recv:|%.*hhq|", len, data_ptr);
    wl += eh_ringbuf_write(input_ringbuf, data_ptr, len);
    if(wl >= free_size)
        goto finish;
    len = 0;
    data_ptr = (uint8_t *)eh_ringbuf_peek(rx_ringbuf, wl, NULL, &len);
    eh_debugfl("recv:|%.*hhq|", len, data_ptr);
    wl += eh_ringbuf_write(input_ringbuf, data_ptr, len);
finish:
    eh_ringbuf_read_skip(rx_ringbuf, wl);
    return wl;
}

static void telnet_server_ehshell_ringbuf_process_finish(ehshell_t *shell){
    struct telnet_server_client *client = ehshell_get_user_data(shell);
    int ret;
    ret = telnet_server_ehshell_auto_recv(client);
    if(ret <= 0) return ;
    ehip_tcp_client_request_update(client->pcb, TCP_RECV);
    ehshell_notify_processor(shell);
}

static void telnet_server_ehshell_stream_finish(ehshell_t *shell){
    struct telnet_server_client *client = ehshell_get_user_data(shell);
    ehip_tcp_client_request_update(client->pcb, TCP_SNED);
}
static void telnet_server_ehshell_stream_write(ehshell_t* ehshell, const char *buf, size_t len){
    struct telnet_server_client *client = ehshell_get_user_data(ehshell);
    eh_ringbuf_t *tx_ringbuf = ehip_tcp_client_get_send_ringbuf(client->pcb);
    int32_t wl = 0;
    wl += eh_ringbuf_write(tx_ringbuf, (uint8_t *)buf, (int32_t)len);
    if(wl != (int32_t)len)
        eh_mwarnfl(TELNET_SERVER, "telnet server write ringbuf overflow, wl: %d, len: %d", wl, len);
    if(eh_ringbuf_free_size(tx_ringbuf) <= (CONFIG_PACKAGE_EHSHELL_BUILTIN_TELNET_SERVER_SHELL_TCP_TX_BUFFER_SIZE/2))
        ehip_tcp_client_request_update(client->pcb, TCP_SNED);
}

static enum ehshell_quit_result telnet_server_ehshell_quit(ehshell_t *ehshell){
    struct telnet_server_client *client = ehshell_get_user_data(ehshell);
    int client_index = telent_server_get_index(client);
    static const uint8_t telnet_exit_cmds[] = {
        0xFF, 0xFD, 0x12,
    };
    eh_ringbuf_t *tx_ringbuf = ehip_tcp_client_get_send_ringbuf(client->pcb);
    eh_ringbuf_write(tx_ringbuf, telnet_exit_cmds, sizeof(telnet_exit_cmds));
    ehip_tcp_client_request_update(client->pcb, TCP_SNED);
    if(client_index >= 0)
        telent_server_ehshell_clean_client(client_index);
    return EHSHELL_QUIT_SUCCESS;
}

static const struct ehshell_config ehshell_config_default = {
    .host = "eventos-telnet-server",
    .input_linebuf_size = CONFIG_PACKAGE_EHSHELL_BUILTIN_TELNET_SERVER_SHELL_LINE_BUFFER_SIZE,
    .input_ringbuf_size = CONFIG_PACKAGE_EHSHELL_BUILTIN_TELNET_SERVER_SHELL_INPUT_BUFFER_SIZE,
    .input_ringbuf_process_finish = telnet_server_ehshell_ringbuf_process_finish,
    .stream_finish = telnet_server_ehshell_stream_finish,
    .quit_shell = telnet_server_ehshell_quit,
    .stream_write = telnet_server_ehshell_stream_write,
};

static void telnet_server_timerout(eh_event_t *e, void *slot_param){
    (void)e;
    struct telnet_server_client *client = slot_param;
    int client_index = telent_server_get_index(client);
    if(client_index < 0){
        eh_mwarnfl(TELNET_SERVER, "telnet server get client index failed");
        return ;
    }
    if(client->timer_downcnt > 0)
        client->timer_downcnt--;
    if(client->timer_downcnt == 0){
        /* 没有等到协商数据，超时开启shell */
        client->shell = ehshell_create(&ehshell_config_default);
        if(client->shell == NULL){
            telent_server_ehshell_clean_client(client_index);
            return ;
        }
        ehshell_set_userdata(client->shell, client);
        eh_signal_slot_disconnect(&signal_eh_comp_timer_100ms, &client->slot_timerout);
    }
}

static void telnet_server_tcp_event_callback(tcp_pcb_t pcb, enum tcp_event state){
    (void)pcb;
    struct telnet_server_client *client = ehip_tcp_client_get_userdata(pcb);
    int client_index = telent_server_get_index(client);
    if(client_index < 0){
        eh_mwarnfl(TELNET_SERVER, "telnet server get client index failed");
        ehip_tcp_client_delete(pcb);
        return ;
    }
    switch (state) {
    case TCP_CONNECT_TIMEOUT:
    case TCP_ERROR:
    case TCP_RECV_FIN:
    case TCP_RECV_RST:
    case TCP_SEND_TIMEOUT:
    case TCP_KEEPALIVE_TIMEOUT:
    case TCP_DISCONNECTED:
        telent_server_ehshell_clean_client(client_index);
        break;
    case TCP_RECV_DATA:{
        int ret;
        if(eh_signal_slot_is_connected(&client->slot_timerout)){
            /* 处理协商数据 */
            eh_ringbuf_t *rx_ringbuf = ehip_tcp_client_get_recv_ringbuf(client->pcb);
            if(eh_ringbuf_free_size(rx_ringbuf) == 0)
                break;
            /* 协商数据存在，我们不进行任何处理，直接清除掉，设置200ms超时，等待开启shell */
            eh_ringbuf_clear(rx_ringbuf);
            client->timer_downcnt = 2; /* 200ms */
            break;
        }
        ret = telnet_server_ehshell_auto_recv(client);
        if(ret > 0)
            ehshell_notify_processor(client->shell);
        break;
    }
    case TCP_CONNECTED:
    case TCP_RECV_ACK:
        break;
    }
}

static void telnet_server_tcp_new_connect(tcp_server_pcb_t server_pcb, tcp_pcb_t new_client){
    struct telnet_server_client *client = NULL;
    int ret;
    (void)server_pcb;
    int client_index = telent_server_get_free_client_index();
    if(client_index < 0){
        eh_mwarnfl(TELNET_SERVER, "telnet server max client reached");
        goto error;
    }
    client = eh_malloc(sizeof(struct telnet_server_client));
    if(client == NULL){
        eh_merrfl(TELNET_SERVER, "malloc telnet_server_client failed");
        goto error;
    }
    memset(client, 0, sizeof(struct telnet_server_client));
    client->pcb = new_client;
    ehip_tcp_client_set_userdata(new_client, client);
    ehip_tcp_set_events_callback(new_client, telnet_server_tcp_event_callback);
    {
        static const uint8_t telnet_init_cmds[] = {
            0xFF, 0xFB, 0x01,  // IAC WILL ECHO
            0xFF, 0xFB, 0x03,  // IAC WILL SUPPRESS-GO-AHEAD
            // 0xFF, 0xFD, 0x01,  // IAC DO ECHO
            0xFF, 0xFD, 0x03,  // IAC DO SUPPRESS-GO-AHEAD
        };
        eh_ringbuf_t *tx_ringbuf = ehip_tcp_client_get_send_ringbuf(new_client);
        eh_ringbuf_write(tx_ringbuf, telnet_init_cmds, sizeof(telnet_init_cmds));
        ehip_tcp_client_request_update(new_client, TCP_SNED);
    }
    eh_signal_slot_init(&client->slot_timerout, telnet_server_timerout, client);
    ret = eh_signal_slot_connect(&signal_eh_comp_timer_100ms, &client->slot_timerout);
    if(ret < 0){
        eh_merrfl(TELNET_SERVER, "eh_signal_slot_connect failed");
        goto eh_signal_slot_connect_error;
    }
    client->timer_downcnt = 5; /* 500ms */
    telnet_client_pcbs[client_index] = client;
    return ;
eh_signal_slot_connect_error:
    eh_free(client);
error:
    ehip_tcp_client_delete(new_client);
}

static int __init telnet_server_shell_init(void){
    int ret;
    telnet_server = ehip_tcp_server_any_new(
        eh_hton16(CONFIG_PACKAGE_EHSHELL_BUILTIN_TELNET_SERVER_SHELL_PORT),
        CONFIG_PACKAGE_EHSHELL_BUILTIN_TELNET_SERVER_SHELL_TCP_RX_WINDOW_SIZE,
        CONFIG_PACKAGE_EHSHELL_BUILTIN_TELNET_SERVER_SHELL_TCP_TX_BUFFER_SIZE);
    if(telnet_server == NULL){
        eh_merrfl(TELNET_SERVER, "ehip_tcp_server_any_new failed");
        return EH_RET_MALLOC_ERROR;
    }
    ehip_tcp_server_set_new_connect_callback(telnet_server, telnet_server_tcp_new_connect);
    
    ret = ehip_tcp_server_listen(telnet_server);
    if(ret < 0){
        eh_merrfl(TELNET_SERVER, "ehip_tcp_server_listen failed");
        ehip_tcp_server_delete(telnet_server);
        return ret;
    }
    return 0;
}

static void __exit telnet_server_shell_exit(void){
    for(int i=0;i<CONFIG_PACKAGE_EHSHELL_BUILTIN_TELNET_SERVER_SHELL_MAX_CLIENTS;i++)
        telent_server_ehshell_clean_client(i);
    ehip_tcp_server_delete(telnet_server);
}

ehshell_module_shell_export(telnet_server_shell_init, telnet_server_shell_exit);
