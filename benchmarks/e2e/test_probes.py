#!/usr/bin/env python3
"""Build and verify the Linux/glibc diagnostics using real allocator/loader calls."""

import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


FIXTURE = r"""
#include <errno.h>
#include <stdlib.h>
int known_allocations(int enabled) {
    if (!enabled) return 0;
    errno = EDOM;
    void *a = malloc(11);
    void *b = calloc(3, 7);
    if (!a || !b || errno != EDOM) return 1;
    void *c = realloc(a, 17);
    if (!c || errno != EDOM) return 2;
    free(c); free(b);
    return errno == EDOM ? 0 : 3;
}
"""

LOADER = r"""
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
int main(int argc, char **argv) {
    if (argc < 2) return 1;
    errno = ERANGE;
    void *handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    if (!handle || errno != ERANGE) return 2;
    int (*work)(int) = (int (*)(int))dlsym(handle, "known_allocations");
    if (!work) return 3;
    int result = work(argc > 2);
    return dlclose(handle) == 0 ? result : 4;
}
"""

THREADS = r"""
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
static uintptr_t iterations;
static void *worker(void *unused) {
    (void)unused;
    for (uintptr_t i = 0; i < iterations; ++i) {
        void *a = malloc(1);
        void *b = calloc(2, 3);
        if (!a || !b) return (void *)1;
        void *c = realloc(a, 10);
        if (!c) return (void *)1;
        free(c); free(b);
    }
    return NULL;
}
int main(int argc, char **argv) {
    (void)argv;
    iterations = argc > 1 ? 100000 : 0;
    pthread_t threads[4];
    for (int i = 0; i < 4; ++i)
        if (pthread_create(&threads[i], NULL, worker, NULL)) return 1;
    for (int i = 0; i < 4; ++i) {
        void *result;
        if (pthread_join(threads[i], &result) || result != NULL) return 2;
    }
    return 0;
}
"""


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    source = Path(__file__).resolve().parent
    compiler = shlex.split(os.environ.get("CC", "cc"))
    flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
    with tempfile.TemporaryDirectory(prefix="kumwe-probe-test-") as directory:
        root = Path(directory)

        def build(name, source_file, *extra):
            target = root / name
            subprocess.run(compiler + flags + [str(source_file), *extra, "-o", str(target)],
                           check=True, timeout=30)
            return target

        allocation = build("allocation.so", source / "allocation_probe.c", "-fPIC", "-shared")
        deepbind = build("deepbind.so", source / "deepbind_probe.c",
                         "-fPIC", "-shared", "-pthread", "-ldl")
        for name, content in (("fixture", FIXTURE), ("loader", LOADER), ("threads", THREADS)):
            (root / (name + ".c")).write_text(content)
        fixture = build("fixture.so", root / "fixture.c", "-fno-builtin", "-fPIC", "-shared")
        loader = build("loader", root / "loader.c", "-ldl")
        threads = build("threads", root / "threads.c", "-fno-builtin", "-pthread")

        def run(name, command, strip):
            allocation_file = root / (name + "-allocation.json")
            deepbind_file = root / (name + "-deepbind.json")
            environment = os.environ.copy()
            environment.update(
                LD_PRELOAD=f"{allocation}:{deepbind}",
                KUMWE_BENCH_ALLOCATION_FILE=str(allocation_file),
                KUMWE_BENCH_DEEPBIND_FILE=str(deepbind_file),
                KUMWE_BENCH_DISABLE_DEEPBIND=str(int(strip)),
            )
            subprocess.run([str(part) for part in command], env=environment,
                           check=True, timeout=30)
            alloc = json.loads(allocation_file.read_text())
            loads = json.loads(deepbind_file.read_text())
            check(alloc["format"] == "kumwe-glibc-allocation-diagnostic/1", alloc)
            check(loads["format"] == "kumwe-glibc-deepbind-diagnostic/1", loads)
            check(not alloc["counters_overflowed"] and not loads["counters_overflowed"],
                  (alloc, loads))
            for metric in ("successful_allocation_calls", "requested_allocation_bytes"):
                check(alloc[metric] == sum(api[metric] for api in alloc["by_api"].values()), alloc)
            return alloc, loads

        def check_delta(empty, full, calls, sizes):
            for api, size in sizes.items():
                for metric, expected in (("successful_allocation_calls", calls),
                                         ("requested_allocation_bytes", calls * size)):
                    actual = full["by_api"][api][metric] - empty["by_api"][api][metric]
                    check(actual == expected, (api, metric, actual, expected))
            check(full["successful_allocation_calls"] - empty["successful_allocation_calls"]
                  == calls * len(sizes), (empty, full))
            check(full["requested_allocation_bytes"] - empty["requested_allocation_bytes"]
                  == calls * sum(sizes.values()), (empty, full))

        for strip in (False, True):
            allocations = []
            for work in (False, True):
                command = [loader, fixture] + (["work"] if work else [])
                alloc, loads = run(f"loader-{int(strip)}-{int(work)}", command, strip)
                check(loads["total_dlopen_calls"] == loads["requested_deepbind_calls"]
                      == loads["successful_dlopen_calls"] == 1, loads)
                check(loads["stripped_deepbind_calls"] == int(strip), loads)
                check(loads["disable_deepbind_enabled"] == strip, loads)
                allocations.append(alloc)
            check_delta(*allocations, int(strip), {"malloc": 11, "calloc": 21, "realloc": 17})
            print(f"DEEPBIND stripping={strip}: {3 * int(strip)} calls / "
                  f"{49 * int(strip)} requested bytes; exact per-API deltas and errno PASS")

        empty, _ = run("threads-empty", [threads], False)
        full, _ = run("threads-full", [threads, "work"], False)
        check_delta(empty, full, 400000, {"malloc": 1, "calloc": 6, "realloc": 10})
        print("Four joined threads: 1200000 calls / 6800000 requested bytes; "
              "exact per-API deltas PASS")


if __name__ == "__main__":
    main()
