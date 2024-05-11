//
// Created by 2c2048d2 on 24-4-29.
//

#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "myDataPack.h"

struct myDataPack *gen_data_pack(enum myDataPackType type,
                                 union myDataPackSubtype subtype,
                                 unsigned long data_length, const char *payload,
                                 struct myDataPack *dataPack) {
    //    printf("生成数据包中，payload大小为%ld，总大小：%ld\n", data_length,
    //    sizeof(struct myDataPack) + data_length);
    if (dataPack == NULL) {
        printf("Warning: NULL data pack ptr\n");
        dataPack = malloc(sizeof(struct myDataPack) + data_length);
    }
    dataPack->type = type;
    dataPack->subtype = subtype;
    dataPack->data_length = data_length;
    if (data_length) memcpy(dataPack->payload, payload, data_length);
    return dataPack;
}

void receive_data(void *dest, int src_fd, unsigned long size) {
    if (dest == NULL) {
        printf("receive_data -> \n\t dest IS NULL\n");
        return;
    }
    if (!read_until_finish(src_fd, dest, size)) {
        perror("receive_dara read");
    }
}

void receive_data_pack(int sock_fd, struct myDataPack *datapack) {
    receive_data(datapack, sock_fd, sizeof(struct myDataPack));
    receive_data(datapack->payload, sock_fd, datapack->data_length);
}

void send_data_pack(struct myDataPack *data_pack, int sock_fd) {
    if (data_pack == NULL) {
        printf("send_data_pack: data_pack is NULL \n");
        return;
    }

    if (!send_until_finish(sock_fd, data_pack,
                           sizeof(struct myDataPack) +
                               data_pack->data_length)) {
        perror("send_data_pack send");
    }
}

bool write_until_finish(int fd, const void *buf, size_t n) {
    ssize_t cnt = 0, now = 0;
    do {
        now = write(fd, buf + cnt, n - cnt);
        if (now >= 0) cnt += now;
        else {
            perror("write");
            return false;
        }
    } while (cnt < n);
    return true;
}
bool read_until_finish(int fd, void *buf, size_t n) {
    ssize_t cnt = 0, now = 0;
    do {
        now = read(fd, buf + cnt, n - cnt);
        if (now >= 0) cnt += now;
        else {
            perror("read");
            return false;
        }
    } while (cnt < n);
    return true;
}
bool send_until_finish(int fd, const void *buf, size_t n) {
    ssize_t cnt = 0, now = 0;
    do {
        now = send(fd, buf + cnt, n - cnt, 0);
        if (now >= 0) cnt += now;
        else {
            perror("send");
            return false;
        }
    } while (cnt < n);
    return true;
}
