#define _POSIX_C_SOURCE 200809L

#include "reboot_handoff.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef HOST_BUILD
static bool init_is_busybox(void) {
    char executable[256];
    ssize_t length = readlink("/proc/1/exe", executable, sizeof(executable) - 1);
    if (length <= 0 || length >= (ssize_t)sizeof(executable) - 1) return false;
    executable[length] = '\0';
    const char *name = strrchr(executable, '/');
    return strcmp(name ? name + 1 : executable, "busybox") == 0;
}

/* True when "/" comes from a loop or ram device, which is how the multi-boot
 * image runs CompasOS: the root filesystem is a squashfs on the card, mounted
 * through a loop device.
 *
 * That matters here because the init answering the request would have to
 * unmount the medium it is standing on before it can shut down, and that takes
 * many seconds during which nothing happens on screen. Reboot directly instead;
 * the card is mounted read-only for the root and sd_fsck repairs it if needed.
 */
static bool root_is_loop_backed(void) {
    FILE * mounts = fopen("/proc/mounts", "r");
    if (!mounts) return false;
    bool loop = false;
    char line[512];
    while (fgets(line, sizeof line, mounts)) {
        char device[128], point[128];
        if (sscanf(line, "%127s %127s", device, point) != 2) continue;
        if (strcmp(point, "/") != 0) continue;
        loop = strncmp(device, "/dev/loop", 9) == 0 || strncmp(device, "/dev/ram", 8) == 0;
        break;
    }
    fclose(mounts);
    return loop;
}

static void wait_for_init_shutdown(void) {
    struct timespec deadline;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return;
    deadline.tv_sec += 20;
    for (;;) {
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return;
        struct timespec remaining = {
            .tv_sec = deadline.tv_sec - now.tv_sec,
            .tv_nsec = deadline.tv_nsec - now.tv_nsec,
        };
        if (remaining.tv_nsec < 0) {
            remaining.tv_sec--;
            remaining.tv_nsec += 1000000000L;
        }
        if (remaining.tv_sec < 0 || (remaining.tv_sec == 0 && remaining.tv_nsec == 0)) return;
        if (nanosleep(&remaining, NULL) == 0) return;
        if (errno != EINTR) return;
    }
}
#endif

reboot_handoff_result_t reboot_request_orderly_shutdown(void (*before_request)(void)) {
#ifdef HOST_BUILD
    (void)before_request;
    errno = ENOSYS;
    return REBOOT_HANDOFF_UNAVAILABLE;
#else
    if (root_is_loop_backed()) {
        /* Card-rooted: asking init to shut down would stall for seconds. */
        errno = ENOSYS;
        return REBOOT_HANDOFF_UNAVAILABLE;
    }
    if (!init_is_busybox()) {
        errno = ENOSYS;
        return REBOOT_HANDOFF_UNAVAILABLE;
    }
    struct sigaction action = {0};
    sigset_t unblocked;
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    sigemptyset(&unblocked);
    sigaddset(&unblocked, SIGTERM);
    /* Reset the inherited disposition and unblock SIGTERM before asking init
     * to terminate its process set, which includes this caller. */
    if (sigaction(SIGTERM, &action, NULL) != 0 || sigprocmask(SIG_UNBLOCK, &unblocked, NULL) != 0)
        return REBOOT_HANDOFF_SIGNAL_SETUP_FAILED;
    if (before_request) before_request();
    if (kill(1, SIGTERM) != 0) return REBOOT_HANDOFF_REQUEST_FAILED;
    wait_for_init_shutdown();
    return REBOOT_HANDOFF_REQUESTED;
#endif
}
