#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/sendfile.h>
#endif

struct cache_context {
    int cache_fd;
};

struct cache_context context;

void open_cache_file() {
    const char *path = "p_cache/data-file";
    context.cache_fd = open(path, O_RDWR | O_CREAT, 0644);
    if (context.cache_fd < 0) {
        perror("pache: failed to open cache file");
        exit(EXIT_FAILURE);
    }
    printf("pache: Cache file opened: %s\n", path);
}

void fetch_data(int cache_id, int fd, char *key) {

#ifdef __linux__

    off_t offset = 0;
    ssize_t sent = sendfile(fd, context.cache_fd, &offset, 50);
    if (sent < 0) {
        perror("pache: sendfile failed");
    }

#elif __APPLE__

    off_t len = 50;
    if (sendfile(context.cache_fd, fd, 0, &len, NULL, 0) < 0) {
        perror("pache: sendfile failed");
    }

#else

    printf("pache: sendfile not supported on this platform\n");

#endif
}
