// SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
/* Copyright (c) 2020 Cloudflare */
/*
 * Inserts a socket belonging to another process, as specified by the target PID
 * and FD number, into a given BPF map.
 */

#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/bpf.h>

#include "syscall.h"

int main(int argc, char **argv)
{
	pid_t target_pid;
	int pid_fd, target_fd, sock_fd, map_fd, err;
	uint32_t key;
	uint64_t value;
	const char *map_path;
	union bpf_attr attr;

	if (argc < 4) {
		fprintf(stderr, "Usage: %s <target pid> <target fd> <map path> [map key]\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	target_pid = atoi(argv[1]);
	target_fd = atoi(argv[2]);
	map_path = argv[3];
	key = 0;

	if (argc == 5)
		key = atoi(argv[4]);

	/* Get duplicate FD for the socket */
	pid_fd = pidfd_open(target_pid, 0);
	if (pid_fd == -1)
		error(EXIT_FAILURE, errno, "pidfd_open");

	sock_fd = pidfd_getfd(pid_fd, target_fd, 0);
	if (sock_fd == -1)
		error(EXIT_FAILURE, errno, "pidfd_getfd");

	/* Open BPF map for storing the socket */
	memset(&attr, 0, sizeof(attr));
	attr.pathname = (uint64_t) map_path;
	attr.bpf_fd = 0;
	attr.file_flags = 0;

	map_fd = bpf(BPF_OBJ_GET, &attr, sizeof(attr));
	if (map_fd == -1)
		error(EXIT_FAILURE, errno, "bpf(OBJ_GET)");

	/* Insert socket FD into the BPF map */
	value = (uint64_t) sock_fd;
	memset(&attr, 0, sizeof(attr));
	attr.map_fd = map_fd;
	attr.key = (uint64_t) &key;
	attr.value = (uint64_t) &value;
	attr.flags = BPF_ANY;

	err = bpf(BPF_MAP_UPDATE_ELEM, &attr, sizeof(attr));
	if (err)
		error(EXIT_FAILURE, errno, "bpf(MAP_UPDATE_ELEM)");

	close(map_fd);
	close(sock_fd);
	close(pid_fd);
}
