#include "services/init_pache.h"
#include "services/tcp_server/server.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

void *server_thread(void *arg) {
    start_tcp_server();
    return NULL;
}

int main() {

    signal(SIGINT, handle_sigint);

    if (create_pache()) {
        return 1;
    }

    pthread_t tid;

    if (pthread_create(&tid, NULL, server_thread, NULL) != 0) {
        perror("pthread_create failed");
        return 1;
    }

    pthread_join(tid, NULL);

    return 0;
}
