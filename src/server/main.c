//
// Created by 2c2048d2 on 24-4-21.
//

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

struct epoll_event events[MAX_EVENTS];

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

int init_epoll(const int *server_fd, int *epoll_fd) {
    *epoll_fd = epoll_create1(0);

    // 定义监听事件
    struct epoll_event event;
    event.data.fd = *server_fd;
    event.events =
        EPOLLIN; // 触发事件 => in 有数据进入 out 数据可以输出 err 有错误发生

    // 添加事件
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, *server_fd, &event) == -1) {
        perror("epoll_ctl");
        return -1;
    }

    return 1;
}

int init(int *server_fd, int *epoll_fd) {
    if (init_server_socket(server_fd) < 0) {
        perror("Init Server Socket");
        return -1;
    }

    if (init_epoll(server_fd, epoll_fd) < 0) {
        perror("Init epoll");
        return -1;
    }
    return 1;
}

void close_all_fd(const int *list, int n) {
    for (int i = 0; i < n; i++)
        close(*(list + i));
}

int main() {
    int server_fd, epoll_fd;
    if (init(&server_fd, &epoll_fd) < 0) {
        perror("init");
        return -1;
    }
    printf("服务器socket描述符为%d,epoll描述符为%d\n", server_fd, epoll_fd);

    // main loop
    uint connect_count = 0;
    bool running_flag = 1;
    char *message = malloc(BUF_SIZE); // 在main loop结束之后free
    int file_fd[128] = {0}; // 记录每个socket接收文件时 在本地创建的文件的描述符
    unsigned long file_size[128] = {0};

    while (running_flag) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        /* nfds => number of file descriptors */
        if (nfds == -1 && errno == EINTR)
            continue; // 调试时发送中断信号应该会跳到这个分支
        if (nfds == -1) {
            perror("epoll_wait");
            return -1;
        }
        // 处理每个连接
        for (int i = 0; i < nfds; i++) {
            const int fd = events[i].data.fd; // 当前连接的描述符
            if (fd == server_fd) {
                // 服务器的事件(accept
                int client_fd;
                while ((client_fd = accept(server_fd, NULL, NULL)) > 0) {
                    connect_count++;

                    // addr
                    printf("新连接，描述符为%d\n", client_fd);

                    // 设置为非阻塞模式
                    //                    int flags = fcntl(client_fd, F_GETFL,
                    //                    0); // F_GET FL =>
                    //                    获取当前的文件的描述符标记
                    //                    fcntl(client_fd, F_SETFL, flags |
                    //                    O_NONBLOCK);

                    // 添加监听事件
                    struct epoll_event event;
                    event.data.fd = client_fd;
                    event.events = EPOLLIN | EPOLLRDHUP;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) ==
                        -1) {
                        perror("epoll_ctl");
                    }
                }

                if (client_fd == -1 && errno != EAGAIN &&
                    errno != EWOULDBLOCK) {
                    /*
                     * 在非阻塞模式下，如果调用 accept 而没有新的连接请求到来
                     * 它会返回 -1 并设置 errno 为 EAGAIN 或 EWOULDBLOCK
                     * 表示“尝试再次”或“操作本来就会阻塞”
                     * 若不是上述情况，则表明accept时出现了其他问题
                     */
                    perror("accept");
                }
            } else {
                // 客户端的事件
                if (events[i].events & EPOLLRDHUP) {
                    printf("客户端%d断开连接\n", fd);
                    connect_count--;
                    close(fd);
                    continue;
                }
                struct myDataPack *data = receive_data_pack(fd);
                if (data != NULL) {
                    union myDataPackSubtype subtype;
                    switch (data->type) {
                    case DATA_PACK_TYPE_INFO:
                        printf("收到来自客户端%d的信息:%*s\n", fd,
                               (int)data->data_length, data->payload);
                        break;
                    case DATA_PACK_TYPE_COMMAND:
                        switch (data->subtype.command_type) {
                        case DATA_PACK_TYPE_COMMAND_QUIT:
                            printf("收到退出信号\n");
                            subtype.status_type = DATA_PACK_TYPE_STATUS_OK;
                            send_data_pack(gen_data_pack(DATA_PACK_TYPE_STATUS,
                                                         subtype, 0, NULL,
                                                         NULL),
                                           fd, 1);
                            running_flag = 0;
                            break;
                        case DATA_PACK_TYPE_COMMAND_GET_INFO:
                            sprintf(message, "当前共有%d个连接\n",
                                    connect_count);
                            subtype.info_type = DATA_PACK_TYPE_INFO_NORMAL;
                            send_data_pack(gen_data_pack(DATA_PACK_TYPE_INFO,
                                                         subtype,
                                                         strlen(message) + 1,
                                                         message, NULL),
                                           fd, 1);
                            break;
                        case DATA_PACK_TYPE_COMMAND_MKDIR:
                            printf("正在创建文件夹%s\n", data->payload);
                            if (mkdir(data->payload, 0777) < 0) perror("mkdir");
                            break;
                        }
                        break;
                    case DATA_PACK_TYPE_FILE:
                        switch (data->subtype.file_type) {
                        case DATA_PACK_TYPE_FILE_START:
                            printf("收到新文件，文件名:%s\n", data->payload);
                            int new_file_fd =
                                open(data->payload,
                                     O_WRONLY | O_TRUNC | O_CREAT, DEFFILEMODE);
                            if (new_file_fd == -1) {
                                perror("文件打开失败");
                                subtype.status_type =
                                    DATA_PACK_TYPE_STATUS_FAILED;
                                send_data_pack(
                                    gen_data_pack(DATA_PACK_TYPE_STATUS,
                                                  subtype, 0, NULL, NULL),
                                    fd, 1);
                            } else {
                                file_size[fd] = 0;

                                printf("文件创建成功，描述符为%d\n",
                                       new_file_fd);
                                file_fd[fd] = new_file_fd;
                                subtype.status_type = DATA_PACK_TYPE_STATUS_OK;
                                send_data_pack(
                                    gen_data_pack(DATA_PACK_TYPE_STATUS,
                                                  subtype, 0, NULL, NULL),
                                    fd, 1);
                            }
                            break;
                        case DATA_PACK_TYPE_FILE_SENDING:
                            printf("当前文件已接收%ld字节数据\r",
                                   file_size[fd] += data->data_length);
                            if (!write_until_finish(file_fd[fd], data->payload,
                                                    data->data_length)) {
                                perror("receiving file -> write");
                                exit(1);
                            }
                            break;
                        case DATA_PACK_TYPE_FILE_END:
                            file_size[fd] = 0;
                            printf("\n文件接收完毕\n");
                            subtype.status_type = DATA_PACK_TYPE_STATUS_OK;
                            send_data_pack(gen_data_pack(DATA_PACK_TYPE_STATUS,
                                                         subtype, 0, NULL,
                                                         NULL),
                                           fd, 1);
                            close(file_fd[fd]);
                            break;
                        }
                        break;
                    case DATA_PACK_TYPE_STATUS:
                        printf("case DATA_PACK_TYPE_STATUS\n");
                        break;
                    }
                }
                free(data);
            }
        }
    }
    // main loop finished
    free(message);

    int fds[] = {epoll_fd, server_fd};
    close_all_fd(fds, sizeof fds);

    return 0;
}
