//
// Created by 2c2048d2 on 24-4-21.
//

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "myConfig.h"
#include "myDataPack.h"

#define MAX_EVENTS 10

int init_server_socket(int *server_fd) {
    // 创建socket
    *server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_fd < 0) {
        perror("Socket Create");
        return -1;
    }

    // 设置服务器地址
    struct sockaddr_in address;
    socklen_t address_len = sizeof(struct sockaddr_in);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    // 绑定地址
    if (bind(*server_fd, (struct sockaddr *)&address, address_len) < 0) {
        perror("Bind");
        return -1;
    }

    /* 防止退出后继续占用端口 */
    int opt = 1;
    setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    // 开始监听
    if (listen(*server_fd, SOMAXCONN) < 0) {
        perror("listen");
        return -1;
    }

    return 1;
}

int init_epoll(const int *client_fd, int *epoll_fd) {
    *epoll_fd = epoll_create1(0);

    // 定义监听事件
    struct epoll_event event;
    event.data.fd = *client_fd;
    event.events =
        EPOLLIN | EPOLLERR |
        EPOLLRDHUP; // 触发事件 => in 有数据进入 out 数据可以输出 err 有错误发生

    // 添加事件
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, *client_fd, &event) == -1) {
        perror("epoll_ctl");
        return -1;
    }

    return 1;
}

int init(int *server_fd) {
    if (init_server_socket(server_fd) < 0) {
        perror("Init Server Socket");
        return -1;
    }

    // if (init_epoll(server_fd, epoll_fd) < 0) {
    //     perror("Init epoll");
    //     return -1;
    // }
    return 1;
}

void close_all_fd(const int *list, int n) {
    for (int i = 0; i < n; i++)
        close(*(list + i));
}

