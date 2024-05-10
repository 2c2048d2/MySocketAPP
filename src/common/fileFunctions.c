//
// Created by 2c2048d2 on 24-5-1.
//

#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
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
    mq_send(mq, (const char *)&dataPack, sizeof dataPack, 0);
    mq_close(mq);
    return NULL;
}

void *thread_send_dir(void *arg) {
    struct threadStartSendDirArg *data = arg;

    mqd_t mq = mq_open(MQ_NAME_SEND_DIR, O_RDONLY);
    struct MQDataPack *MQDataPack = malloc(sizeof(struct MQDataPack));
    bool running_flag = 1;
    while (running_flag) {
        mq_receive(mq, (char *)(MQDataPack), sizeof(struct MQDataPack), NULL);
        switch (MQDataPack->type) {
        case COMMAND_MKDIR:
            send_command_mkdir(data->sockfd, MQDataPack->dest_path,
                               data->datapack);
            break;
        case SEND_FILE:
            send_file(data->sockfd, MQDataPack->src_path, MQDataPack->dest_path,
                      data->datapack);
            break;
        case FINISH_FLAG:
            running_flag = 0;
            break;
        }
        free(MQDataPack->dest_path);
        free(MQDataPack->src_path);
    }
    free(MQDataPack);
    mq_close(mq);
    return NULL;
}

void send_dir(int sock_fd, const char *src_path, const char *dest_path,
              struct myDataPack *datapack) {
    struct threadStartSearchDirArg *searchDirArg =
        malloc(sizeof(struct threadStartSearchDirArg));
    searchDirArg->sockfd = sock_fd;
    searchDirArg->src_path = src_path;
    searchDirArg->dest_path = dest_path;
    searchDirArg->datapack = datapack;

    struct threadStartSendDirArg *sendDirArg =
        malloc(sizeof(struct threadStartSendDirArg));
    sendDirArg->sockfd = sock_fd;
    sendDirArg->datapack = datapack;

    pthread_t search_thread, send_thread;
    int flag = 0;
    flag +=
        pthread_create(&search_thread, NULL, thread_search_dir, searchDirArg);
    flag += pthread_create(&send_thread, NULL, thread_send_dir, sendDirArg);
    if (flag) perror("pthread_create");

    pthread_join(search_thread, NULL);
    pthread_join(send_thread, NULL);

    free(searchDirArg);
    free(sendDirArg);
}

void send_file(const int sock_fd, const char *src_path, const char *dest_path,
               struct myDataPack *datapack) {
    if (access(src_path, F_OK) < 0) {
        printf("文件不存在或无读的权限\n");
        return;
    }
    printf("send_file point0\n");
    union myDataPackSubtype subtype;

    printf("源文件路径:%s\n", src_path);
    printf("目标路径:%s\n", dest_path);
    subtype.file_type = DATA_PACK_TYPE_FILE_START;
    send_data_pack(gen_data_pack(DATA_PACK_TYPE_FILE, subtype,
                                 strlen(dest_path) + 1, dest_path, datapack),
                   sock_fd);

    receive_data_pack(sock_fd, datapack);

    printf("send_file point1\n");

    if (datapack->type != DATA_PACK_TYPE_STATUS ||
        datapack->subtype.status_type != DATA_PACK_TYPE_STATUS_OK) {
        printf("对方未就绪\n");
        return;
    }
    printf("send_file point2\n");
    const int file_sock = open(src_path, O_RDONLY);
    unsigned long length;
    subtype.file_type = DATA_PACK_TYPE_FILE_SENDING;
    datapack->type = DATA_PACK_TYPE_FILE;
    datapack->subtype.file_type = DATA_PACK_TYPE_FILE_SENDING;
    while ((length = read(file_sock, datapack->payload, BUF_SIZE))) {
        if (length < 0) {
            perror("send file -> read");
            return;
        }
        datapack->data_length = length;
        send_data_pack(datapack, sock_fd);
    }

    subtype.file_type = DATA_PACK_TYPE_FILE_END;
    send_data_pack(
        gen_data_pack(DATA_PACK_TYPE_FILE, subtype, 0, NULL, datapack),
        sock_fd);

    receive_data_pack(sock_fd, datapack);
    if (datapack->type == DATA_PACK_TYPE_STATUS &&
        datapack->subtype.status_type == DATA_PACK_TYPE_STATUS_OK) {
        printf("传输完成，没有发现错误\n");
    } else printf("传输过程中出现错误\n");
}

void send_command_mkdir(const int sock_fd, const char *path,
                        struct myDataPack *datapack) {
    printf("COMMAND: mkdir in %s\n", path);
    union myDataPackSubtype subtype;
    subtype.command_type = DATA_PACK_TYPE_COMMAND_MKDIR;
    send_data_pack(gen_data_pack(DATA_PACK_TYPE_COMMAND, subtype,
                                 strlen(path) + 1, path, datapack),
                   sock_fd);
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
            mq_send(mq_fd, (const char *)&dataPack, sizeof(struct MQDataPack),
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
            if (mq_send(mq_fd, (const char *)&dataPack,
                        sizeof(struct MQDataPack), 0) < 0) {
                perror("mq_send");
                mq_close(mq_fd);
                exit(EXIT_FAILURE);
            }
        }
    }
    closedir(p_dir);
}

const char *path2filename(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) return path;
    else return last_slash + 1;
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
    if (stat(path, &stat_buf) != 0) return false;
    else return S_ISDIR(stat_buf.st_mode);
}

bool is_file(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) return false;
    else return S_ISREG(stat_buf.st_mode);
}

bool is_file_dir(const char *path) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) return false;
    else return S_ISDIR(stat_buf.st_mode) || S_ISREG(stat_buf.st_mode);
}
