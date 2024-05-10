//
// Created by 2c2048d2 on 24-4-21.
//
#include <arpa/inet.h>
#include <mqueue.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "fileFunctions.h"
#include "myConfig.h"
#include "myDataPack.h"

enum userOpt {
    USER_OPT_UPLOAD = 1,
    USER_OPT_UPLOAD_DIR = 2,
    USER_OPT_DOWNLOAD = 3,
    USER_OPT_DOWNLOAD_DIR = 4,
    USER_OPT_SEND_MESSAGE = 5,
    USER_OPT_INFO = 6,
    USER_OPT_QUIT = 7,
    USER_OPT_STOP_SERVER = 8,
};

enum userOpt get_user_opt() {
    long opt;
    bool ok = 0;
    char input_buff[BUF_SIZE], *end = input_buff;
    do {
        printf("请输入选项：\n");
        printf("1. 上传文件   2. 上传文件夹\n");
        printf("3. 下载文件   4. 下载文件夹\n");
        printf("5. 发送信息   6. 获取信息\n");
        printf("7. 断开连接   8. 发送退出信号\n");

        fgets(input_buff, BUF_SIZE, stdin);
        opt = strtol(input_buff, &end, 10);

        if (input_buff == end || *end != '\n') {
            printf("请输入一个合法的整数\n");
            fflush(stdin);
        } else if (opt < 1 || opt > 8) {
            printf("没有这个选项\n");
        } else ok = 1;
    } while (!ok);
    return opt;
}

int init_socket(int *client_fd) {
    int status = *client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (status < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    size_t buf_size = BUF_SIZE;
    char *ip = NULL, *end_ptr;
    char *buff = malloc(sizeof(char) * buf_size);
    do {
        printf("1：本地，2：mac，3: termux\n");
        getline(&buff, &buf_size, stdin);
        rtrim(buff);
        const long x = strtol(buff, &end_ptr, 10);
        ip = x == 1 ? "127.0.0.1" : ip;
        ip = x == 2 ? "192.168.31.219" : ip;
        ip = x == 3 ? "192.168.31.104" : ip;
    } while (ip == NULL);
    free(buff);

    status = inet_pton(AF_INET, ip, &server_addr.sin_addr);
    //    free(ip);
    if (status <= 0) {
        perror("inet_pton");
        return -1;
    }

    status = connect(*client_fd, (struct sockaddr *)&server_addr,
                     sizeof server_addr);
    if (status < 0) {
        perror("connect");
        return -1;
    }

    return 1;
}

int init_MQ() {
    mq_unlink(MQ_NAME_SEND_DIR);
    struct mq_attr attr;
    attr.mq_curmsgs = 0;
    attr.mq_flags = 0;
    attr.mq_msgsize = sizeof(struct MQDataPack);
    attr.mq_maxmsg = 10;
    int mq = mq_open(MQ_NAME_SEND_DIR, O_CREAT | O_EXCL | O_RDWR, 0777, &attr);
    if (mq < 0) return -1;
    return 1;
}

void cleanup_MQ() {
    mq_unlink(MQ_NAME_SEND_DIR);
    /* FIXME mq_close(MQ_NAME_SEND_DIR);
     * user maybe not use send_dir option so that mq will be not closed */
}

int main() {
    int client_fd;
    if (init_socket(&client_fd) < 0) {
        /* FIXME close client_fd if it's initialized done */
        perror("init socket");
        return -1;
    }

    if (atexit(cleanup_MQ) != 0) {
        perror("at exit");
        exit(EXIT_FAILURE);
    }

    if (init_MQ() < 0) {
        perror("init_MQ");
        return -1;
    }

    bool running_flag = 1;
    while (running_flag) {
        enum userOpt opt = get_user_opt();
        union myDataPackSubtype subtype;
        switch (opt) {
        case USER_OPT_UPLOAD: {
            size_t buf_size = BUF_SIZE;
            printf("请输入你要上传的文件:\n");
            char *filepath = malloc(buf_size);
            getline(&filepath, &buf_size, stdin);
            rtrim(filepath);
            send_file(client_fd, filepath, path2filename(filepath));
            free(filepath);
            break;
        }
        case USER_OPT_UPLOAD_DIR: {
            size_t buf_size = BUF_SIZE;
            printf("请输入你要发送的文件夹:\n");
            char *path = malloc(sizeof(char) * buf_size);
            getline(&path, &buf_size, stdin);
            rtrim(path);
            printf("获取到的路径:%s\n", path);
            const char *dir_name = path2filename(path);
            char *start_relative_path = malloc(STR_SIZE);
            sprintf(start_relative_path, "./%s", dir_name);
            if (is_file_dir(path) > 0) {
                send_command_mkdir(client_fd, start_relative_path);
                send_dir(client_fd, path, start_relative_path);
            } else printf("文件(夹)不存在\n");
            free(path);
            break;
        }
        case USER_OPT_DOWNLOAD:
        case USER_OPT_DOWNLOAD_DIR:
            printf("DNF\n");
            break;
        case USER_OPT_SEND_MESSAGE: {
            size_t buf_size = BUF_SIZE;
            printf("请输入你要向服务端发送的消息：\n");
            char *buffer = malloc(sizeof(char) * buf_size);
            getline(&buffer, &buf_size, stdin);
            rtrim(buffer);

            subtype.info_type = DATA_PACK_TYPE_INFO_NORMAL;
            send_data_pack(gen_data_pack(DATA_PACK_TYPE_INFO, subtype,
                                         strlen(buffer), buffer, NULL),
                           client_fd, 1);
            free(buffer);
            break;
        }
        case USER_OPT_INFO: {
            subtype.command_type = DATA_PACK_TYPE_COMMAND_GET_INFO;
            send_data_pack(
                gen_data_pack(DATA_PACK_TYPE_COMMAND, subtype, 0, NULL, NULL),
                client_fd, 1);
            struct myDataPack *p = receive_data_pack(client_fd);
            printf("服务器的数据：\n%s", p->payload);
            free(p);
            break;
        }
        case USER_OPT_QUIT:
            running_flag = 0;
            break;
        case USER_OPT_STOP_SERVER:

            subtype.command_type = DATA_PACK_TYPE_COMMAND_QUIT;
            send_data_pack(
                gen_data_pack(DATA_PACK_TYPE_COMMAND, subtype, 0, NULL, NULL),
                client_fd, 1);
            struct myDataPack *p = receive_data_pack(client_fd);
            if (p->type == DATA_PACK_TYPE_STATUS &&
                p->subtype.status_type == DATA_PACK_TYPE_STATUS_OK) {
                printf("服务器已退出\n");
                running_flag = 0;
            } else printf("未收到服务器成功退出的消息\n");
            break;
        }
    }
    close(client_fd);
    return 0;
}
