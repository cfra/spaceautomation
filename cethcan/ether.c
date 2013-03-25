#include "cethcan.h"

#include <net/if.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>

#define ETHER_PROTO 0x88b7
#define ETHER_MCADDR 0xff, 0x3a, 0xf6, 'C', 'A', 'N'

struct ether {
	struct can_user *u;
	struct event *ev;

	char *ifname;
	int ifindex;

	int sock;
};

static void ether_can_handler(void *arg, struct can_message *msg)
{
}

static void ether_sock_handler(int sock, short event, void *arg)
{
}

int ether_init(json_t *config)
{
	struct ether *e;
	const char *iface;
	int ifindex;
	struct packet_mreq pmr = {
		.mr_type = PACKET_MR_MULTICAST,
		.mr_alen = ETH_ALEN,
		.mr_address = { ETHER_MCADDR },
	};

	if (!json_is_object(config)) {
		lprintf("ethernet config must be an object/dictionary");
		return 1;
	}
	if (!json_is_string(json_object_get(config, "interface"))) {
		lprintf("ethernet config must have an 'interface' key");
		return 1;
	}
	iface = json_string_value(json_object_get(config, "interface"));
	ifindex = if_nametoindex(iface);

	if (ifindex == 0) {
		lprintf("ethernet interface '%s' error: %s",
			iface, strerror(errno));
		return 1;
	}

	e = calloc(sizeof(*e), 1);
	e->ifname = strdup(iface);
	e->ifindex = ifindex;

	e->sock = socket(AF_PACKET, SOCK_RAW, htons(ETHER_PROTO));
	if (e->sock == -1) {
		lprintf("ethernet interface '%s' socket() error: %s",
			iface, strerror(errno));
		return 1;
	}

	pmr.mr_ifindex = ifindex;
	if (setsockopt(e->sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
		&pmr, sizeof(pmr))) {
		lprintf("ethernet interface '%s' multicast join error: %s",
			iface, strerror(errno));
		return 1;
	}

	e->u = can_register_alloc(e, ether_can_handler, "ether[%s]", iface);
	e->ev = event_new(ev_base, e->sock, EV_READ | EV_PERSIST, ether_sock_handler, e);
	event_add(e->ev, NULL);

	return 0;
}
