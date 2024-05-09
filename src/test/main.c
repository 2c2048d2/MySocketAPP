#include <fcntl.h> /* For O_* constants */
#include <mqueue.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> /* For mode constants */
#include <unistd.h>

#define MQ_MSG_MAX_SIZE 512 ///< 最大消息长度
#define MQ_MSG_MAX_ITEM 5   ///< 最大消息数目

static pthread_t s_thread1_id;
static pthread_t s_thread2_id;
static unsigned char s_thread1_running = 0;
static unsigned char s_thread2_running = 0;

static mqd_t s_mq;
static char send_msg[10] = "hello";

void *thread1_fun(void *arg) {
    int ret = 0;

    s_thread1_running = 1;
    while (s_thread1_running) {
        ret = mq_send(s_mq, send_msg, sizeof(send_msg), 0);
        if (ret < 0) {
            perror("mq_send error");
        }
        printf("send msg = %s\n", send_msg);
        usleep(100 * 1000);
    }

    pthread_exit(NULL);
}

void *thread2_fun(void *arg) {
    char buf[MQ_MSG_MAX_SIZE];
    int recv_size = 0;

    s_thread2_running = 1;
    while (s_thread2_running) {
        recv_size = mq_receive(s_mq, &buf[0], sizeof(buf), NULL);
        if (-1 != recv_size) {
            printf("receive msg = %s\n", buf);
        } else {
            perror("mq_receive error");
            break;
        }

        usleep(100 * 1000);
    }

    pthread_exit(NULL);
}

int main(void) {
    int ret = 0;
    struct mq_attr attr = {0};
    attr.mq_maxmsg = MQ_MSG_MAX_ITEM;
    attr.mq_msgsize = MQ_MSG_MAX_SIZE;
    attr.mq_flags = 0;
    s_mq = mq_open("/mq", O_CREAT | O_RDWR, 0777, &attr);
    if (-1 == s_mq) {
        perror("mq_open error");
        return -1;
    }

    ///< 创建线程1
    ret = pthread_create(&s_thread1_id, NULL, thread1_fun, NULL);
    if (ret != 0) {
        printf("thread1_create error!\n");
        exit(EXIT_FAILURE);
    }
    ret = pthread_detach(s_thread1_id);
    if (ret != 0) {
        printf("s_thread1_id error!\n");
        exit(EXIT_FAILURE);
    }

    ///< 创建线程2
    ret = pthread_create(&s_thread2_id, NULL, thread2_fun, NULL);
    if (ret != 0) {
        printf("thread2_create error!\n");
        exit(EXIT_FAILURE);
    }
    ret = pthread_detach(s_thread2_id);
    if (ret != 0) {
        printf("s_thread2_id error!\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        sleep(1);
    }

    return 0;
}
