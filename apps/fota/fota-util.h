#ifndef _FOTA_COMMON_H_
#define _FOTA_COMMON_H_


int8_t convert_version(const char *version_str, uint32_t len, uint32_t *version_bin);
int8_t parse_fota(const char *json, uint32_t len, char **version, char **host, char **url, char **protocol);
void start_esp_connect(struct espconn *conn, uint8_t secure);
void clear_tcp_of_espconn(struct espconn *conn);

#endif