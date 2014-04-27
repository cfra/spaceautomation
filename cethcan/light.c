#include "cethcan.h"

struct value {
	uint8_t val;
	time_t change, valid;
};

struct light {
	struct light *next;
	struct can_user *u;

	char *name;
	bool aggregate;

	unsigned logical_addr;
	size_t slave_count;
	struct light **slaves;

	struct value set, actual;
};

static struct light *lights = NULL, **plights = &lights;

struct light *light_find(const char *name)
{
	struct light *l;
	for (l = lights; l; l = l->next)
		if (!strcmp(l->name, name))
			break;
	return l;
}

int light_set(struct light *l, unsigned value)
{
	if (l->aggregate) {
		int ec = 0;
		for (size_t i = 0; i < l->slave_count; i++)
			ec |= light_set(l->slaves[i], value);
		return ec;
	}

	struct can_message msg;
	msg.daddr = CANA_LIGHT_F(0, l->logical_addr);
	msg.dlc = 1;
	msg.bytes[0] = value;
	can_broadcast(l->u, &msg);

	return 0;
}

unsigned light_getset(struct light *l)
{
	if (l->aggregate) {
		unsigned long long sum = 0;
		size_t count = 0;

		for (size_t i = 0; i < l->slave_count; i++) {
			sum += light_getset(l->slaves[i]);
			count++;
		}

		if (count)
			return sum / count;
		return 0;
	}

	return l->set.val;
}

unsigned light_getact(struct light *l)
{
	if (l->aggregate) {
		unsigned long long sum = 0;
		size_t count = 0;

		for (size_t i = 0; i < l->slave_count; i++) {
			sum += light_getact(l->slaves[i]);
			count++;
		}

		if (count)
			return sum / count;
		return 0;
	}

	return l->actual.val;
}

static void light_json_handler(void *arg, json_t *json, enum json_subtype type)
{
	struct light *l = arg;
	struct light *p = l->aggregate ? l->slaves[0] : l;
	json_t *lobj = json_object();

	json_object_set_new(lobj, "klass", json_string("light"));
	json_object_set_new(lobj, "addr", json_integer(p->logical_addr));

	json_object_set_new(lobj, "actual", json_integer(light_getact(l)));
	if (type != JSON_LONGPOLL)
		json_object_set_new(lobj, "actual_ts", json_integer(p->actual.valid));
	json_object_set_new(lobj, "actual_tschg", json_integer(p->actual.change));

	json_object_set_new(lobj, "set", json_integer(light_getset(l)));
	if (type != JSON_LONGPOLL)
		json_object_set_new(lobj, "set_ts", json_integer(p->set.valid));
	json_object_set_new(lobj, "set_tschg", json_integer(p->set.change));

	json_object_set_new(json, l->name, lobj);
}

static void light_can_handler(void *arg, struct can_message *msg)
{
	struct light *l = arg;
	struct value *v = NULL;
	unsigned laddr;
	uint8_t dval;

	if (l->aggregate)
		return;

	if ((msg->daddr & CANA_PROTOCOL) == CANA_LIGHT)
		v = &l->set;
	if ((msg->daddr & CANA_PROTOCOL) == CANA_SENSOR)
		v = &l->actual;
	if (!v)
		return;

	laddr = msg->daddr & 0xfff;
	if (l->logical_addr < laddr)
		return;
	if (l->logical_addr - laddr >= msg->dlc)
		return;
	dval = msg->bytes[l->logical_addr - laddr];

	time(&v->valid);
	if (dval != v->val || v->change == 0) {
		v->val = dval;
		time(&v->change);
		lprintf("%s: set %02x", l->u->name, dval);

		json_bump_longpoll();
	}
}

int light_init_conf(json_t *config)
{
	struct light *l;
	bool aggregate;

	if (!json_is_object(config)) {
		lprintf("light config must be an object/dictionary");
		return 1;
	}
	if (json_is_integer(json_object_get(config, "addr"))) {
		aggregate = false;
	} else if (json_is_array(json_object_get(config, "slaves"))) {
		aggregate = true;
	} else {
		lprintf("light config must have an 'addr' key or 'saves'.");
		return 1;
	}

	if (!json_is_string(json_object_get(config, "name"))) {
		lprintf("light config must have a 'name' key");
		return 1;
	}
	
	l = calloc(sizeof(*l), 1);
	l->name = strdup(json_string_value(json_object_get(config, "name")));
	l->aggregate = aggregate;

	if (aggregate) {
		json_t *slaves = json_object_get(config, "slaves");
		l->slave_count = json_array_size(slaves);
		if (!l->slave_count) {
			lprintf("Slave array must not be empty.");
			return 1;
		}

		l->slaves = calloc(l->slave_count, sizeof(*l->slaves));
		for (size_t i = 0; i < json_array_size(slaves); i++) {
			const char *name = json_string_value(
				json_array_get(slaves, i)
			);
			l->slaves[i] = light_find(name);
			if (!l->slaves[i]) {
				lprintf("Unknown slave '%s'", name);
				return 1;
			}
		}
	} else {
		l->logical_addr = json_integer_value(
				json_object_get(config, "addr")
		);
	}

	l->u = can_register_alloc(l, light_can_handler, "light[%s]", l->name);
	l->u->json = light_json_handler;

	*plights = l;
	plights = &l->next;
	return 0;
}
