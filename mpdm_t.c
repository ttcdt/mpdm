/*

    MPDM - Minimum Profit Data Manager
    mpdm_t.c - Threads, mutexes, semaphores and other stuff

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>

#ifdef CONFOPT_WIN32
#include <windows.h>
#endif

#ifdef CONFOPT_PTHREADS
#include <pthread.h>
#endif

#ifdef CONFOPT_POSIXSEMS
#include <semaphore.h>
#endif

#ifdef CONFOPT_GETTIMEOFDAY
#include <sys/time.h>
#endif

#ifdef CONFOPT_ZLIB
#include <zlib.h>
#endif

#include "mpdm.h"


/** code **/

/**
 * mpdm_sleep - Sleeps a number of milliseconds.
 * @msecs: the milliseconds to sleep
 *
 * Sleeps a number of milliseconds.
 * [Threading]
 */
void mpdm_sleep(int msecs)
{
#ifdef CONFOPT_WIN32

    Sleep(msecs);

#endif

#ifdef CONFOPT_NANOSLEEP
    struct timespec ts;

    ts.tv_sec = msecs / 1000;
    ts.tv_nsec = (msecs % 1000) * 1000000;

    nanosleep(&ts, NULL);
#endif
}


double mpdm_time(void)
{
    double r = 0.0;

#ifdef CONFOPT_GETTIMEOFDAY

    struct timeval tv;

    gettimeofday(&tv, NULL);

    r = ((double)tv.tv_sec) + (((double)tv.tv_usec) / 1000000.0);

#else /* CONFOPT_GETTIMEOFDAY */

    r = (double) time(NULL);

#endif /* CONFOPT_GETTIMEOFDAY */

    return r;
}


mpdm_t mpdm_random(mpdm_t v)
{
    static unsigned int seed = 0;
    mpdm_t r = NULL;

    if (mpdm_type(v) == MPDM_TYPE_ARRAY)
        r = mpdm_get(v, mpdm_random(MPDM_I(mpdm_size(v))));
    else {
        int range = mpdm_ival(v);
        int v = 0;

        /* crappy random seed */
        if (seed == 0) {
            time_t t = time(NULL);
            seed = t ^ (t << 16);
        }

        seed = (seed * 58321) + 11113;
        v = (seed >> 16);

        if (range)
            r = MPDM_I(v % range);
        else
            r = MPDM_R((v & 0xffff) / 65536.0);
    }

    return r;
}


/** mutexes **/

static mpdm_t vc_mutex_destroy(mpdm_t v)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) v->data;

    CloseHandle(*h);
#endif

#ifdef CONFOPT_PTHREADS
    pthread_mutex_t *m = (pthread_mutex_t *) v->data;

    pthread_mutex_destroy(m);
#endif

    v->data = NULL;

    return v;
}


/**
 * mpdm_new_mutex - Creates a new mutex.
 *
 * Creates a new mutex.
 * [Threading]
 */
mpdm_t mpdm_new_mutex(void)
{
    char *ptr = NULL;
    int size = 0;

#ifdef CONFOPT_WIN32
    HANDLE h;

    h = CreateMutex(NULL, FALSE, NULL);

    if (h != NULL) {
        size = sizeof(h);
        ptr = (char *) &h;
    }
#endif

#ifdef CONFOPT_PTHREADS
    pthread_mutex_t m;

    if (pthread_mutex_init(&m, NULL) == 0) {
        size = sizeof(m);
        ptr = (char *) &m;
    }

#endif

    return MPDM_C(MPDM_TYPE_MUTEX, ptr, size);
}


/**
 * mpdm_mutex_lock - Locks a mutex.
 * @mutex: the mutex to be locked
 *
 * Locks a mutex. If the mutex is not already locked,
 * it waits until it is.
 * [Threading]
 */
void mpdm_mutex_lock(mpdm_t mutex)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) mutex->data;

    WaitForSingleObject(*h, INFINITE);
#endif

#ifdef CONFOPT_PTHREADS
    pthread_mutex_t *m = (pthread_mutex_t *) mutex->data;

    pthread_mutex_lock(m);
#endif
}


/**
 * mpdm_mutex_unlock - Unlocks a mutex.
 * @mutex: the mutex to be unlocked
 *
 * Unlocks a previously locked mutex. The thread
 * unlocking the mutex must be the one who locked it.
 * [Threading]
 */
void mpdm_mutex_unlock(mpdm_t mutex)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) mutex->data;

    ReleaseMutex(*h);
#endif

