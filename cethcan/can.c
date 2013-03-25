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

	msg->origin = origin;
	for (u = users; u; u = u->next)
		if (u != origin)
			u->handler(u->arg, msg);
}

void can_init(void)
{
	/* nothing to do */
}

