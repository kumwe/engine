/*
 * Linux/glibc diagnostic-only companion to allocation_probe.c. Never preload
 * this shim into the primary timing process: changing symbol lookup is itself
 * an instrumentation change. Build with:
 *   cc -std=c11 -O2 -fPIC -shared -Wall -Wextra -Werror -pthread \
 *      deepbind_probe.c -ldl -o deepbind_probe.so
 *
 * Only KUMWE_BENCH_DISABLE_DEEPBIND=1 enables removal of RTLD_DEEPBIND from
 * interposed dlopen calls. Every other flag and the filename pass unchanged.
 * KUMWE_BENCH_DEEPBIND_FILE names a fresh JSON output file for this process.
 * Configuration is captured on load, before PHP clears its shutdown environment.
 * Paths must fit in 4096 bytes including their terminator. The bounded finalizer
 * uses no allocation or stdio. Require a complete valid file in the driver;
 * abnormal exit and failed output writes do not yield a usable measurement.
 *
 * Counts include failed load attempts and are a snapshot at this shim's
 * finalizer. End worker threads before exit for reproducible snapshots. This
 * interposes dlopen, not dlmopen or calls bound directly inside another library.
 * Children inherit configuration and counters; benchmark processes must not
 * fork. Direct dlsym(RTLD_NEXT) resolution is performed once, before forwarding.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>

#if !defined(__linux__) || !defined(__GLIBC__) || !defined(RTLD_DEEPBIND)
#error "deepbind_probe requires Linux/glibc with RTLD_DEEPBIND"
#endif

_Static_assert(sizeof(unsigned long long) == sizeof(uint64_t),
               "deepbind_probe requires 64-bit unsigned long long");
_Static_assert(ATOMIC_LLONG_LOCK_FREE == 2 && ATOMIC_BOOL_LOCK_FREE == 2,
               "deepbind_probe requires always-lock-free counters");

typedef void *(*dlopen_function)(const char *, int);
static dlopen_function real_dlopen;
static pthread_once_t initialization = PTHREAD_ONCE_INIT;
static bool disable_deepbind;
static char output_path[4096];
static _Atomic unsigned long long total_calls;
static _Atomic unsigned long long deepbind_calls;
static _Atomic unsigned long long stripped_calls;
static _Atomic unsigned long long successful_calls;
static _Atomic bool counters_overflowed;

static void initialize_probe(void)
{
    int saved_errno = errno;
    const char *setting = getenv("KUMWE_BENCH_DISABLE_DEEPBIND");
    disable_deepbind = setting != NULL && setting[0] == '1' && setting[1] == '\0';
    const char *path = getenv("KUMWE_BENCH_DEEPBIND_FILE");
    if (path != NULL) {
        size_t position;
        for (position = 0; position < sizeof(output_path); ++position) {
            output_path[position] = path[position];
            if (path[position] == '\0') {
                break;
            }
        }
        if (position == sizeof(output_path)) {
            output_path[0] = '\0';
        }
    }
    real_dlopen = (dlopen_function)dlsym(RTLD_NEXT, "dlopen");
    errno = saved_errno;
}

__attribute__((constructor)) static void start_probe(void)
{
    int saved_errno = errno;
    (void)pthread_once(&initialization, initialize_probe);
    errno = saved_errno;
}

static void increment(_Atomic unsigned long long *counter)
{
    unsigned long long previous = atomic_load_explicit(counter, memory_order_relaxed);
    do {
        if (previous == ULLONG_MAX) {
            atomic_store_explicit(&counters_overflowed, true, memory_order_relaxed);
            return;
        }
    } while (!atomic_compare_exchange_weak_explicit(counter, &previous, previous + 1,
                                                   memory_order_relaxed,
                                                   memory_order_relaxed));
}

void *dlopen(const char *filename, int flags)
{
    int entry_errno = errno;
    (void)pthread_once(&initialization, initialize_probe);
    increment(&total_calls);
    if ((flags & RTLD_DEEPBIND) != 0) {
        increment(&deepbind_calls);
        if (disable_deepbind) {
            flags &= ~RTLD_DEEPBIND;
            increment(&stripped_calls);
        }
    }
    if (real_dlopen == NULL) {
        errno = ENOSYS;
        return NULL;
    }
    errno = entry_errno;
    void *result = real_dlopen(filename, flags);
    int saved_errno = errno;
    if (result != NULL) {
        increment(&successful_calls);
    }
    errno = saved_errno;
    return result;
}

typedef struct {
    char data[1024];
    size_t length;
    bool failed;
} json_buffer;

static void append_text(json_buffer *buffer, const char *text)
{
    while (*text != '\0') {
        if (buffer->length == sizeof(buffer->data)) {
            buffer->failed = true;
            return;
        }
        buffer->data[buffer->length++] = *text++;
    }
}

static void append_counter(json_buffer *buffer, const char *name,
                           _Atomic unsigned long long *counter)
{
    char digits[21];
    size_t position = sizeof(digits) - 1;
    digits[position] = '\0';
    unsigned long long number = atomic_load_explicit(counter, memory_order_relaxed);
    do {
        digits[--position] = (char)('0' + number % 10);
        number /= 10;
    } while (number != 0);
    append_text(buffer, ",\"");
    append_text(buffer, name);
    append_text(buffer, "\":");
    append_text(buffer, digits + position);
}

__attribute__((destructor)) static void emit_diagnostic(void)
{
    int saved_errno = errno;
    if (output_path[0] == '\0') {
        errno = saved_errno;
        return;
    }
    json_buffer buffer = { .length = 0, .failed = false };
    append_text(&buffer, "{\"format\":\"kumwe-glibc-deepbind-diagnostic/1\","
                         "\"scope\":\"interposed-dlopen-calls-at-probe-finalizer\","
                         "\"disable_deepbind_enabled\":");
    append_text(&buffer, disable_deepbind ? "true" : "false");
    append_counter(&buffer, "total_dlopen_calls", &total_calls);
    append_counter(&buffer, "requested_deepbind_calls", &deepbind_calls);
    append_counter(&buffer, "stripped_deepbind_calls", &stripped_calls);
    append_counter(&buffer, "successful_dlopen_calls", &successful_calls);
    append_text(&buffer, ",\"counters_overflowed\":");
    append_text(&buffer, atomic_load_explicit(&counters_overflowed, memory_order_relaxed)
                         ? "true}\n" : "false}\n");
    if (!buffer.failed) {
        int output = open(output_path,
                          O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (output >= 0) {
            size_t written = 0;
            while (written < buffer.length) {
                ssize_t count = write(output, buffer.data + written, buffer.length - written);
                if (count < 0 && errno == EINTR) {
                    continue;
                }
                if (count <= 0) {
                    break;
                }
                written += (size_t)count;
            }
            (void)close(output);
        }
    }
    errno = saved_errno;
}
