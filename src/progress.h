#pragma once

// Utilites to track progress

#include <linux/futex.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
namespace progress {

struct thread_args {
    int volatile *progressp;
    uint32_t total_count;

    constexpr thread_args(int volatile *progressp, uint32_t total_count)
        : progressp(progressp), total_count(total_count) {}
};

[[gnu::noinline]] void *reporter_thread(void *start) {
    auto *progp = reinterpret_cast<thread_args *>(start);
    thread_args prog = *progp;
    delete progp;

    printf("Progress thread started @ %p, target = %u\n", prog.progressp,
           prog.total_count);

    // NOTE: this thread will get cancelled by the main one.
    for (;;) {
        int prog_read = __atomic_load_n(prog.progressp, __ATOMIC_ACQUIRE);
        printf("\r\x1b[2KProgress: %u/%u", prog_read, prog.total_count);
        fflush(stdout);

        // Wait till it's updated
        syscall(SYS_futex, prog.progressp, FUTEX_WAIT_PRIVATE, prog_read,
                NULL /* timeout */);
    }
}

struct progress_state {
    int volatile __attribute__((aligned(64))) progress;
    pthread_t reporter_handle = 0;

    constexpr progress_state() {}

    constexpr bool is_valid() const & { return reporter_handle > 0; }

    void manual_teardown() {
        if (is_valid()) {
            pthread_cancel(reporter_handle);
        }
    }

    // Same return as `pthread_create`
    int setup(uint32_t total_count) {
        progress = 0;
        auto *args = new thread_args(&progress, total_count);
        int res = pthread_create(&reporter_handle, NULL, reporter_thread,
                                 (void *)args);

        return res;
    }

    void increment(int count = 1) {
        __atomic_fetch_add(&progress, count, __ATOMIC_ACQ_REL);
        syscall(SYS_futex, &progress, FUTEX_WAKE_PRIVATE, 1);
    }

    ~progress_state() { manual_teardown(); }
};

}  // namespace progress