int main() {
    int server_fd;
    if (init(&server_fd) < 0) {
        perror("init");
        return -1;
    }
    printf("服务器socket描述符为%d\n", server_fd);

    // main loop
    uint connect_count = 0;
    bool running_flag = 1;
    char *message = malloc(BUF_SIZE); // 在main loop结束之后free
    int file_fd[128] = {0}; // 记录每个socket接收文件时 在本地创建的文件的描述符
    unsigned long file_size[128] = {0};

    struct myDataPack *datapack = malloc(sizeof(struct myDataPack) + BUF_SIZE);
    int client_fd;

    struct sockaddr_in addr;
    socklen_t size = sizeof(struct sockaddr_in);
    while ((client_fd = accept(server_fd, (struct sockaddr *)&addr, &size))) {
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        char ipstr[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &(addr.sin_addr), ipstr, INET_ADDRSTRLEN) ==
            NULL) {
            fprintf(stderr, "inet_ntop() 失败\n");
            return 1;
        }
        char full_ip_str[100];
        sprintf(full_ip_str, "%s:%d", ipstr, addr.sin_port);
        printf("连接%s已建立\n", full_ip_str);
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork失败");
            exit(EXIT_FAILURE);
        }

        if (pid > 0) {

        } else {
            struct epoll_event events[MAX_EVENTS];
            puts("新子进程");
            // 子进程
            int epoll_fd;
            init_epoll(&client_fd, &epoll_fd);
            int running_flag = 1;
            while (running_flag) {
                int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
                if (nfds == -1 && errno == EINTR)
                    continue; // 调试时发送中断信号应该会跳到这个分支
                if (nfds == -1) {
                    perror("epoll_wait");
                    return -1;
                }
                for (int i = 0; i < nfds; i++) {
                    // 客户端的事件
                    if (events[i].events & EPOLLRDHUP ||
                        events[i].events & EPOLLERR) {
                        printf("客户端%s已断开连接\n", full_ip_str);
                        running_flag = 0;
                        break;
                    }
                    receive_data_pack(client_fd, datapack);
                    if (datapack != NULL) {
                        union myDataPackSubtype subtype;
                        switch (datapack->type) {
                        case DATA_PACK_TYPE_INFO:
                            printf("收到来自客户端%s的信息:%*s\n", full_ip_str,
                                   (int)datapack->data_length,
                                   datapack->payload);
                            break;
                        case DATA_PACK_TYPE_COMMAND:
                            switch (datapack->subtype.command_type) {
                            case DATA_PACK_TYPE_COMMAND_QUIT:
                                printf("收到退出信号\n");
                                subtype.status_type = DATA_PACK_TYPE_STATUS_OK;
                                send_data_pack(
                                    gen_data_pack(DATA_PACK_TYPE_STATUS,
                                                  subtype, 0, NULL, datapack),
                                    client_fd);
                                running_flag = 0;
                                break;
                            case DATA_PACK_TYPE_COMMAND_GET_INFO:
                                sprintf(message, "当前共有%d个连接\n",
                                        connect_count);
                                subtype.info_type = DATA_PACK_TYPE_INFO_NORMAL;
                                send_data_pack(
                                    gen_data_pack(DATA_PACK_TYPE_INFO, subtype,
                                                  strlen(message) + 1, message,
                                                  datapack),
                                    client_fd);
                                break;
                            case DATA_PACK_TYPE_COMMAND_MKDIR:
                                printf("正在创建文件夹%s\n", datapack->payload);
                                if (mkdir(datapack->payload, 0777) < 0)
                                    perror("mkdir");
                                break;
                            }
                            break;
                        case DATA_PACK_TYPE_FILE:
                            switch (datapack->subtype.file_type) {
                            case DATA_PACK_TYPE_FILE_START:
                                printf("收到来自%s的新文件，文件名:%s\n",
                                       full_ip_str, datapack->payload);
                                int new_file_fd = open(
                                    datapack->payload,
                                    O_WRONLY | O_TRUNC | O_CREAT, DEFFILEMODE);
                                if (new_file_fd == -1) {
                                    perror("文件打开失败");
                                    subtype.status_type =
                                        DATA_PACK_TYPE_STATUS_FAILED;
                                    send_data_pack(
                                        gen_data_pack(DATA_PACK_TYPE_STATUS,
                                                      subtype, 0, NULL,
                                                      datapack),
                                        client_fd);
                                } else {
                                    file_size[client_fd] = 0;

                                    // printf("文件创建成功，描述符为%d\n",
                                    //        new_file_fd);
                                    file_fd[client_fd] = new_file_fd;
                                    subtype.status_type =
                                        DATA_PACK_TYPE_STATUS_OK;
                                    send_data_pack(
                                        gen_data_pack(DATA_PACK_TYPE_STATUS,
                                                      subtype, 0, NULL,
                                                      datapack),
                                        client_fd);
                                }
                                break;
                            case DATA_PACK_TYPE_FILE_SENDING:
                                // printf("当前文件已接收%ld字节数据\r",
                                //        file_size[fd] +=
                                //        datapack->data_length);
                                if (!write_until_finish(
                                        file_fd[client_fd], datapack->payload,
                                        datapack->data_length)) {
                                    perror("receiving file -> write");
                                    exit(1);
                                }
                                break;
                            case DATA_PACK_TYPE_FILE_END:
                                file_size[client_fd] = 0;
                                printf("来自%s的文件接收完毕\n", full_ip_str);
                                subtype.status_type = DATA_PACK_TYPE_STATUS_OK;
                                send_data_pack(
                                    gen_data_pack(DATA_PACK_TYPE_STATUS,
                                                  subtype, 0, NULL, datapack),
                                    client_fd);
                                close(file_fd[client_fd]);
                                break;
                            }
                            break;
                        case DATA_PACK_TYPE_STATUS:
                            printf("case DATA_PACK_TYPE_STATUS\n");
                            break;
                        }
                    }
                }
            }

            int fds[] = {client_fd, epoll_fd};
            close_all_fd(fds, sizeof fds);
        }
    }

    free(message);
    free(datapack);
    int fds[] = {server_fd};
    close_all_fd(fds, sizeof fds);

    return 0;
}
