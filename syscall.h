#pragma once

/*
 * Syscall wrappers missing in glibc
 */

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <unistd.h>

#include <linux/bpf.h>

static inline int bpf(enum bpf_cmd cmd, union bpf_attr *attr, unsigned int size)
{
        return syscall(__NR_bpf, cmd, attr, size);
}

static inline int pidfd_open(pid_t target_pid, unsigned int flags)
{
	return syscall(__NR_pidfd_open, target_pid, flags);
}

static inline int pidfd_getfd(int pidfd, int targetfd, unsigned int flags)
{
	return syscall(__NR_pidfd_getfd, pidfd, targetfd, flags);
}
