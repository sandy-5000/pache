#include "constants/globals.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

volatile sig_atomic_t running = 1;

struct server_state {
    int server_fd, max_fd;
    struct sockaddr_in server_addr, client_addr;
    fd_set master_set, read_set;
    int client_count;
    char buffer[BUFFER_SIZE];
};

int add_new_connection(struct server_state *context) {
    socklen_t addrlen = sizeof(context->client_addr);
    int new_fd = accept(context->server_fd, (struct sockaddr *)&context->client_addr, &addrlen);
    if (new_fd < 0) {
        perror("pache: socket accept error");
        return 1;
    }
    if (context->client_count >= MAX_CLIENTS || !running) {
        printf("Max clients reached. Rejecting fd=%d\n", new_fd);
        close(new_fd);
        return 2;
    }

    FD_SET(new_fd, &context->master_set);
    if (context->max_fd < new_fd) {
        context->max_fd = new_fd;
    }
    ++context->client_count;

    printf("pache: New connection fd=%d (clients=%d)\n", new_fd, context->client_count);
    return 0;
}

void remove_connection(int fd, struct server_state *context) {
    if (FD_ISSET(fd, &context->master_set)) {
        printf("pache: Client disconnected fd=%d\n", fd);
        FD_CLR(fd, &context->master_set);
        context->client_count--;
    }
    close(fd);
}

void read_buffer(int fd, struct server_state *context) {
    int sent = 0;
    int bytes = read(fd, context->buffer, BUFFER_SIZE - 1);
    if (bytes <= 0) {
        remove_connection(fd, context);
    } else {
        context->buffer[bytes] = '\0';
        printf("pache: fd %d: %s", fd, context->buffer);
        while (sent < bytes) {
            int n = send(fd, context->buffer + sent, bytes - sent, 0);
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                remove_connection(fd, context);
                return;
            }
            sent += n;
        }
    }
}

void close_all_connections(struct server_state *context) {
    printf("\npache: shutting down...\n");

    close(context->server_fd);
    FD_CLR(context->server_fd, &context->master_set);

    for (int fd = 0; fd <= context->max_fd; ++fd) {
        if (FD_ISSET(fd, &context->master_set)) {
            if (fd == context->server_fd) {
                printf("pache: closing server socket fd=%d\n", fd);
            } else {
                printf("pache: closing client fd=%d\n", fd);
            }
            close(fd);
        }
    }

    printf("pache: server stopped cleanly\n");
}

void select_handle_sigint(int sig) {
    running = 0;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    close(sock);
}

void start_select_tcp_server() {
    struct server_state context;

    context.client_count = 0;
    context.server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (context.server_fd < 0) {
        perror("pcache: socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(context.server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("pcache: setsockopt failed");
        exit(EXIT_FAILURE);
    }

    context.server_addr.sin_family = AF_INET;
    context.server_addr.sin_addr.s_addr = INADDR_ANY;
    context.server_addr.sin_port = htons(TCP_SERVER_PORT);

    if (bind(context.server_fd, (struct sockaddr *)&context.server_addr, sizeof(context.server_addr)) < 0) {
        perror("pache: socket bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(context.server_fd, MAX_CLIENTS) < 0) {
        perror("pache: socket listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", TCP_SERVER_PORT);

    FD_ZERO(&context.master_set);
    FD_SET(context.server_fd, &context.master_set);
    context.max_fd = context.server_fd;

    while (running) {
        context.read_set = context.master_set;
        if (select(context.max_fd + 1, &context.read_set, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("pache: socket select");
            continue;
        }

        for (int fd = 0; fd <= context.max_fd; ++fd) {
            if (!FD_ISSET(fd, &context.read_set)) {
                continue;
            }

            if (fd == context.server_fd) {
                add_new_connection(&context);
            } else {
                read_buffer(fd, &context);
            }
        }
    }

    close_all_connections(&context);
}
