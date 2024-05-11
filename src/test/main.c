#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int read_size;

    // 读取客户端发送的数据
    while ((read_size = read(client_socket, buffer, sizeof(buffer))) > 0) {
        // 输出接收到的数据
        write(client_socket, buffer, read_size);
        write(STDOUT_FILENO, buffer, read_size);
    }

    if (read_size == 0) {
        puts("客户端断开连接");
    } else if (read_size == -1) {
        perror("读取失败");
    }

    // 关闭客户端socket
    close(client_socket);
}

int main() {
    int server_fd, client_socket, c;
    struct sockaddr_in server, client;
    pid_t pid;

    // 创建socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket创建失败");
        exit(EXIT_FAILURE);
    }

    // 设置socket地址
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // 绑定socket
    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("绑定失败");
        exit(EXIT_FAILURE);
    }

    // 监听
    if (listen(server_fd, 3) < 0) {
        perror("监听失败");
        exit(EXIT_FAILURE);
    }

    // 等待并接受连接
    puts("等待客户端连接...");
    c = sizeof(struct sockaddr_in);
    while ((client_socket = accept(server_fd, (struct sockaddr *)&client,
                                   (socklen_t *)&c))) {
        puts("连接已建立");

        // fork一个进程来处理新的连接
        pid = fork();
        if (pid < 0) {
            perror("fork失败");
            exit(EXIT_FAILURE);
        }

        // 子进程处理客户端连接
        if (pid == 0) {
            close(server_fd); // 子进程不需要监听socket
            handle_client(client_socket);
            exit(EXIT_SUCCESS);
        } else {
            close(client_socket); // 父进程不需要客户端socket
        }
    }

    if (client_socket < 0) {
        perror("接受连接失败");
        exit(EXIT_FAILURE);
    }

    return 0;
}
