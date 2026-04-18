#include "constants/page.h"
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

int main() {
    const char *dir = "p_cache";

    if (mkdir(dir, 0755) == -1) {
        if (errno != EEXIST) {
            perror("mkdir failed");
            return 1;
        }
    }

    printf("Persistant Cache - **pache!**\n");
    printf("Page Size - %d\n", PAGE_SIZE);
    printf("Directory '%s' ready\n", dir);

    return 0;
}
