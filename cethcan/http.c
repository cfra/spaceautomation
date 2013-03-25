#include "cethcan.h"

#include <event2/http.h>
#include <event2/buffer.h>

static struct evhttp *evhttp;

static int evb_json_add(const char *data, size_t size, void *arg)
{
	struct evbuffer *buf = arg;
	return evbuffer_add(buf, data, size);
}

static void http_json_basic(struct evhttp_request *req, void *arg)
{
	struct evkeyvalq *outhdr = evhttp_request_get_output_headers(req);
	struct evbuffer *out = evbuffer_new();

	evhttp_add_header(outhdr, "Content-Type", "text/plain; charset=utf-8");

	json_t *jsout = json_object();
	can_json(jsout, JSON_NORMAL);
	json_dump_callback(jsout, evb_json_add, out, JSON_SORT_KEYS | JSON_INDENT(4));
	evhttp_send_reply(req, 200, "OK", out);
	evbuffer_free(out);
}

void http_init(void)
{
	evhttp = evhttp_new(ev_base);
	evhttp_set_cb(evhttp, "/", http_json_basic, NULL);
	evhttp_bind_socket(evhttp, "127.0.0.1", 34999);
}
