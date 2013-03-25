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

struct frame {
	uint8_t daddr[ETH_ALEN];
	uint8_t saddr[ETH_ALEN];
	uint16_t proto;
	uint8_t oui[3];
	uint16_t protoid;
	uint8_t protover;
	uint32_t ts1, ts2;
	uint32_t can_daddr;
	uint8_t can_dlc;
	uint8_t can_data[8];
} __attribute__((packed));


static void ether_can_handler(void *arg, struct can_message *msg)
{
	struct ether *e = arg;
	lprintf("%s: TX not implemented", e->u->name);
}

static void ether_rx_dataframe(struct ether *e, struct frame *f)
{
	struct can_message msg;

	msg.daddr = ntohl(f->can_daddr);
	msg.dlc = f->can_dlc;
	if (msg.dlc > 8)
		msg.dlc = 8;
	memcpy(msg.bytes, f->can_data, msg.dlc);

	can_broadcast(e->u, &msg);
}

static void ether_sock_handler(int sock, short event, void *arg)
{
	struct ether *e = arg;
	union {
		struct sockaddr_ll ll;
		struct sockaddr_storage ss;
		struct sockaddr su;
	} lladdr;
	socklen_t addrlen = sizeof(lladdr);
	union {
		struct frame frame;
		uint8_t raw[1536];
	} buf;

	if (event & EV_READ) {
		ssize_t rlen = recvfrom(e->sock, &buf, sizeof(buf),
			MSG_TRUNC, &lladdr.su, &addrlen);
		if (rlen > 0) {
			if (0) {
				lprintf("got %zd bytes", rlen);
				char pbuffer[16 * 3 + 1];
				for (size_t i = 0; i < (size_t)rlen; i++) {
					size_t j = i % 16;
					sprintf(pbuffer + (3 * j), " %02x", buf.raw[i]);
					if (j == 15)
						lprintf(">>%s", pbuffer);
				}
				if ((rlen % 16) != 15)
					lprintf(">>%s", pbuffer);
			}

			if (ntohs(buf.frame.proto) == ETHER_PROTO
				&& buf.frame.oui[0] == 0x00
				&& buf.frame.oui[1] == 0x80
				&& buf.frame.oui[2] == 0x41
				&& ntohs(buf.frame.protoid) == 0xaaaa) {
				switch (buf.frame.protover) {
				case 3:
					ether_rx_dataframe(e, &buf.frame);
					break;
				default:
					lprintf("%s: unsupported CAN protocol version %d",
						e->u->name, buf.frame.protover);
				}
			} else {
				lprintf("%s: non-CAN frame (%zd bytes)",
					e->u->name, rlen);
			}
		}
	}
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
