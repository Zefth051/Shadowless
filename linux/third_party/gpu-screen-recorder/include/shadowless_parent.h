#ifndef SHADOWLESS_PARENT_H
#define SHADOWLESS_PARENT_H
#include <signal.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
/* Do not keep capturing when Shadowless has forcibly stopped its launcher. */
static inline void shadowless_bind_parent(pid_t expected_parent) {
#ifdef __linux__
    const pid_t parent = getppid();
    if(parent == 1 || (expected_parent != 0 && parent != expected_parent) ||
        prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || getppid() != parent)
        _exit(125);
#endif
}
#endif
