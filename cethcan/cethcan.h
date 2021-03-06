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
#include <signal.h>
#include <assert.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <jansson.h>

#include <cosc/cosc.h>
#include <cosc/oscserver.h>
#include <cosc/oscparser.h>

#include "protocol.h"

extern int verbosity;

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

enum json_subtype {
	JSON_NORMAL = 0,
	JSON_LONGPOLL = 1,
};

typedef void (*can_handler)(void *arg, struct can_message *msg);
typedef void (*json_handler)(void *arg, json_t *json, enum json_subtype type);

struct can_user {
	struct can_user *next;

	const char *name;

	void *arg;
	can_handler handler;
	json_handler json;
};

extern void can_register(struct can_user *user);
extern struct can_user *can_register_alloc(void *arg, can_handler handler,
		const char *fmt, ...);
extern void can_broadcast(struct can_user *origin, struct can_message *msg);
extern void can_json(json_t *json, enum json_subtype type);
extern void can_init(void);

extern void rpc_perform(struct evbuffer *request,
	void (*response_handler)(void *arg, struct evbuffer *data),
	void *handler_arg);

struct light;
extern struct light *light_find(const char *name);
extern int light_set(struct light *l, unsigned value);
extern unsigned light_getset(struct light *l);
extern unsigned light_getact(struct light *l);

struct espnet_device;
struct espnet_device *espnet_find(const char *name);
extern int espnet_set(struct espnet_device *dev,
		unsigned r, unsigned g, unsigned b);
extern void espnet_get(struct espnet_device *dev,
		unsigned *r, unsigned *g, unsigned *b);

struct ttydmx_device;
struct ttydmx_device *ttydmx_find(const char *name);
extern int ttydmx_set(struct ttydmx_device *dev,
		unsigned r, unsigned g, unsigned b);
extern void ttydmx_get(struct ttydmx_device *dev,
		unsigned *r, unsigned *g, unsigned *b);

extern void json_bump_longpoll(void);

extern int socan_init(json_t *config);
extern int ether_init(json_t *config);
extern int light_init_conf(json_t *config);
extern int bean_init_conf(json_t *config);
extern int espnet_init_conf(json_t *config);
extern int ttydmx_init_conf(json_t *config);
extern void http_init(void);

extern struct osc_server *osc_server;
extern void osc_init(void);

#endif /* _CETHCAN_H */
