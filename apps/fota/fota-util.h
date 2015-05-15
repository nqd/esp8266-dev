#ifndef _FOTA_COMMON_H_
#define _FOTA_COMMON_H_

#ifndef INFO
#define INFO os_printf
#endif

#ifndef REPORT
#define REPORT os_printf
#endif


int8_t convert_version(const char *version_str, uint32_t len, uint32_t *version_bin);
int8_t parse_fota(const char *json, uint32_t len, char *version, char *host, char *url, char *protocol);

#endif