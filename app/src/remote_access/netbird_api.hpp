#pragma once
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

int netbird_init(const char* managementServer, const char* setupKey, char* error, size_t error_len);
void netbird_poll(void);
int netbird_is_ready(void);
int netbird_get_peer_count(void);
int netbird_get_peer(int index, char* ip, size_t ip_len, char* name, size_t name_len);
int netbird_proxy_start(const char* peer_ip, uint16_t port);
int netbird_proxy_start_udp(const char* peer_ip);
void netbird_proxy_stop(void);
void netbird_proxy_stop_udp(void);
void netbird_shutdown(void);

#ifdef __cplusplus
}
#endif
