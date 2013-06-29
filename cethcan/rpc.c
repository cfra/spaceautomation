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
	const char *name = json_string_value(json_array_get(json_params, 0));
	unsigned val = json_integer_value(json_array_get(json_params, 1));
	
	l = light_find(name);
	if (!l) {
		*result = jsonrpc_error_object(JSONRPC_INVALID_PARAMS,
			json_string("cann't find specified light"));
		return JSONRPC_INVALID_PARAMS;
	}
	*result = json_boolean(!light_set(l, val));
	return 0;
}

static int rpc_light_get(void *apparg, json_t *json_params, json_t **result)
{
	struct light *l;
	const char *name = json_string_value(json_array_get(json_params, 0));
	unsigned set, actual;

	l = light_find(name);
	if (!l) {
		*result = jsonrpc_error_object(JSONRPC_INVALID_PARAMS,
			json_string("cannot find specified light"));
		return JSONRPC_INVALID_PARAMS;
	}

	set = light_getset(l);
	actual = light_getact(l);

	*result = json_pack("{s:i,s:i}", "set", set, "actual", actual);
	return 0;
}

struct jsonrpc_method_entry_t method_table[] = {
	{ "ping", rpc_ping, "" },
	{ "light_set", rpc_light_set, "[si]" },
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
	if (output)
		evbuffer_add(outbuf, output, strlen(output));
	response_handler(handler_arg, outbuf);
	evbuffer_free(outbuf);
}
