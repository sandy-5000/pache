#include "services/tcp_server/select_server.h"

void handle_sigint(int sig) { select_handle_sigint(sig); }

void start_tcp_server() { start_select_tcp_server(); }
