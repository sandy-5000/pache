#include "services/init_pache.h"
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

int create_pache() {
    const char *dir = "p_cache";

    if (mkdir(dir, 0755) == -1) {
        if (errno == EEXIST) {
            printf("pache: directory '%s' already exists\n", dir);
        } else {
            perror("[ERROR]: failed to create directory");
            return 1;
        }
    }

    printf("Directory '%s' ready\n", dir);
    return 0;
}
