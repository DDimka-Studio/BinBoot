/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/* Test /init: proves the initrd was delivered and shows the kernel cmdline. */
#include <fcntl.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <unistd.h>

static void out(const char *s, size_t n)
{
    if (write(1, s, n) < 0)
        return;
}

int main(void)
{
    char buf[512];
    ssize_t n;
    int fd;

    mount("proc", "/proc", "proc", 0, 0);
    out("\nINITRD-OK\n", 11);
    fd = open("/proc/cmdline", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            out("CMDLINE: ", 9);
            out(buf, (size_t)n);
        }
        close(fd);
    }
    sync();
    reboot(RB_POWER_OFF);
    for (;;)
        pause();
}
