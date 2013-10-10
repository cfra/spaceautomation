#include "cethcan.h"

#include <event2/buffer.h>
#include "jsonrpc/jsonrpc.h"

static int rpc_ping(void *apparg, json_t *json_params, json_t **result)
{
	*result = json_string("pong");
	return 0;
}

static int rpc_light_set(void *apparg, json_t *json_params, json_t **result)
{
	struct light *l;
	struct espnet_device *esp;
	const char *name = json_string_value(json_array_get(json_params, 0));
	const char *emsg;

	l = light_find(name);
	if (l) {
		unsigned val;

		if (!json_is_integer(json_array_get(json_params, 1))) {
			emsg = "expected integer value";
			goto out_err;
		}

		val = json_integer_value(json_array_get(json_params, 1));
		*result = json_boolean(!light_set(l, val));
		return 0;
	}

	esp = espnet_find(name);
	if (esp) {
		unsigned r, g, b;
		json_t *val = json_array_get(json_params, 1);

		if (json_is_integer(val)) {
			r = json_integer_value(val);
			g = (r * 180) / 255;
			b = (r * 144) / 255;
		} else if (json_is_array(val)) {
			if (json_unpack(val, "[iii]", &r, &g, &b)) {
				emsg = "failed to parse value array";
				goto out_err;
			}
		} else {
			emsg = "expected integer or [int,int,int] value";
			goto out_err;
		}

		*result = json_boolean(!espnet_set(esp, r, g, b));
		return 0;
	}

	emsg = "cann't find specified light";
out_err:
	*result = jsonrpc_error_object(JSONRPC_INVALID_PARAMS,
			json_string(emsg));
	return JSONRPC_INVALID_PARAMS;
}

static int rpc_light_get(void *apparg, json_t *json_params, json_t **result)
{
	struct light *l;
	struct espnet_device *esp;
	const char *name = json_string_value(json_array_get(json_params, 0));
	unsigned set, actual;

	l = light_find(name);
	if (l) {
		set = light_getset(l);
		actual = light_getact(l);

		*result = json_pack("{s:i,s:i}",
				"set", set, "actual", actual);
		return 0;
	}

	esp = espnet_find(name);
	if (esp) {
		unsigned r, g, b;
		espnet_get(esp, &r, &g, &b);

		*result = json_pack("{s:i,s:i,s:i}",
				"r", r, "g", g, "b", b);
		return 0;
	}

	*result = jsonrpc_error_object(JSONRPC_INVALID_PARAMS,
		json_string("cannot find specified light"));
	return JSONRPC_INVALID_PARAMS;
}

struct jsonrpc_method_entry_t method_table[] = {
	{ "ping", rpc_ping, "" },
	{ "light_set", rpc_light_set, "[so]" },
	{ "light_get", rpc_light_get, "[s]" },
	{ NULL, NULL, NULL },
};

void rpc_perform(struct evbuffer *request,
	void (*response_handler)(void *arg, struct evbuffer *data),
	void *handler_arg)
{
	size_t len = evbuffer_get_length(request);
	char *data = (char *)evbuffer_pullup(request, len);
	struct evbuffer *outbuf = evbuffer_new();

	/* TODO: asynchronous calls */
	char *output = jsonrpc_handler(NULL, data, len, method_table);
	if (output) {
		evbuffer_add(outbuf, output, strlen(output));
		free(output);
	}
	response_handler(handler_arg, outbuf);
	evbuffer_free(outbuf);
}
