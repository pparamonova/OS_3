#ifndef COMMON_H
#define COMMON_H

#include <unistd.h>

#define BUF_SIZE 4096

typedef struct {
    int len;
    char data[BUF_SIZE];
} shm_t;

static int str_len(const char *s) {
    int n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static void print_str(int fd, const char *s) {
    write(fd, s, str_len(s));
}

#endif
