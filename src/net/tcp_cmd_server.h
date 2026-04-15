#ifndef TCP_CMD_SERVER_H
#define TCP_CMD_SERVER_H

#include "cmd_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Callback invoked when a command is received (called from server thread) */
typedef void (*cmd_callback_t)(uint8_t cmd_id, const void *payload, uint16_t len);

/* Start TCP command server on CMD_PORT. Non-blocking: spawns background thread. */
int tcp_cmd_server_start(cmd_callback_t cb);

/* Stop the server and close all connections. */
void tcp_cmd_server_stop(void);

/* Send a status report to the connected client. Thread-safe. */
int tcp_cmd_server_send_status(const status_report_t *report);

#ifdef __cplusplus
}
#endif

#endif /* TCP_CMD_SERVER_H */
