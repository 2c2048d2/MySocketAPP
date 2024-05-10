//
// Created by 2c2048d2 on 24-5-1.
//

#ifndef MYSOCKETAPP_FILEFUNCTIONS_H
#define MYSOCKETAPP_FILEFUNCTIONS_H

#include <dirent.h>
#include <fcntl.h>
#include <mqueue.h>
#include <myConfig.h>
#include <pthread.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <unistd.h>

#include <stdbool.h>

#include "myDataPack.h"
enum MQDataPackType {
    COMMAND_MKDIR,
    SEND_FILE,
    FINISH_FLAG,
};

struct MQDataPack {
    enum MQDataPackType type;
    char *src_path;
    char *dest_path;
};

struct threadStartSearchDirArg {
    int sockfd;
    const char *src_path;
    const char *dest_path;
    struct myDataPack *datapack;
};

struct threadStartSendDirArg {
    int sockfd;
    struct myDataPack *datapack;
};

const char *path2filename(const char *path);

void rtrim(char *str);

bool is_dir(const char *path);

bool is_file(const char *path);

bool is_file_dir(const char *path);

void send_file(const int sock_fd, const char *src_path, const char *dest_path,
               struct myDataPack *datapack);

void send_dir(int sock_fd, const char *src_path, const char *dest_path,
              struct myDataPack *datapack);

void search_dir(int sock_fd, const char *src_path, const char *dest_path,
                int depth, mqd_t mq_fd);

void send_command_mkdir(const int sock_fd, const char *path,
                        struct myDataPack *datapack);

#endif // MYSOCKETAPP_FILEFUNCTIONS_H
