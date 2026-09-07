/*
 * Linux/glibc-only diagnostic, deliberately separate from primary timings.
 * Build: cc -std=c11 -O2 -fPIC -shared -Wall -Wextra -Werror \
 *           allocation_probe.c -o allocation_probe.so
 * Run: KUMWE_BENCH_ALLOCATION_FILE=/absolute/unique/output.json \
 *        LD_PRELOAD=/absolute/allocation_probe.so USE_ZEND_ALLOC=0 php ...
 *
 * Counts successful, interposed malloc/calloc/realloc calls and their requested
 * bytes. A non-NULL zero-size result counts as a call and zero bytes. Realloc
 * counts its entire requested size, including in-place success; a NULL result
 * (including glibc realloc(p, 0)) does not count. Free is forwarded unchanged.
 * These are cumulative allocation requests, NOT live/peak heap, freed bytes,
 * usable allocator capacity, RSS, or unique allocations. Calls internal to
 * glibc, alternate allocation APIs, and custom allocator arenas are excluded.
 * RTLD_DEEPBIND may bypass preload interception: the driver uses a separate
 * explicit deepbind diagnostic interposer and validates its recorded coverage.
 * PHP must use USE_ZEND_ALLOC=0 for this separate diagnostic; its results must
 * not be labelled as the default Zend allocator's allocation counts/timings.
 *
 * The JSON is a relaxed-atomic snapshot at this library's shutdown finalizer,
 * not after all other finalizers. End worker threads before normal exit for
 * deterministic totals. _exit, fatal signals, and failed output opens/writes
 * produce no usable diagnostic; the driver must require and validate the file.
 * Use a fresh output filename per process: children inherit the environment.
 * The output path is captured on load (PHP clears its environment at shutdown)
 * and must fit in 4096 bytes including its terminator; longer paths are refused.
 * Counters saturate at UINT64_MAX and set counters_overflowed on saturation.
 * Direct glibc entrypoints avoid dlsym/allocator initialization recursion. The
 * finalizer uses a bounded stack buffer and no stdio, formatting, or allocation.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>

#if !defined(__linux__) || !defined(__GLIBC__)
#error "allocation_probe requires Linux and glibc; other allocators are unsupported"
#endif

_Static_assert(sizeof(unsigned long long) == sizeof(uint64_t),
               "allocation_probe requires 64-bit unsigned long long");
_Static_assert(sizeof(size_t) <= sizeof(unsigned long long),
               "allocation_probe requires allocation sizes that fit in its counters");
_Static_assert(ATOMIC_LLONG_LOCK_FREE == 2 && ATOMIC_BOOL_LOCK_FREE == 2,
               "allocation_probe requires always-lock-free counters");

extern void *__libc_malloc(size_t size);
extern void *__libc_calloc(size_t count, size_t size);
extern void *__libc_realloc(void *pointer, size_t size);
extern void __libc_free(void *pointer);

typedef struct {
    _Atomic unsigned long long calls;
    _Atomic unsigned long long requested_bytes;
} allocation_counter;

static allocation_counter malloc_counter;
static allocation_counter calloc_counter;
static allocation_counter realloc_counter;
static _Atomic bool counters_overflowed;
static char allocation_output_path[4096];

__attribute__((constructor)) static void capture_allocation_output_path(void)
{
    int saved_errno = errno;
    const char *path = getenv("KUMWE_BENCH_ALLOCATION_FILE");
    if (path != NULL) {
        size_t position;
        for (position = 0; position < sizeof(allocation_output_path); ++position) {
            allocation_output_path[position] = path[position];
            if (path[position] == '\0') {
                break;
            }
        }
        if (position == sizeof(allocation_output_path)) {
            allocation_output_path[0] = '\0';
        }
    }
    errno = saved_errno;
}

static unsigned long long saturated_sum(unsigned long long left,
                                        unsigned long long right)
{
    if (right > ULLONG_MAX - left) {
        atomic_store_explicit(&counters_overflowed, true, memory_order_relaxed);
        return ULLONG_MAX;
    }
    return left + right;
}

static void add_counter(_Atomic unsigned long long *counter,
                        unsigned long long increment)
{
    unsigned long long previous = atomic_load_explicit(counter, memory_order_relaxed);
    unsigned long long next;
    do {
        next = saturated_sum(previous, increment);
    } while (!atomic_compare_exchange_weak_explicit(counter, &previous, next,
                                                   memory_order_relaxed,
                                                   memory_order_relaxed));
}

static void record_success(allocation_counter *counter, size_t requested_bytes)
{
    add_counter(&counter->calls, 1);
    add_counter(&counter->requested_bytes, (unsigned long long)requested_bytes);
}

void *malloc(size_t size)
{
    void *result = __libc_malloc(size);
    int saved_errno = errno;
    if (result != NULL) {
        record_success(&malloc_counter, size);
    }
    errno = saved_errno;
    return result;
}

void *calloc(size_t count, size_t size)
{
    void *result = __libc_calloc(count, size);
    int saved_errno = errno;
    if (result != NULL) {
        /* A successful glibc calloc cannot overflow its size_t product. */
        record_success(&calloc_counter, count * size);
    }
    errno = saved_errno;
    return result;
}

