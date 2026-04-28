#include "tcp_cmd_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/uio.h>

static int g_server_fd = -1;
static int g_client_fd = -1;
static pthread_t g_accept_thread;
static volatile int g_running = 0;
static cmd_callback_t g_callback = NULL;
static pthread_mutex_t g_client_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *client_handler(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    uint8_t buf[256];

    while (g_running) {
        cmd_header_t hdr;
        int n = recv(fd, &hdr, sizeof(hdr), MSG_WAITALL);
        if (n != (int)sizeof(hdr)) break;
        if (hdr.magic != CMD_MAGIC) continue;
        if (hdr.payload_len > sizeof(buf)) continue;

        if (hdr.payload_len > 0) {
            n = recv(fd, buf, hdr.payload_len, MSG_WAITALL);
            if (n != (int)hdr.payload_len) break;
        }

        if (g_callback) {
            g_callback(hdr.cmd_id, buf, hdr.payload_len);
        }
    }

    pthread_mutex_lock(&g_client_mutex);
    if (g_client_fd == fd) g_client_fd = -1;
    pthread_mutex_unlock(&g_client_mutex);
    close(fd);

    printf("[TCP] Client disconnected, auto-unlock tracking\n");
    if (g_callback) {
        g_callback(CMD_TRACK_UNLOCK, NULL, 0);
    }

    return NULL;
}

static void *accept_loop(void *arg) {
    (void)arg;
    while (g_running) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int fd = accept(g_server_fd, (struct sockaddr *)&addr, &len);
        if (fd < 0) {
            if (g_running) usleep(100000);
            continue;
        }
        printf("[TCP] Client connected: %s:%d\n",
               inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

        pthread_mutex_lock(&g_client_mutex);
        if (g_client_fd >= 0) {
            /* Shutdown 通知旧 client_handler 线程退出，而非直接 close */
            shutdown(g_client_fd, SHUT_RDWR);
        }
        g_client_fd = fd;
        pthread_mutex_unlock(&g_client_mutex);

        pthread_t t;
        int *pfd = (int *)malloc(sizeof(int));
        *pfd = fd;
        pthread_create(&t, NULL, client_handler, pfd);
        pthread_detach(t);
    }
    return NULL;
}

int tcp_cmd_server_start(cmd_callback_t cb) {
    g_callback = cb;

    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) return -1;

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(CMD_PORT);

    if (bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[TCP] bind");
        close(g_server_fd);
        g_server_fd = -1;
        return -1;
    }
    if (listen(g_server_fd, 1) < 0) {
        close(g_server_fd);
        g_server_fd = -1;
        return -1;
    }

    g_running = 1;
    pthread_create(&g_accept_thread, NULL, accept_loop, NULL);
    printf("[TCP] Command server started on port %d\n", CMD_PORT);
    return 0;
}

void tcp_cmd_server_stop(void) {
    g_running = 0;
    if (g_server_fd >= 0) {
        shutdown(g_server_fd, SHUT_RDWR);
        close(g_server_fd);
        g_server_fd = -1;
    }
    pthread_mutex_lock(&g_client_mutex);
    if (g_client_fd >= 0) {
        close(g_client_fd);
        g_client_fd = -1;
    }
    pthread_mutex_unlock(&g_client_mutex);
    pthread_join(g_accept_thread, NULL);
}

int tcp_cmd_server_send_status(const status_report_t *report) {
    pthread_mutex_lock(&g_client_mutex);
    int fd = g_client_fd;
    if (fd < 0) {
        pthread_mutex_unlock(&g_client_mutex);
        return -1;
    }

    cmd_header_t hdr;
    hdr.magic = CMD_MAGIC;
    hdr.cmd_id = CMD_STATUS_REPORT;
    hdr.payload_len = sizeof(status_report_t);
    struct iovec iov[2];
    iov[0].iov_base = &hdr;
    iov[0].iov_len  = sizeof(hdr);
    iov[1].iov_base = (void *)report;
    iov[1].iov_len  = sizeof(status_report_t);
    writev(fd, iov, 2);
    pthread_mutex_unlock(&g_client_mutex);
    return 0;
}

int tcp_cmd_server_send_target_list(const target_info_t *targets, uint8_t count) {
    pthread_mutex_lock(&g_client_mutex);
    int fd = g_client_fd;
    if (fd < 0) {
        pthread_mutex_unlock(&g_client_mutex);
        return -1;
    }

    if (count > MAX_TARGETS_PER_LIST) count = MAX_TARGETS_PER_LIST;

    cmd_header_t hdr;
    hdr.magic = CMD_MAGIC;
    hdr.cmd_id = CMD_TARGET_LIST;
    hdr.payload_len = 1 + count * sizeof(target_info_t);
    struct iovec iov[3];
    iov[0].iov_base = &hdr;
    iov[0].iov_len  = sizeof(hdr);
    iov[1].iov_base = &count;
    iov[1].iov_len  = 1;
    iov[2].iov_base = (void *)targets;
    iov[2].iov_len  = count * sizeof(target_info_t);
    writev(fd, iov, 3);
    pthread_mutex_unlock(&g_client_mutex);
    return 0;
}
