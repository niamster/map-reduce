#ifndef _MR_UTILS_H_
#define _MR_UTILS_H_

#include <stdbool.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARRAY_SIZE(x) ((unsigned)(sizeof(x)/sizeof(x[0])))

void die(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
bool is_sep(char c);

int pthread_mutex_init_ec(pthread_mutex_t *mtx);

static inline void __mtx_lock(pthread_mutex_t *mtx) {
    int err = pthread_mutex_lock(mtx);
    if (err != 0)
        die("Failed to lock TP: %s", strerror(err));
}

static inline void __mtx_unlock(pthread_mutex_t *mtx) {
    int err = pthread_mutex_unlock(mtx);
    if (err != 0)
        die("Failed to unlock TP: %s", strerror(err));
}

static inline void __cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx) {
    int err = pthread_cond_wait(cond, mtx);
    if (err != 0)
        die("Failed to wait on TP: %s", strerror(err));
}

static inline void __cond_notify(pthread_cond_t *cond) {
    int err = pthread_cond_signal(cond);
    if (err != 0)
        die("Failed to signal on TP: %s", strerror(err));
}


typedef struct {
    unsigned index;
    off_t offset;
    const char *mem;
    size_t count;
} fchunk_t;

typedef void (*fchunk_cb_t)(fchunk_t *chunk, void *user);

int fchunk_read_fd(int fd, unsigned max, fchunk_cb_t cb, void *user, void **map, size_t *len);
int fchunk_read(const char *mem, size_t size, unsigned max, fchunk_cb_t cb, void *user);
void fchunk_read_word(const char *mem, size_t size, fchunk_cb_t cb, void *user);

#ifdef __cplusplus
};
#endif

#endif
