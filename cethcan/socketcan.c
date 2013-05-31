#include "cethcan.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

struct socan {
	struct can_user *u;
	struct event *ev;

	char *ifname;
	int ifindex;

	int sock;
};

static void socan_handler(void *arg, struct can_message *msg)
{
	struct socan *sc = arg;
	struct can_frame frame;
	memset(&frame, 0, sizeof(frame));

	if (msg->daddr & 0x00080000)
		frame.can_id = CAN_EFF_FLAG
			| (msg->daddr & 0x0003ffff)
			| ((msg->daddr & 0xffe00000) >> 3);
	else
		frame.can_id = msg->daddr >> 21;

	frame.can_dlc = msg->dlc;
	memcpy(frame.data, msg->bytes, msg->dlc > CANFD_MAX_DLEN ? 
			CANFD_MAX_DLEN : msg->dlc);

	if (write(sc->sock, &frame, sizeof(frame)) != sizeof(frame))
		lprintf("%s: send failed: %s", sc->u->name, strerror(errno));
}

static void socan_event(int sock, short event, void *arg)
{
	struct socan *sc = arg;
	struct can_message msg;
	union {
		struct sockaddr_can can;
		struct sockaddr_storage ss;
		struct sockaddr su;
	} canaddr;
	socklen_t addrlen = sizeof(canaddr);
	struct canfd_frame frame;

	if (event & EV_READ) {
		ssize_t rlen = recvfrom(sc->sock, &frame, sizeof(frame),
			MSG_TRUNC, &canaddr.su, &addrlen);
		if (rlen > 0) {
#if 0
			if (1) {
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
#endif
			if (frame.can_id & CAN_EFF_FLAG)
				msg.daddr = 0x00080000
					| (frame.can_id & 0x0003ffff)
					| ((frame.can_id & 0x1ffc0000) << 3);
			else
				msg.daddr = (frame.can_id & 0x7ff) << 21;
			msg.dlc = frame.len;
			if (msg.dlc > 8)
				msg.dlc = 8;
			memcpy(msg.bytes, frame.data, msg.dlc);
			can_broadcast(sc->u, &msg);
		}
	}
}

int socan_init(json_t *config)
{
	struct socan *sc;
	const char *iface;
	int ifindex;
	int on = 1;
	struct sockaddr_can addr;

	if (!json_is_object(config)) {
		lprintf("socketcan config must be an object/dictionary");
		return 1;
	}
	if (!json_is_string(json_object_get(config, "interface"))) {
		lprintf("socketcan config must have an 'interface' key");
		return 1;
	}
	iface = json_string_value(json_object_get(config, "interface"));
	ifindex = if_nametoindex(iface);

	if (ifindex == 0) {
		lprintf("socketcan interface '%s' error: %s",
			iface, strerror(errno));
		return 1;
	}

	sc = calloc(sizeof(*sc), 1);
	sc->ifname = strdup(iface);
	sc->ifindex = ifindex;

	sc->sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (sc->sock == -1) {
		lprintf("socketcan interface '%s' socket() error: %s",
			iface, strerror(errno));
		free(sc);
		return 1;
	}
	if (setsockopt(sc->sock, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
		&on, sizeof(on))) {
		lprintf("socketcan interface '%s' raw_fd_frames error: %s",
			iface, strerror(errno));
		return 1;
	}
/*	if (setsockopt(sc->sock, SOL_SOCKET, SO_TIMESTAMP, &on, sizeof(on))) {
		lprintf("socketcan interface '%s' so_timestamp error: %s",
			iface, strerror(errno));
		return 1;
	} */

	addr.can_family = AF_CAN;
	addr.can_ifindex = ifindex;
	if (bind(sc->sock, (struct sockaddr *)&addr, sizeof(addr))) {
		lprintf("socketcan interface '%s' bind error: %s",
			iface, strerror(errno));
		return 1;
	}

	sc->u = can_register_alloc(sc, socan_handler, "socketcan[%s]", iface);
	sc->ev = event_new(ev_base, sc->sock, EV_READ | EV_PERSIST,
		socan_event, sc);
	event_add(sc->ev, NULL);

	return 0;
}
