#ifndef _FOTA_INFO_H_
#define _FOTA_INFO_H_

void get_version_disconnect_cb(void *arg);
void get_version_connect_cb(void *arg);
void get_version_recon_cb(void *arg, sint8 err);

#endif