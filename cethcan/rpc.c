#include "cethcan.h"

#include <event2/buffer.h>
#include "jsonrpc/jsonrpc.h"

static int rpc_ping(void *apparg, json_t *json_params, json_t **result)
{
	*result = json_string("pong");
	return 0;
}

struct jsonrpc_method_entry_t method_table[] = {
	{ "ping", rpc_ping, "" },
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
