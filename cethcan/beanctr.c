#include "cethcan.h"

struct bean {
	struct can_user *u;

	char *name;
	unsigned logical_addr;

	uint8_t raw;
	json_t *vals[2];

	time_t change, valid;
};

static void bean_json_handler(void *arg, json_t *json, enum json_subtype type)
{
	struct bean *b = arg;
	json_t *bobj = json_object();

	json_object_set_new(bobj, "klass", json_string("beancounter"));
	json_object_set_new(bobj, "addr", json_integer(b->logical_addr));

	json_object_set_new(bobj, "raw", json_integer(b->raw));
	json_object_set_new(bobj, "value", json_boolean(b->raw & 1));
	json_object_set_new(bobj, "text", json_incref(b->vals[b->raw & 1]));
	if (type != JSON_LONGPOLL)
		json_object_set_new(bobj, "ts", json_integer(b->valid));
	json_object_set_new(bobj, "tschg", json_integer(b->change));

	json_object_set_new(json, b->name, bobj);
}

static void bean_can_handler(void *arg, struct can_message *msg)
{
	struct bean *b = arg;
	unsigned laddr;
	uint8_t dval;

	if ((msg->daddr & CANA_PROTOCOL) != CANA_SENSOR)
		return;

	laddr = msg->daddr & 0xfff;
	if (b->logical_addr < laddr)
		return;
	if (b->logical_addr - laddr >= msg->dlc)
		return;
	dval = msg->bytes[b->logical_addr - laddr];

	time(&b->valid);
	if (dval != b->raw || b->change == 0) {
		b->raw = dval;
		time(&b->change);
		lprintf("%s: set %02x", b->u->name, dval);

		json_bump_longpoll();
	}
}

int bean_init_conf(json_t *config)
{
	struct bean *b;
	json_t *vals;

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
	vals = json_object_get(config, "values");
	if (!json_is_array(vals) || json_array_size(vals) != 2) {
		lprintf("light config must have a 'values' array of len 2");
		return 1;
	}

	b = calloc(sizeof(*b), 1);
	b->name = strdup(json_string_value(json_object_get(config, "name")));
	b->logical_addr = json_integer_value(json_object_get(config, "addr"));
	b->vals[0] = json_array_get(vals, 0);
	b->vals[1] = json_array_get(vals, 1);

	b->u = can_register_alloc(b, bean_can_handler, "bean[%s]", b->name);
	b->u->json = bean_json_handler;
	return 0;
}
