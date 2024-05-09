//
// Created by 2c2048d2 on 24-5-1.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <errno.h>

#include "fileFunctions.h"
#include "myDataPack.h"

void *thread_search_dir(void *arg) {
    struct threadStartSearchDirArg *data = arg;
    mqd_t mq = mq_open(MQ_NAME_SEND_DIR, O_WRONLY);
    printf("开始搜索文件夹\n");
    search_dir(data->sockfd, data->src_path, data->dest_path, 0, mq);
    printf("搜索文件夹完毕\n");
    struct MQDataPack dataPack;
    dataPack.type = FINISH_FLAG;
    dataPack.src_path = dataPack.dest_path = NULL;
    mq_send(mq, (const char *) &dataPack, sizeof dataPack, 0);
    mq_close(mq);
    return NULL;
}

void *thread_send_dir(void *arg) {
    struct threadStartSendDirArg *data = arg;

    mqd_t mq = mq_open(MQ_NAME_SEND_DIR, O_RDONLY);
    struct MQDataPack *dataPack = malloc(sizeof(struct MQDataPack));
    bool running_flag = 1;
    while (running_flag) {
        mq_receive(mq, (char *) (dataPack), sizeof(struct MQDataPack), NULL);
        switch (dataPack->type) {
            case COMMAND_MKDIR:
                send_command_mkdir(data->sockfd, dataPack->dest_path);
                break;
            case SEND_FILE:
                send_file(data->sockfd, dataPack->src_path, dataPack->dest_path);
                break;
            case FINISH_FLAG:
                running_flag = 0;
                break;
        }
        free(dataPack->dest_path);
        free(dataPack->src_path);
    }
    free(dataPack);
    return NULL;
}

void send_dir(int sock_fd, const char *src_path, const char *dest_path) {
    struct threadStartSearchDirArg *searchDirArg =
            malloc(sizeof(struct threadStartSearchDirArg));
    searchDirArg->sockfd = sock_fd;
    searchDirArg->src_path = src_path;
    searchDirArg->dest_path = dest_path;

    struct threadStartSendDirArg *sendDirArg =
            malloc(sizeof(struct threadStartSendDirArg));
    sendDirArg->sockfd = sock_fd;

    pthread_t search_thread, send_thread;
    int flag = 0;
    flag +=
            pthread_create(&search_thread, NULL, thread_search_dir, searchDirArg);
    flag += pthread_create(&send_thread, NULL, thread_send_dir, sendDirArg);
    if (flag)
        perror("pthread_create");

    pthread_join(search_thread, NULL);
    pthread_join(send_thread, NULL);

    free(searchDirArg);
    free(sendDirArg);
}

void send_file(const int sock_fd, const char *src_path, const char *dest_path) {
    if (access(src_path, F_OK) < 0) {
        printf("文件不存在\n");
        return;
    }

    if (access(src_path, R_OK) < 0) {
        printf("没有读的权限\n");
        return;
    }

    union myDataPackSubtype subtype;

    printf("源文件路径:%s\n", src_path);
    printf("目标路径:%s\n", dest_path);
    subtype.file_type = DATA_PACK_TYPE_FILE_START;
    send_data_pack(gen_data_pack(DATA_PACK_TYPE_FILE, subtype,
                                 strlen(dest_path) + 1, dest_path, NULL),
                   sock_fd, 1);

    struct myDataPack *received_data_pack = receive_data_pack(sock_fd);

    if (received_data_pack->type != DATA_PACK_TYPE_STATUS ||
        received_data_pack->subtype.status_type != DATA_PACK_TYPE_STATUS_OK) {
        printf("对方未就绪\n");
        free(received_data_pack);
        return;
    }
    free(received_data_pack);

    const int file_sock = open(src_path, O_RDONLY);
    unsigned long length;
    subtype.file_type = DATA_PACK_TYPE_FILE_SENDING;
    char *payload = malloc(sizeof(char) * BUF_SIZE);
    struct myDataPack *dataPack = malloc(sizeof(struct myDataPack) + BUF_SIZE);
    while ((length = read(file_sock, payload, BUF_SIZE)) > 0) {
        send_data_pack(gen_data_pack(DATA_PACK_TYPE_FILE, subtype, length,
                                     payload, dataPack),
                       sock_fd, 0);
    }

    free(dataPack);
    free(payload);

    subtype.file_type = DATA_PACK_TYPE_FILE_END;
    send_data_pack(gen_data_pack(DATA_PACK_TYPE_FILE, subtype, 0, NULL, NULL),
                   sock_fd, 1);

    received_data_pack = receive_data_pack(sock_fd);
    if (received_data_pack->type == DATA_PACK_TYPE_STATUS &&
        received_data_pack->subtype.status_type == DATA_PACK_TYPE_STATUS_OK) {
        printf("传输完成，没有发现错误\n");
    } else
        printf("传输过程中出现错误\n");
    free(received_data_pack);
}

