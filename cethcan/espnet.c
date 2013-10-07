#include "cethcan.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define ESPNET_PORT		3333
#define ESPNET_TIMER		500*1000 /*us*/
#define ESPNET_TIMER_SHORT	5*1000 /*us*/

struct espnet_device;

struct espnet_sink {
	struct espnet_sink *next;

	struct can_user *u;
	int fd;
	uint8_t universe;
	char *ifname;
	int ifindex;

	struct event *writer;
	bool writer_resched;

	struct espnet_device *devs;
};

struct espnet_device {
	struct espnet_device *next;
	struct espnet_sink *sink;

	char *name;
	unsigned baseaddr;
	uint8_t r, g, b;
};

struct espnet_packet {
	char signature[4];
	uint8_t universe;
	uint8_t startcode;
	uint8_t datatype;
	uint16_t length;
	uint8_t dmx[512];
} __attribute__((packed));

static struct espnet_sink *sinks = NULL, **psinks = &sinks;

struct espnet_device *espnet_find(const char *name)
{
	for (struct espnet_sink *sink = sinks; sink; sink = sink->next)
		for (struct espnet_device *d = sink->devs; d; d = d->next)
			if (!strcmp(d->name, name))
				return d;
	return NULL;
}

int espnet_set(struct espnet_device *dev, unsigned r, unsigned g, unsigned b)
{
	bool bump = dev->r != r || dev->g != g || dev->b != b;
		
	dev->r = r;
	dev->g = g;
	dev->b = b;

	if (!dev->sink->writer_resched) {
		struct timeval tvs = { .tv_sec = 0,
				.tv_usec = ESPNET_TIMER_SHORT };
		event_del(dev->sink->writer);
		event_add(dev->sink->writer, &tvs);
		dev->sink->writer_resched = true;
	}
	if (bump)
		json_bump_longpoll();
	return 0;
}

void espnet_get(struct espnet_device *dev,
		unsigned *r, unsigned *g, unsigned *b)
{
	*r = dev->r;
	*g = dev->g;
	*b = dev->b;
}

void espnet_sink_writer(evutil_socket_t fd, short event, void *arg)
{
	struct espnet_sink *sink = arg;
	struct timeval tvs = { .tv_sec = 0, .tv_usec = ESPNET_TIMER };
	struct espnet_packet packet = {
		.signature = { 'E', 'S', 'D', 'D' },
		.universe = sink->universe,
		.startcode = 0,
		.datatype = 1,
		.length = htons(512),
	};
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(ESPNET_PORT),
		.sin_addr = { .s_addr = INADDR_BROADCAST },
	};

	for (struct espnet_device *d = sink->devs; d; d = d->next) {
		if (d->baseaddr >= 511)
			continue;
		packet.dmx[d->baseaddr - 1] = d->r;
		packet.dmx[d->baseaddr + 0] = d->g;
		packet.dmx[d->baseaddr + 1] = d->b;
	}

	if (sendto(sink->fd, &packet, sizeof(packet), 0,
		(struct sockaddr *)&addr, sizeof(addr)) != sizeof(packet))
		lprintf("ESPnet[%s#%d] send failed: %s",
			sink->ifname, sink->universe, strerror(errno));

	sink->writer_resched = false;
	event_add(sink->writer, &tvs);
}

static void espnet_json_one(struct espnet_sink *sink,
		struct espnet_device *dev,
		json_t *json, enum json_subtype type)
{
	json_t *lobj = json_object();

	json_object_set_new(lobj, "klass", json_string("dmxrgb"));
	json_object_set_new(lobj, "dmxaddr", json_integer(dev->baseaddr));

	json_object_set_new(lobj, "r", json_integer(dev->r));
	json_object_set_new(lobj, "g", json_integer(dev->g));
	json_object_set_new(lobj, "b", json_integer(dev->b));

	json_object_set_new(json, dev->name, lobj);
}

static void espnet_json_handler(void *arg, json_t *json,
		enum json_subtype type)
{
	struct espnet_sink *sink = arg;
	for (struct espnet_device *d = sink->devs; d; d = d->next)
		espnet_json_one(sink, d, json, type);
}

