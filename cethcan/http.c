#include "cethcan.h"

#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <openssl/sha.h>

static struct evhttp *evhttp;
static struct can_user *cuhttp;

static int evb_json_add(const char *data, size_t size, void *arg)
{
	struct evbuffer *buf = arg;
	return evbuffer_add(buf, data, size);
}

static void http_json_basic(struct evhttp_request *req, void *arg)
{
	struct evkeyvalq *outhdr = evhttp_request_get_output_headers(req);
	struct evbuffer *out = evbuffer_new();

	evhttp_add_header(outhdr, "Content-Type", "application/json; charset=utf-8");

	json_t *jsout = json_object();
	can_json(jsout, JSON_NORMAL);
	json_dump_callback(jsout, evb_json_add, out, JSON_SORT_KEYS | JSON_INDENT(4));
	json_decref(jsout);

	evhttp_send_reply(req, 200, "OK", out);
	evbuffer_free(out);
}

static void http_json_set(struct evhttp_request *req, void *arg)
{
	struct evkeyvalq *outhdr = evhttp_request_get_output_headers(req);
	struct evbuffer *out = evbuffer_new();
	const char *cmd = evhttp_uri_get_query(req->uri_elems), *set;
	char *e = NULL;
	unsigned long cmdl, setl;
	struct can_message msg;

	if (!cmd || !(set = strchr(cmd, '=')) || set == cmd || set[1] == '\0')
		goto out_inval;
	cmdl = strtoul(cmd, &e, 0);
	if (e != set)
		goto out_inval;
	setl = strtoul(set + 1, &e, 0);
	if (*e || setl > 0xff)
		goto out_inval;

	msg.daddr = CANA_LIGHT_F(0, cmdl);
	msg.dlc = 1;
	msg.bytes[0] = setl;
	can_broadcast(cuhttp, &msg);

	evhttp_add_header(outhdr, "Content-Type", "text/plain; charset=utf-8");

	evbuffer_add_printf(out, "ok %lu = %lu", cmdl, setl);
	evhttp_send_reply(req, 200, "OK", out);
	evbuffer_free(out);
	return;

out_inval:
	evbuffer_add_printf(out, "invalid request.");
	evhttp_send_reply(req, 500, "Parameter missing", out);
	evbuffer_free(out);
}

static void http_jsonrpc_response(void *arg, struct evbuffer *data)
{
	struct evhttp_request *req = arg;
	evhttp_send_reply_chunk(req, data);
	evhttp_send_reply_end(req);
}

static void http_jsonrpc(struct evhttp_request *req, void *arg)
{
	struct evkeyvalq *outhdr = evhttp_request_get_output_headers(req);
	struct evbuffer *inp = evhttp_request_get_input_buffer(req);

	if (evhttp_request_get_command(req) != EVHTTP_REQ_POST) {
		evhttp_send_error(req, 405, "JSON-RPC request must be POSTed");
		return;
	}

	evhttp_add_header(outhdr, "Content-Type", "application/json; charset=utf-8");
	evhttp_send_reply_start(req, 200, "OK");
	evhttp_request_own(req);

	rpc_perform(inp, http_jsonrpc_response, req);
	return;
}

static void http_json_bump(struct evhttp_request *req, void *arg)
{
	struct evkeyvalq *outhdr = evhttp_request_get_output_headers(req);
	struct evbuffer *out = evbuffer_new();

	evhttp_add_header(outhdr, "Content-Type", "text/plain; charset=utf-8");
	evbuffer_add_printf(out, "OK");
	evhttp_send_reply(req, 200, "OK", out);
	evbuffer_free(out);

	json_bump_longpoll();
}

struct longpoll {
	struct longpoll *next;
	struct evhttp_request *req;
};
static struct longpoll *longpolls = NULL, **plongpoll = &longpolls;
static size_t longpoll_count = 0;

static char *longpollbuf = NULL;
static size_t longpollbuflen = 0;
static char longpollhash[SHA_DIGEST_LENGTH * 2 + 1] = "";

static void http_json_longpoll(struct evhttp_request *req, void *arg)
{
	struct evkeyvalq *outhdr = evhttp_request_get_output_headers(req);
	struct evbuffer *out;
	struct longpoll *lp;
	const char *query;
	
	evhttp_add_header(outhdr, "Content-Type", "application/json; charset=utf-8");

	query = evhttp_uri_get_query(evhttp_request_get_evhttp_uri(req));
	if (query && !strcmp(query, longpollhash)) {
		lprintf("long poll");
		evhttp_send_reply_start(req, 200, "OK Long Poll");
		evhttp_request_own(req);

		lp = calloc(sizeof(*lp), 1);
		lp->req = req;
		*plongpoll = lp;
		plongpoll = &lp->next;
		longpoll_count++;
		return;
	}

	out = evbuffer_new();
	evbuffer_add(out, longpollbuf, longpollbuflen);
	evhttp_send_reply(req, 200, "OK Short Poll", out);
	evbuffer_free(out);
}

static void longpoll_updatedata(void)
{
	json_t *jsout, *jswrap;
	char *data;
	unsigned char digest[SHA_DIGEST_LENGTH];

	if (longpollbuf)
		free(longpollbuf);
	
	jsout = json_object();
	can_json(jsout, JSON_LONGPOLL);
	data = json_dumps(jsout, JSON_SORT_KEYS | JSON_INDENT(4));
	SHA1((unsigned char *)data, strlen(data), digest);
	free(data);

	for (size_t i = 0; i < SHA_DIGEST_LENGTH; i++)
		sprintf(longpollhash + i * 2, "%02x", digest[i]);

	jswrap = json_object();
	json_object_set_new(jswrap, "data", jsout);
	json_object_set_new(jswrap, "ref", json_string(longpollhash));
	longpollbuf = json_dumps(jswrap, JSON_SORT_KEYS | JSON_INDENT(4));
	longpollbuflen = strlen(longpollbuf);
	json_decref(jswrap);
}

void json_bump_longpoll(void)
{
	struct longpoll *lp, *lpnext;
	struct evbuffer *out;

	longpoll_updatedata();

	out = evbuffer_new();
	for (lp = longpolls; lp; lp = lpnext) {
		lpnext = lp->next;

		evbuffer_add(out, longpollbuf, longpollbuflen);
		evhttp_send_reply_chunk(lp->req, out);
		/* send_reply_end calls request_free() */
		evhttp_send_reply_end(lp->req);
		free(lp);
	}
	evbuffer_free(out);

	longpolls = NULL;
	plongpoll = &longpolls;
	longpoll_count = 0;
}

static void http_can_handler(void *arg, struct can_message *msg)
{
}

void http_init(void)
{
	cuhttp = can_register_alloc(NULL, http_can_handler, "http");

	evhttp = evhttp_new(ev_base);
	evhttp_set_cb(evhttp, "/", http_json_basic, NULL);
	evhttp_set_cb(evhttp, "/subcan.json", http_json_basic, NULL);
	evhttp_set_cb(evhttp, "/set", http_json_set, NULL);
	evhttp_set_cb(evhttp, "/longpoll", http_json_longpoll, NULL);
	evhttp_set_cb(evhttp, "/bump", http_json_bump, NULL);
	evhttp_set_cb(evhttp, "/jsonrpc", http_jsonrpc, NULL);
	evhttp_bind_socket(evhttp, "127.0.0.1", 34999);

	longpoll_updatedata();
}
