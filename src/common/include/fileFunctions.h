//
// Created by 2c2048d2 on 24-5-1.
//

#ifndef MYSOCKETAPP_FILEFUNCTIONS_H
#define MYSOCKETAPP_FILEFUNCTIONS_H

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
};

struct threadStartSendDirArg {
    int sockfd;
};

const char *path2filename(const char *path);

void rtrim(char *str);

bool is_dir(const char *path);

bool is_file(const char *path);

bool is_file_dir(const char *path);

void send_file(int sock_fd, const char *src_path, const char *dest_path);

void send_dir(int sock_fd, const char *src_path, const char *dest_path);

void search_dir(int sock_fd, const char *src_path, const char *dest_path, int depth, mqd_t mq_fd);

void send_command_mkdir(int sock_fd, const char *path);

#endif //MYSOCKETAPP_FILEFUNCTIONS_H