static void espnet_can_handler(void *arg, struct can_message *msg)
{
}

static struct espnet_device *espnet_add_dev(struct espnet_sink *sink,
		json_t *config)
{
	struct espnet_device *d;

	if (!json_is_object(config)) {
		lprintf("ESPnet device config must be an object/dictionary");
		return NULL;
	}
	if (!json_is_string(json_object_get(config, "name"))) {
		lprintf("ESPnet device config must specify str 'name'");
		return NULL;
	}
	if (!json_is_integer(json_object_get(config, "baseaddr"))) {
		lprintf("ESPnet device config must specify int 'baseaddr'");
		return NULL;
	}

	d = calloc(sizeof(*d), 1);
	d->sink = sink;
	d->baseaddr = json_integer_value(json_object_get(config, "baseaddr"));
	d->name = strdup(json_string_value(json_object_get(config, "name")));
	return d;
}

int espnet_init_conf(json_t *config)
{
	struct espnet_sink *sink;
	int universe = 0;
	const char *iface;
	int ifindex;
	struct espnet_device **devp;

	if (!json_is_object(config)) {
		lprintf("ESPnet config must be an object/dictionary");
		return 1;
	}
	if (!json_is_string(json_object_get(config, "interface"))) {
		lprintf("ESPnet config must have a string 'interface' key");
		return 1;
	}
	if (!json_is_array(json_object_get(config, "devices"))) {
		lprintf("ESPnet config must have an array 'devices' key");
		return 1;
	}

	json_t *univ = json_object_get(config, "universe");
	if (univ && !json_is_integer(univ)) {
		lprintf("ESPnet universe number must be integer if present");
		return 1;
	}
	if (univ)
		universe = json_integer_value(univ);
	if (universe > 255 || universe < 0) {
		lprintf("ESPnet supports universes 0 to 255");
		return 1;
	}

	iface = json_string_value(json_object_get(config, "interface"));
	ifindex = if_nametoindex(iface);

	if (ifindex == 0) {
		lprintf("ESPnet interface '%s' error: %s",
			iface, strerror(errno));
		return 1;
	}

	sink = calloc(sizeof(*sink), 1);
	sink->ifname = strdup(iface);
	sink->ifindex = ifindex;
	sink->universe = universe;

	struct ip_mreqn mrn = { .imr_ifindex = ifindex };
	struct sockaddr_in addr = { .sin_family = AF_INET,
		.sin_port = htons(ESPNET_PORT) };
	int mone = -1;

	sink->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sink->fd < 0
		|| setsockopt(sink->fd, SOL_IP, IP_MULTICAST_IF,
				&mrn, sizeof(mrn))
		|| setsockopt(sink->fd, SOL_SOCKET, SO_BROADCAST,
				&mone, sizeof(mone))
		|| setsockopt(sink->fd, SOL_SOCKET, SO_BINDTODEVICE,
				iface, strlen(iface) + 1)
		|| bind(sink->fd, (struct sockaddr *)&addr, sizeof(addr))) {
		lprintf("ESPnet interface '%s' initalisation error: %s",
			iface, strerror(errno));
		free(sink->ifname);
		free(sink);
		return 1;
	}

	json_t *devcfg = json_object_get(config, "devices");
	devp = &sink->devs;
	for (size_t i = 0; i < json_array_size(devcfg); i++) {
		struct espnet_device *dev;
		json_t *c = json_array_get(devcfg, i);
		dev = espnet_add_dev(sink, c);
		if (!dev) {
			close(sink->fd);
			free(sink->ifname);
			free(sink);
			return 1;
		}
		*devp = dev;
		devp = &dev->next;
	}

	sink->u = can_register_alloc(sink, espnet_can_handler,
		"ESPnet[%s#%d]", sink->ifname, universe);
	sink->u->json = espnet_json_handler;

	sink->writer = event_new(ev_base, -1, 0, espnet_sink_writer, sink);
	struct timeval tvs = { .tv_sec = 0, .tv_usec = ESPNET_TIMER };
	event_add(sink->writer, &tvs);

	*psinks = sink;
	psinks = &sink->next;
	return 0;
}
