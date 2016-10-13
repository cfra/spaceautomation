#include "cethcan.h"

struct osc_server *osc_server = NULL;
static struct event *osc_server_event = NULL;

static void osc_sock_handler(int sock, short event, void *arg)
{
	if (osc_server_run(osc_server)) {
		event_del(osc_server_event);
		osc_server_event = NULL;
	}
}

void osc_init(void)
{
	if (osc_server)
		return;

	osc_server = osc_server_new(NULL, "4223", NULL);
	if (!osc_server)
		return;

	if (osc_server_set_blocking(osc_server, false))
		return;

	osc_server_event = event_new(ev_base, osc_server_fd(osc_server),
	                             EV_READ | EV_PERSIST, osc_sock_handler, NULL);
	event_add(osc_server_event, NULL);
}
