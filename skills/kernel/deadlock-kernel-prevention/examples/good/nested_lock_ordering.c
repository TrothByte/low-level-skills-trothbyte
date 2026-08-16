/* GOOD: nested locking with a real static hierarchy, mirroring
 * mutex_lock_nested(BD_MUTEX_WHOLE / BD_MUTEX_PARTITION): a whole-object
 * lock is always acquired before the partition sub-lock, and the subclass
 * is derived from the object type, not invented. Consistent order + true
 * hierarchy means lockdep can validate the pattern.
 * Compile+run: gcc -Wall -Wextra -Werror -O2 -pthread nested_lock_ordering.c
 *   -o /tmp/d3.exe && /tmp/d3.exe
 */
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

typedef enum {
    BD_MUTEX_NORMAL = 0,
    BD_MUTEX_WHOLE = 1,
    BD_MUTEX_PARTITION = 2
} lock_subclass_t;

typedef struct {
    pthread_mutex_t lock;
    lock_subclass_t subclass;
} nested_mutex_t;

#define NESTED_MUTEX_INIT(s) { PTHREAD_MUTEX_INITIALIZER, (s) }

static nested_mutex_t whole  = NESTED_MUTEX_INIT(BD_MUTEX_WHOLE);
static nested_mutex_t part0  = NESTED_MUTEX_INIT(BD_MUTEX_PARTITION);
static nested_mutex_t part1  = NESTED_MUTEX_INIT(BD_MUTEX_PARTITION);
static unsigned long reads;

static void open_partition(nested_mutex_t *p)
{
    /* hierarchy: whole is always above partition */
    pthread_mutex_lock(&whole.lock);
    pthread_mutex_lock(&p->lock);
    reads++;
    pthread_mutex_unlock(&p->lock);
    pthread_mutex_unlock(&whole.lock);
}

static void *worker(void *arg)
{
    intptr_t id = (intptr_t)arg;
    for (int i = 0; i < 20000; i++) {
        open_partition(id % 2 == 0 ? &part0 : &part1);
    }
    return NULL;
}

int main(void)
{
    pthread_t t[4];
    for (intptr_t i = 0; i < 4; i++) {
        pthread_create(&t[i], NULL, worker, (void *)i);
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(t[i], NULL);
    }
    if (reads != 80000) {
        printf("BUG: reads=%lu expected=80000\n", reads);
        return 1;
    }
    printf("GOOD: static hierarchy (whole->partition), consistent, reads=%lu\n",
           reads);
    return 0;
}