void send_command_mkdir(const int sock_fd, const char *path) {
    printf("COMMAND: mkdir in %s\n", path);
    union myDataPackSubtype subtype;
    subtype.command_type = DATA_PACK_TYPE_COMMAND_MKDIR;
    send_data_pack(gen_data_pack(DATA_PACK_TYPE_COMMAND, subtype,
                                 strlen(path) + 1, path, NULL),
                   sock_fd, 1);
}

void search_dir(const int sock_fd, const char *src_path, const char *dest_path,
                const int depth, const mqd_t mq_fd) {
    printf(
            "FUNC "
            "search_dir:\n\tsockfd:%d\n\tsrc_path:%s\n\tdest_path:%s\n\tdepth:%d\n",
            sock_fd, src_path, dest_path, depth);
    if (depth > 10) {
        printf("递归深度最大为10层\n");
        return;
    }
    struct dirent *entry;
    struct stat file_stat;

    DIR *p_dir = opendir(src_path);
    if (p_dir == NULL) {
        perror("send_dir -> open dir");
        return;
    }
    struct MQDataPack dataPack;
    while ((entry = readdir(p_dir)) != NULL) {
        char next_dest_path[STR_SIZE], next_src_path[STR_SIZE];

        snprintf(next_dest_path, STR_SIZE, "%s/%s", dest_path, entry->d_name);
        snprintf(next_src_path, STR_SIZE, "%s/%s", src_path, entry->d_name);

        if (stat(next_src_path, &file_stat) < 0) {
            perror("stat 失败");
            continue;
        }

        if (S_ISDIR(file_stat.st_mode)) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;

            //            send_command_mkdir(sock_fd, next_dest_path);

            char *target_dest_path = malloc(STR_SIZE);
            strcpy(target_dest_path, next_dest_path);

            dataPack.type = COMMAND_MKDIR;
            dataPack.src_path = NULL;
            dataPack.dest_path = target_dest_path;
            printf("正在向消息队列发送数据|在%s创建文件夹\n", next_dest_path);
            mq_send(mq_fd, (const char *) &dataPack, sizeof(struct MQDataPack),
                    0);
            search_dir(sock_fd, next_src_path, next_dest_path, depth + 1,
                       mq_fd);
        } else {
            //            send_file(sock_fd, next_src_path, next_dest_path);

            char *target_dest_path = malloc(STR_SIZE);
            strcpy(target_dest_path, next_dest_path);

            char *target_src_path = malloc(STR_SIZE);
            strcpy(target_src_path, next_src_path);


            dataPack.type = SEND_FILE;
            dataPack.src_path = target_src_path;
            dataPack.dest_path = target_dest_path;
            printf("正在向消息队列发送数据|发送文件到%s\n", next_dest_path);
            mq_send(mq_fd, (const char *) &dataPack, sizeof(struct MQDataPack),
                    0);
        }
    }
    closedir(p_dir);
}

const char *path2filename(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL)
        return path;
    else
        return last_slash + 1;
}

void rtrim(char *str) {
    char *end = str + strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\n' ||
                          *end == '\r' || *end == '/'))
        end--;
    *(end + 1) = '\0';
}

bool is_dir(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0)
        return false;
    else
        return S_ISDIR(stat_buf.st_mode);
}

bool is_file(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0)
        return false;
    else
        return S_ISREG(stat_buf.st_mode);
}

bool is_file_dir(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0)
        return false;
    else
        return S_ISDIR(stat_buf.st_mode) || S_ISREG(stat_buf.st_mode);
}
