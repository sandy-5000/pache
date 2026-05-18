#include "services/tcp_server/kqueue_server.h"
#include "services/tcp_server/select_server.h"

void handle_sigint(int sig) {

#ifdef __APPLE__

    kqueue_handle_sigint(sig);

#else

    select_handle_sigint(sig);

#endif
}

void start_tcp_server() {

#ifdef __APPLE__

    start_kqueue_tcp_server();

#else

    start_select_tcp_server();

#endif
}