#ifdef CONFOPT_PTHREADS
    pthread_mutex_t *m = (pthread_mutex_t *) mutex->data;

    pthread_mutex_unlock(m);
#endif
}


/** semaphores **/

static mpdm_t vc_semaphore_destroy(mpdm_t v)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) v->data;

    CloseHandle(*h);
#endif

#ifdef CONFOPT_POSIXSEMS
    sem_t *s = (sem_t *) v->data;

    sem_destroy(s);
#endif

    v->data = NULL;

    return v;
}


/**
 * mpdm_new_semaphore - Creates a new semaphore.
 * @init_value: the initial value of the semaphore.
 *
 * Creates a new semaphore with an @init_value.
 * [Threading]
 */
mpdm_t mpdm_new_semaphore(int init_value)
{
    char *ptr = NULL;
    int size = 0;

#ifdef CONFOPT_WIN32
    HANDLE h;

    if ((h = CreateSemaphore(NULL, init_value, 0x7fffffff, NULL)) != NULL) {
        size = sizeof(h);
        ptr = (char *) &h;
    }

#endif

#ifdef CONFOPT_POSIXSEMS
    sem_t s;

    if (sem_init(&s, 0, init_value) == 0) {
        size = sizeof(s);
        ptr = (char *) &s;
    }

#endif

    return MPDM_C(MPDM_TYPE_SEMAPHORE, ptr, size);
}


/**
 * mpdm_semaphore_wait - Waits for a semaphore to be ready.
 * @sem: the semaphore to wait onto
 *
 * Waits for the value of a semaphore to be > 0. If it's
 * not, the thread waits until it is.
 * [Threading]
 */
void mpdm_semaphore_wait(mpdm_t sem)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) sem->data;

    WaitForSingleObject(*h, INFINITE);
#endif

#ifdef CONFOPT_POSIXSEMS
    sem_t *s = (sem_t *) sem->data;

    sem_wait(s);
#endif
}


/**
 * mpdm_semaphore_post - Increments the value of a semaphore.
 * @sem: the semaphore to increment
 *
 * Increments by 1 the value of a semaphore.
 * [Threading]
 */
void mpdm_semaphore_post(mpdm_t sem)
{
#ifdef CONFOPT_WIN32
    HANDLE *h = (HANDLE *) sem->data;

    ReleaseSemaphore(*h, 1, NULL);
#endif

#ifdef CONFOPT_POSIXSEMS
    sem_t *s = (sem_t *) sem->data;

    sem_post(s);
#endif
}


/** threads **/

static mpdm_t vc_thread_destroy(mpdm_t v)
{
    v->data = NULL;

    return v;
}


static void thread_caller(mpdm_t a)
{
    /* ignore return value */
    mpdm_void(mpdm_exec(mpdm_get_i(a, 0), mpdm_get_i(a, 1), mpdm_get_i(a, 2)));

    /* was referenced in mpdm_exec_thread() */
    mpdm_unref(a);
}


#ifdef CONFOPT_WIN32
DWORD WINAPI win32_thread(LPVOID param)
{
    thread_caller((mpdm_t) param);

    return 0;
}
#endif

#ifdef CONFOPT_PTHREADS
void *pthreads_thread(void *args)
{
    thread_caller((mpdm_t) args);

    return NULL;
}
#endif

/**
 * mpdm_exec_thread - Runs an executable value in a new thread.
 * @c: the executable value
 * @args: executable arguments
 * @ctxt: the context
 *
 * Runs the @c executable value in a new thread. The code
 * starts executing immediately. The @args and @ctxt arguments
 * are sent to the executable value as arguments.
 *
 * Returns a handle for the thread.
 * [Threading]
 */
mpdm_t mpdm_exec_thread(mpdm_t c, mpdm_t args, mpdm_t ctxt)
{
    mpdm_t a;
    char *ptr = NULL;
    int size = 0;

    if (ctxt == NULL)
        ctxt = MPDM_A(0);

    /* to be unreferenced at thread stop */
    a = mpdm_ref(MPDM_A(3));

    mpdm_set_i(a, c, 0);
    mpdm_set_i(a, args, 1);
    mpdm_set_i(a, ctxt, 2);

#ifdef CONFOPT_WIN32
    HANDLE t;

    t = CreateThread(NULL, 0, win32_thread, a, 0, NULL);

    if (t != NULL) {
        size = sizeof(t);
        ptr = (char *) &t;
    }

#endif

#ifdef CONFOPT_PTHREADS
    pthread_t pt;

    if (pthread_create(&pt, NULL, pthreads_thread, a) == 0) {
        size = sizeof(pthread_t);
        ptr = (char *) &pt;
    }

#endif

    return MPDM_C(MPDM_TYPE_THREAD, ptr, size);
}


