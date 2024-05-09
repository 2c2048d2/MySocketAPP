//
// Created by 2c2048d2 on 24-4-29.
//

#ifndef MYSOCKETAPP_MYCONFIG_H
#define MYSOCKETAPP_MYCONFIG_H

#define SERVER_PORT 8848
#define BUF_SIZE (1 << 14)
#define STR_SIZE 1024
#define MQ_NAME_SEND_DIR "/MySockAPP_File2Send"

#ifndef DEFFILEMODE
#define DEFFILEMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif

#endif
