#include "constants/globals.h"
#include "services/cache/cache_server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 65536
#define BUFFER_SIZE 4096
#define MAX_EVENTS 1024

static volatile sig_atomic_t running = 1;

struct server_state {
    int server_fd;
    int kq;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int client_count;
    char buffer[BUFFER_SIZE];
};

static int add_new_connection(struct server_state *context) {
    socklen_t addrlen = sizeof(context->client_addr);
    int new_fd = accept(context->server_fd, (struct sockaddr *)&context->client_addr, &addrlen);
    if (new_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1;
        }
        perror("pache: socket accept error");
        return 2;
    }
    fcntl(new_fd, F_SETFL, O_NONBLOCK);

    if (context->client_count >= MAX_CLIENTS || !running) {
        printf("Max clients reached (%d). Rejecting fd=%d\n", context->client_count, new_fd);
        close(new_fd);
        return 3;
    }

    struct kevent ev;
    EV_SET(&ev, new_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(context->kq, &ev, 1, NULL, 0, NULL) < 0) {
        perror("pache: kevent add client error");
        close(new_fd);
        return 4;
    }
    ++context->client_count;

    // printf("pache: new connection fd=%d clients=%d\n", new_fd, context->client_count);
    return 0;
}

static void remove_connection(int fd, struct server_state *ctx) {
    // printf("pache: client disconnected fd=%d\n", fd);
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(ctx->kq, &ev, 1, NULL, 0, NULL);
    close(fd);
    if (ctx->client_count > 0) {
        ctx->client_count--;
    }
}

static void read_buffer(int fd, struct server_state *context) {
    while (1) {
        ssize_t bytes = read(fd, context->buffer, BUFFER_SIZE - 1);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            remove_connection(fd, context);
            return;
        }
        if (bytes == 0) {
            remove_connection(fd, context);
            return;
        }
        context->buffer[bytes] = '\0';
        for (ssize_t i = 0; i < bytes; i++) {
            if (context->buffer[i] == '\n') {
                // printf("pache: fd %d: %s", fd, context->buffer);
                fetch_data(0, fd, context->buffer, 0);
            }
        }
    }
}

void kqueue_handle_sigint(int sig) {
    running = 0;

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        printf("pache: failed to create terminator socket\n");
        return;
    }

    struct sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    close(sock);
}

void start_kqueue_tcp_server() {
    struct server_state context;

    memset(&context, 0, sizeof(context));

    context.server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (context.server_fd < 0) {
        perror("pcache: socket creation failed");
        exit(EXIT_FAILURE);
    }
    fcntl(context.server_fd, F_SETFL, O_NONBLOCK);

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

    if (listen(context.server_fd, SOMAXCONN) < 0) {
        perror("pache: socket listen failed");
        exit(EXIT_FAILURE);
    }

    context.kq = kqueue();
    if (context.kq < 0) {
        perror("pache: kqueue init failed");
        exit(EXIT_FAILURE);
    }

    struct kevent ev;
    EV_SET(&ev, context.server_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(context.kq, &ev, 1, NULL, 0, NULL) < 0) {
        perror("pache: kevent server add failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", TCP_SERVER_PORT);

    struct kevent events[MAX_EVENTS];

    while (running) {

        int nev = kevent(context.kq, NULL, 0, events, MAX_EVENTS, NULL);

        if (!running) {
            break;
        }
        if (nev < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("pache: kevent wait");
            continue;
        }

        for (int i = 0; i < nev; ++i) {
            int fd = (int)events[i].ident;
            if (events[i].flags & EV_ERROR) {
                if (fd == context.server_fd) {
                    fprintf(stderr, "pache: server socket failed\n");
                    running = 0;
                    break;
                } else {
                    fprintf(stderr, "pache: kevent error: %s\n", strerror((int)events[i].data));
                    remove_connection(fd, &context);
                }
                continue;
            }

            if (fd == context.server_fd) {
                while (1) {
                    int rc = add_new_connection(&context);
                    if (rc != 0) {
                        break;
                    }
                }
            } else {
                read_buffer(fd, &context);
            }
        }
    }

    close(context.server_fd);
    close(context.kq);
}
