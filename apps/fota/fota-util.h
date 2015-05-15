#ifndef _FOTA_COMMON_H_
#define _FOTA_COMMON_H_

#ifndef INFO
#define INFO os_printf
#endif

#ifndef REPORT
#define REPORT os_printf
#endif

#define VERSION_DIGIT_MAX   255             // max value of 1B
#define VERSION_BYTE        3               // MAJOR.MINOR.PATCH
#define VERSION_BYTE_STR    (VERSION_BYTE*3+(VERSION_BYTE-1)+1)       // xxx.xxx.xxx\0


int8_t convert_version(const char *version_str, uint32_t len, uint32_t *version_bin);
int8_t parse_fota(const char *json, uint32_t len, uint32_t *version, char *host, char *url);

#endif