/* zlib functions */

unsigned char *mpdm_gzip_inflate(unsigned char *cbuf, size_t cz, size_t *dz)
{
    unsigned char *dbuf = NULL;

#ifdef CONFOPT_ZLIB

    if (cbuf[0] == 0x1f && cbuf[1] == 0x8b) {
        z_stream d_stream;
        int err;

        /* size % 2^32 is at the end */
        *dz = cbuf[cz - 1];
        *dz = (*dz * 256) + cbuf[cz - 2];
        *dz = (*dz * 256) + cbuf[cz - 3];
        *dz = (*dz * 256) + cbuf[cz - 4];

        dbuf = calloc(*dz + 1, 1);

        memset(&d_stream, '\0', sizeof(d_stream));

        d_stream.next_in   = cbuf;
        d_stream.avail_in  = cz;
        d_stream.next_out  = dbuf;
        d_stream.avail_out = *dz;

        if ((err = inflateInit2(&d_stream, 16 + MAX_WBITS)) == Z_OK) {
            while (err != Z_STREAM_END && err >= 0)
                err = inflate(&d_stream, Z_FINISH);

            err = inflateEnd(&d_stream);
        }

        if (err != Z_OK || *dz != d_stream.total_out)
            dbuf = realloc(dbuf, 0);
    }

#endif /* CONFOPT_ZLIB */

    return dbuf;
}


/* tar archives */

unsigned char *mpdm_read_tar_mem(const char *fn, const char *tar,
                                 const char *tar_e, size_t *z)
{
    unsigned char *data = NULL;

    while (!data && tar < tar_e && *tar) {
        sscanf(&tar[124], "%lo", (long *)z);

        if (strcmp(fn, tar) == 0)
            data = memcpy(calloc(*z + 1, 1), tar + 512, *z);
        else
            tar += (1 + ((*z + 511) / 512)) * 512;
    }

    return data;
}


unsigned char *mpdm_read_tar_file(const char *fn, FILE *f, size_t *z)
{
    unsigned char *data = NULL;
    char tar[512];

    while (!data && fread(tar, sizeof(tar), 1, f) && *tar) {
        sscanf(&tar[124], "%lo", (long *)z);

        if (strcmp(fn, tar) == 0) {
            data = calloc(*z + 1, 1);
            if (!fread(data, *z, 1, f))
                data = realloc(data, 0);
        }
        else
            fseek(f, ((*z + 511) / 512) * 512, 1);
    }

    return data;
}


/** data vc **/

struct mpdm_type_vc mpdm_vc_mutex = { /* VC */
    L"mutex",               /* name */
    vc_mutex_destroy,       /* destroy */
    vc_default_is_true,     /* is_true */
    vc_default_count,       /* count */
    vc_default_get_i,       /* get_i */
    vc_default_get,         /* get */
    vc_default_string,      /* string */
    vc_default_del_i,       /* del_i */
    vc_default_del,         /* del */
    vc_default_set_i,       /* set_i */
    vc_default_set,         /* set */
    vc_default_exec,        /* exec */
    vc_default_iterator,    /* iterator */
    vc_default_map          /* map */
};

struct mpdm_type_vc mpdm_vc_semaphore = { /* VC */
    L"semaphore",           /* name */
    vc_semaphore_destroy,   /* destroy */
    vc_default_is_true,     /* is_true */
    vc_default_count,       /* count */
    vc_default_get_i,       /* get_i */
    vc_default_get,         /* get */
    vc_default_string,      /* string */
    vc_default_del_i,       /* del_i */
    vc_default_del,         /* del */
    vc_default_set_i,       /* set_i */
    vc_default_set,         /* set */
    vc_default_exec,        /* exec */
    vc_default_iterator,    /* iterator */
    vc_default_map          /* map */
};

struct mpdm_type_vc mpdm_vc_thread = { /* VC */
    L"thread",              /* name */
    vc_thread_destroy,      /* destroy */
    vc_default_is_true,     /* is_true */
    vc_default_count,       /* count */
    vc_default_get_i,       /* get_i */
    vc_default_get,         /* get */
    vc_default_string,      /* string */
    vc_default_del_i,       /* del_i */
    vc_default_del,         /* del */
    vc_default_set_i,       /* set_i */
    vc_default_set,         /* set */
    vc_default_exec,        /* exec */
    vc_default_iterator,    /* iterator */
    vc_default_map          /* map */
};