void *realloc(void *pointer, size_t size)
{
    void *result = __libc_realloc(pointer, size);
    int saved_errno = errno;
    if (result != NULL) {
        record_success(&realloc_counter, size);
    }
    errno = saved_errno;
    return result;
}

void free(void *pointer)
{
    int saved_errno = errno;
    __libc_free(pointer);
    errno = saved_errno;
}

typedef struct {
    char data[2048];
    size_t length;
    bool failed;
} json_buffer;

static void append_char(json_buffer *buffer, char byte)
{
    if (buffer->length == sizeof(buffer->data)) {
        buffer->failed = true;
        return;
    }
    buffer->data[buffer->length++] = byte;
}

static void append_text(json_buffer *buffer, const char *text)
{
    while (*text != '\0') {
        append_char(buffer, *text++);
    }
}

static void append_number(json_buffer *buffer, unsigned long long number)
{
    char digits[20];
    size_t length = 0;
    do {
        digits[length++] = (char)('0' + number % 10);
        number /= 10;
    } while (number != 0);
    while (length != 0) {
        append_char(buffer, digits[--length]);
    }
}

static void append_api(json_buffer *buffer, const char *name,
                       unsigned long long calls, unsigned long long bytes)
{
    append_text(buffer, "\"");
    append_text(buffer, name);
    append_text(buffer, "\":{\"successful_allocation_calls\":");
    append_number(buffer, calls);
    append_text(buffer, ",\"requested_allocation_bytes\":");
    append_number(buffer, bytes);
    append_char(buffer, '}');
}

__attribute__((destructor)) static void emit_allocation_diagnostic(void)
{
    int saved_errno = errno;
    if (allocation_output_path[0] == '\0') {
        errno = saved_errno;
        return;
    }

    unsigned long long malloc_calls = atomic_load_explicit(&malloc_counter.calls, memory_order_relaxed);
    unsigned long long malloc_bytes = atomic_load_explicit(&malloc_counter.requested_bytes, memory_order_relaxed);
    unsigned long long calloc_calls = atomic_load_explicit(&calloc_counter.calls, memory_order_relaxed);
    unsigned long long calloc_bytes = atomic_load_explicit(&calloc_counter.requested_bytes, memory_order_relaxed);
    unsigned long long realloc_calls = atomic_load_explicit(&realloc_counter.calls, memory_order_relaxed);
    unsigned long long realloc_bytes = atomic_load_explicit(&realloc_counter.requested_bytes, memory_order_relaxed);
    unsigned long long total_calls = saturated_sum(saturated_sum(malloc_calls, calloc_calls), realloc_calls);
    unsigned long long total_bytes = saturated_sum(saturated_sum(malloc_bytes, calloc_bytes), realloc_bytes);
    json_buffer buffer = { .length = 0, .failed = false };
    append_text(&buffer, "{\"format\":\"kumwe-glibc-allocation-diagnostic/1\","
                         "\"scope\":\"interposed-calls-at-probe-finalizer\","
                         "\"successful_allocation_calls\":");
    append_number(&buffer, total_calls);
    append_text(&buffer, ",\"requested_allocation_bytes\":");
    append_number(&buffer, total_bytes);
    append_text(&buffer, ",\"by_api\":{");
    append_api(&buffer, "malloc", malloc_calls, malloc_bytes);
    append_char(&buffer, ',');
    append_api(&buffer, "calloc", calloc_calls, calloc_bytes);
    append_char(&buffer, ',');
    append_api(&buffer, "realloc", realloc_calls, realloc_bytes);
    append_text(&buffer, "},\"counters_overflowed\":");
    append_text(&buffer, atomic_load_explicit(&counters_overflowed, memory_order_relaxed)
                         ? "true}\n" : "false}\n");

    if (!buffer.failed) {
        int output = open(allocation_output_path,
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
