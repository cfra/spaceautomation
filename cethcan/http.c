#include "cethcan.h"

#include <event2/http.h>
#include <event2/buffer.h>
#include <openssl/sha.h>

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

	evhttp_add_header(outhdr, "Content-Type", "application/json; charset=utf-8");

	json_t *jsout = json_object();
	can_json(jsout, JSON_NORMAL);
	json_dump_callback(jsout, evb_json_add, out, JSON_SORT_KEYS | JSON_INDENT(4));
	json_decref(jsout);

	evhttp_send_reply(req, 200, "OK", out);
	evbuffer_free(out);
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

void http_init(void)
{
	evhttp = evhttp_new(ev_base);
	evhttp_set_cb(evhttp, "/", http_json_basic, NULL);
	evhttp_set_cb(evhttp, "/longpoll", http_json_longpoll, NULL);
	evhttp_set_cb(evhttp, "/bump", http_json_bump, NULL);
	evhttp_bind_socket(evhttp, "127.0.0.1", 34999);

	longpoll_updatedata();
}
