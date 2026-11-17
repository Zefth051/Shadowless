// Intercept only the external command boundary; never reload the host's udev rules.
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unistd.h>

extern "C" ssize_t __real_write(int, const void *, size_t);
extern "C" ssize_t __wrap_write(int fd, const void *buffer, size_t size) {
    if (access("fail-write", F_OK) == 0) {
        errno = ENOSPC;
        return -1;
    }
    return __real_write(fd, buffer, size);
}

extern "C" int __wrap_execve(const char *path, char *const argv[], char *const env[]) {
    // The ACL revoke step runs <root>/usr/bin/setfacl with an empty environment; record it instead of executing anything.
    const size_t length = std::strlen(path), suffix = std::strlen("/usr/bin/setfacl");
    if (env != nullptr && env[0] == nullptr && length > suffix && std::strcmp(path + length - suffix, "/usr/bin/setfacl") == 0) {
        std::string invocation;
        for (int i = 1; argv[i]; ++i) invocation += std::string(i > 1 ? " " : "") + argv[i];
        invocation += '\n';
        const int log = open("setfacl.log", O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (log < 0 || write(log, invocation.data(), invocation.size()) != static_cast<ssize_t>(invocation.size())) _exit(94);
        close(log);
        _exit(0);
    }
    if (env == nullptr || env[0] != nullptr ||
        (std::strcmp(path, "/usr/bin/udevadm") != 0 && std::strcmp(path, "/bin/udevadm") != 0)) _exit(91);
    if (access("fallback", F_OK) == 0 && std::strcmp(path, "/usr/bin/udevadm") == 0) {
        errno = ENOENT;
        return -1;
    }
    std::string invocation;
    int count = 0;
    for (; argv[count]; ++count) {
        if (count) invocation += ' ';
        invocation += argv[count];
    }
    const bool control = count == 3 && invocation == "udevadm control --reload-rules";
    const bool trigger = count == 4 && invocation == "udevadm trigger --action=change --subsystem-match=input";
    if (!control && !trigger) _exit(92);
    const int fd = open("udev.log", O_WRONLY | O_CREAT | O_APPEND, 0600);
    invocation += '\n';
    if (fd < 0 || write(fd, invocation.data(), invocation.size()) != static_cast<ssize_t>(invocation.size())) _exit(93);
    close(fd);
    if ((control && access("fail-reload", F_OK) == 0) ||
        (trigger && access("fail-trigger", F_OK) == 0)) _exit(1);
    _exit(0);
}
