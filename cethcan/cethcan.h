#ifndef _CETHCAN_H
#define _CETHCAN_H

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <time.h>
#include <assert.h>

#include <event2/event.h>
#include <jansson.h>

#include "protocol.h"

#define lprintf(...) do { \
	struct timeval tv; struct tm tm; char tvbuf[64]; \
	gettimeofday(&tv, NULL); localtime_r(&tv.tv_sec, &tm); \
	strftime(tvbuf, sizeof(tvbuf), "%Y-%m-%d %H:%M:%S", &tm); \
	fprintf(stderr, "%s.%03d ", tvbuf, tv.tv_usec / 1000); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
	} while (0)

extern struct event_base *ev_base;

struct can_user;

struct can_message {
	struct can_user *origin;

	uint32_t daddr;

	uint32_t flags;
#define CAN_MSGF_RTR		(1 << 0)

	size_t dlc;
	uint8_t bytes[8];
};

typedef void (*can_handler)(void *arg, struct can_message *msg);

struct can_user {
	struct can_user *next;

	const char *name;

	void *arg;
	can_handler handler;
};

extern void can_register(struct can_user *user);
extern struct can_user *can_register_alloc(void *arg, can_handler handler,
		const char *fmt, ...);
extern void can_broadcast(struct can_user *origin, struct can_message *msg);
extern void can_init(void);

extern int ether_init(json_t *config);
extern int light_init_conf(json_t *config);
extern void http_init(void);

#endif /* _CETHCAN_H */
