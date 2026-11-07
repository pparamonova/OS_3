#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"

static char name_shm_in[64];
static char name_shm_out[64];
static char name_sem_in[64];
static char name_sem_out[64];

static void cleanup_names(void) {
    shm_unlink(name_shm_in);
    shm_unlink(name_shm_out);
    sem_unlink(name_sem_in);
    sem_unlink(name_sem_out);
}

static void fail(const char *msg) {
    print_str(2, "server: ");
    print_str(2, msg);
    print_str(2, "\n");
    cleanup_names();
    _exit(1);
}

static void make_name(char *dst, int pid, const char *suffix) {
    char num[16];
    int n = 0;
    int p = 0;
    const char *pre = "/lab4_";

    if (pid == 0) {
        num[n++] = '0';
    }
    while (pid > 0) {
        num[n++] = '0' + pid % 10;
        pid /= 10;
    }
    while (*pre != '\0') {
        dst[p++] = *pre++;
    }
    while (n > 0) {
        dst[p++] = num[--n];
    }
    dst[p++] = '_';
    while (*suffix != '\0') {
        dst[p++] = *suffix++;
    }
    dst[p] = '\0';
}

static int read_line(char *buf, int max) {
    int len = 0;
    char c;
    int r;

    while (1) {
        r = read(0, &c, 1);
        if (r == -1) {
            if (errno == EINTR) {
                continue;
            }
            fail("read from stdin failed");
        }
        if (r == 0) {
            if (len == 0) {
                return -1;
            }
            return len;
        }
        if (c == '\n') {
            return len;
        }
        if (len < max) {
            buf[len++] = c;
        }
    }
}

static void wait_sem(sem_t *s) {
    while (sem_wait(s) == -1) {
        if (errno != EINTR) {
            fail("sem_wait failed");
        }
    }
}

static shm_t *create_shm(const char *name) {
    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1) {
        fail("shm_open failed");
    }
    if (ftruncate(fd, sizeof(shm_t)) == -1) {
        fail("ftruncate failed");
    }
    shm_t *p = mmap(NULL, sizeof(shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        fail("mmap failed");
    }
    close(fd);
    return p;
}

int main(void) {
    char fname[256];
    int pid = getpid();

    print_str(1, "Enter file name: ");
    int n = read_line(fname, sizeof(fname) - 1);
    if (n <= 0) {
        print_str(2, "server: file name is empty\n");
        return 1;
    }
    fname[n] = '\0';

    make_name(name_shm_in, pid, "in");
    make_name(name_shm_out, pid, "out");
    make_name(name_sem_in, pid, "sem_in");
    make_name(name_sem_out, pid, "sem_out");

    shm_t *in = create_shm(name_shm_in);
    shm_t *out = create_shm(name_shm_out);

    sem_t *sem_in = sem_open(name_sem_in, O_CREAT | O_EXCL, 0600, 0);
    if (sem_in == SEM_FAILED) {
        fail("sem_open (in) failed");
    }
    sem_t *sem_out = sem_open(name_sem_out, O_CREAT | O_EXCL, 0600, 0);
    if (sem_out == SEM_FAILED) {
        fail("sem_open (out) failed");
    }

    in->len = 0;
    out->len = 0;

    int file = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == -1) {
        fail("cannot open output file");
    }

    pid_t child = fork();
    if (child == -1) {
        fail("fork failed");
    }
    if (child == 0) {
        if (dup2(file, 1) == -1) {
            print_str(2, "client: dup2 failed\n");
            _exit(1);
        }
        close(file);
        execl("./client", "client", name_shm_in, name_shm_out,
              name_sem_in, name_sem_out, (char *)NULL);
        print_str(2, "server: exec failed\n");
        _exit(1);
    }
    close(file);

    while (1) {
        int len = read_line(in->data, BUF_SIZE);
        if (len < 0) {
            break;
        }
        in->len = len;
        sem_post(sem_in);
        wait_sem(sem_out);

        if (out->len > 0) {
            write(1, out->data, out->len);
            write(1, "\n", 1);
        }
    }

    in->len = -1;
    sem_post(sem_in);
    waitpid(child, NULL, 0);

    munmap(in, sizeof(shm_t));
    munmap(out, sizeof(shm_t));
    sem_close(sem_in);
    sem_close(sem_out);
    cleanup_names();
    return 0;
}