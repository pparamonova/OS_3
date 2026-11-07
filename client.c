#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"

static void fail(const char *msg) {
    print_str(2, "client: ");
    print_str(2, msg);
    print_str(2, "\n");
    _exit(1);
}

static void wait_sem(sem_t *s) {
    while (sem_wait(s) == -1) {
        if (errno != EINTR) {
            fail("sem_wait failed");
        }
    }
}

static shm_t *open_shm(const char *name) {
    int fd = shm_open(name, O_RDWR, 0);
    if (fd == -1) {
        fail("shm_open failed");
    }
    shm_t *p = mmap(NULL, sizeof(shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        fail("mmap failed");
    }
    close(fd);
    return p;
}

static int starts_with_capital(const char *s, int len) {
    unsigned char c;
    unsigned char d;

    if (len < 1) {
        return 0;
    }
    c = (unsigned char)s[0];
    if (c >= 'A' && c <= 'Z') {
        return 1;
    }
    if (len >= 2 && c == 0xD0) {
        d = (unsigned char)s[1];
        if (d == 0x81 || (d >= 0x90 && d <= 0xAF)) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fail("expected 4 arguments");
    }

    shm_t *in = open_shm(argv[1]);
    shm_t *out = open_shm(argv[2]);

    sem_t *sem_in = sem_open(argv[3], 0);
    if (sem_in == SEM_FAILED) {
        fail("sem_open (in) failed");
    }
    sem_t *sem_out = sem_open(argv[4], 0);
    if (sem_out == SEM_FAILED) {
        fail("sem_open (out) failed");
    }

    const char *prefix = "Error: string must start with a capital letter: ";

    while (1) {
        wait_sem(sem_in);

        if (in->len < 0) {
            break;
        }

        if (starts_with_capital(in->data, in->len)) {
            write(1, in->data, in->len);
            write(1, "\n", 1);
            out->len = 0;
        } else {
            int p = 0;
            int i = 0;
            while (prefix[i] != '\0') {
                out->data[p++] = prefix[i++];
            }
            for (i = 0; i < in->len && p < BUF_SIZE; i++) {
                out->data[p++] = in->data[i];
            }
            out->len = p;
        }

        sem_post(sem_out);
    }

    munmap(in, sizeof(shm_t));
    munmap(out, sizeof(shm_t));
    sem_close(sem_in);
    sem_close(sem_out);
    return 0;
}