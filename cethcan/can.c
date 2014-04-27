#include "cethcan.h"

static struct can_user *users = NULL, **userlast = &users;

void can_register(struct can_user *user)
{
	user->next = NULL;
	*userlast = user;
	userlast = &user->next;
}

struct can_user *can_register_alloc(void *arg, can_handler handler,
	const char *fmt, ...)
{
	struct can_user *user;
	char *name = NULL;
	va_list ap;

	va_start(ap, fmt);
	vasprintf(&name, fmt, ap);
	va_end(ap);

	user = calloc(sizeof(*user), 1);
	user->arg = arg;
	user->handler = handler;
	user->name = name;

	can_register(user);
	return user;
}

void can_broadcast(struct can_user *origin, struct can_message *msg)
{
	struct can_user *u;
	char buf[3*8+1];

	msg->origin = origin;

	if (msg->dlc > 8) {
		lprintf("invalid CAN message (DLC = %zu)", msg->dlc);
		return;
	}
	if (verbosity >= 1) {
		for (size_t i = 0; i < msg->dlc; i++)
			sprintf(buf + 3 * i, " %02x", msg->bytes[i]);
		lprintf("%s: %08x (%zu)%s", origin->name,
			(unsigned)msg->daddr, msg->dlc, buf);
	}

	for (u = users; u; u = u->next)
		if (u != origin)
			u->handler(u->arg, msg);
}

void can_json(json_t *json, enum json_subtype type)
{
	struct can_user *u;
	for (u = users; u; u = u->next)
		if (u->json)
			u->json(u->arg, json, type);
}

void can_init(void)
{
	/* nothing to do */
}

