#ifndef GW_CONFIG_H
#define GW_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Network Server Defaults */
#define GW_CONFIG_DEFAULT_PORT          13400
#define GW_CONFIG_DEFAULT_BIND_IP       "0.0.0.0"
#define GW_CONFIG_LISTEN_BACKLOG        32
#define GW_CONFIG_MAX_CLIENTS           64

/* Socket Buffer Sizing */
#define GW_CONFIG_RECV_BUF_SIZE         (8 + 65535 + 1)
#define GW_CONFIG_SEND_BUF_SIZE         (8 + 65535 + 1)

/* Connection Timeouts in Milliseconds */
#define GW_CONFIG_RECV_TIMEOUT_MS       5000
#define GW_CONFIG_SEND_TIMEOUT_MS       5000

/* Logging Defaults */
#define GW_CONFIG_DEFAULT_LOG_FILE      "gw_server.log"

#ifdef __cplusplus
}
#endif

#endif /* GW_CONFIG_H */
