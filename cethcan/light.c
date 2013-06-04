#include "cethcan.h"

struct value {
	uint8_t val;
	time_t change, valid;
};

struct light {
	struct can_user *u;

	char *name;
	unsigned logical_addr;

	struct value set, actual;
};

static void light_json_handler(void *arg, json_t *json, enum json_subtype type)
{
	struct light *l = arg;
	json_t *lobj = json_object();

	json_object_set_new(lobj, "klass", json_string("light"));
	json_object_set_new(lobj, "addr", json_integer(l->logical_addr));

	json_object_set_new(lobj, "actual", json_integer(l->actual.val));
	if (type != JSON_LONGPOLL)
		json_object_set_new(lobj, "actual_ts", json_integer(l->actual.valid));
	json_object_set_new(lobj, "actual_tschg", json_integer(l->actual.change));

	json_object_set_new(lobj, "set", json_integer(l->set.val));
	if (type != JSON_LONGPOLL)
		json_object_set_new(lobj, "set_ts", json_integer(l->set.valid));
	json_object_set_new(lobj, "set_tschg", json_integer(l->set.change));

	json_object_set_new(json, l->name, lobj);
}

static void light_can_handler(void *arg, struct can_message *msg)
{
	struct light *l = arg;
	struct value *v = NULL;
	unsigned laddr;
	uint8_t dval;

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

	if (!json_is_object(config)) {
		lprintf("light config must be an object/dictionary");
		return 1;
	}
	if (!json_is_integer(json_object_get(config, "addr"))) {
		lprintf("light config must have an 'addr' key");
		return 1;
	}
	if (!json_is_string(json_object_get(config, "name"))) {
		lprintf("light config must have a 'name' key");
		return 1;
	}
	
	l = calloc(sizeof(*l), 1);
	l->name = strdup(json_string_value(json_object_get(config, "name")));
	l->logical_addr = json_integer_value(json_object_get(config, "addr"));

	l->u = can_register_alloc(l, light_can_handler, "light[%s]", l->name);
	l->u->json = light_json_handler;
	return 0;
}