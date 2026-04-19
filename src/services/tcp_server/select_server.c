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

/** variables */
int server_fd, max_fd, new_fd;
struct sockaddr_in addr;
socklen_t addrlen = sizeof(addr);

fd_set master_set, read_set;
int client_count = 0;

char buffer[BUFFER_SIZE];
/** variables - END */

int add_new_connection() {
    new_fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
    if (new_fd < 0) {
        perror("pache: socket accept error");
        return 1;
    }
    if (client_count >= MAX_CLIENTS || !running) {
        printf("Max clients reached. Rejecting fd=%d\n", new_fd);
        close(new_fd);
        return 2;
    }

    FD_SET(new_fd, &master_set);
    if (new_fd > max_fd) {
        max_fd = new_fd;
    }
    ++client_count;

    printf("pache: New connection fd=%d (clients=%d)\n", new_fd, client_count);
    return 0;
}

void read_buffer(int fd) {
    int bytes = read(fd, buffer, BUFFER_SIZE - 1);
    if (bytes <= 0) {
        printf("pache: Client disconnected fd=%d\n", fd);
        close(fd);
        FD_CLR(fd, &master_set);
        client_count--;
    } else {
        buffer[bytes] = '\0';
        printf("pache: fd %d: %s", fd, buffer);
        send(fd, buffer, bytes, 0);
    }
}

void close_all_connections() {
    printf("\npache: shutting down...\n");

    close(server_fd);
    FD_CLR(server_fd, &master_set);

    for (int fd = 0; fd <= max_fd; ++fd) {
        if (FD_ISSET(fd, &master_set)) {
            if (fd == server_fd) {
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

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("pcache: socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
        0) {
        perror("pcache: setsockopt failed");
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(TCP_SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("pache: socket bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("pache: socket listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", TCP_SERVER_PORT);

    FD_ZERO(&master_set);
    FD_SET(server_fd, &master_set);
    max_fd = server_fd;

    while (running) {
        read_set = master_set;
        if (select(max_fd + 1, &read_set, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("pache: socket select");
            continue;
        }

        for (int fd = 0; fd <= max_fd; ++fd) {
            if (!FD_ISSET(fd, &read_set)) {
                continue;
            }

            if (fd == server_fd) {
                add_new_connection();
            } else {
                read_buffer(fd);
            }
        }
    }

    close_all_connections();
}
