#ifndef CACHE_SERVER_H
#define CACHE_SERVER_H

void open_cache_file();
void fetch_data(int cache_id, int fd, char *key);

#endif
