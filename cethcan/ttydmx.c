#include "cethcan.h"

#include <limits.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termio.h>
#include <linux/serial.h>
#include <pthread.h>
#include <stdatomic.h>

struct ttydmx_device;

struct ttydmx_data {
	size_t len;
	uint8_t data[];
};

struct ttydmx_sink {
	struct ttydmx_sink *next;

	struct can_user *u;
	int fd;
	char *ttydev;
	size_t maxaddr;

	pthread_t pusher;
	struct ttydmx_data * _Atomic nextdata;

	struct ttydmx_device *devs;
};

struct ttydmx_device {
	struct ttydmx_device *next;
	struct ttydmx_sink *sink;

	char *name;
	unsigned baseaddr;
	uint8_t r, g, b;
};

static struct ttydmx_sink *sinks = NULL, **psinks = &sinks;

struct ttydmx_device *ttydmx_find(const char *name)
{
	for (struct ttydmx_sink *sink = sinks; sink; sink = sink->next)
		for (struct ttydmx_device *d = sink->devs; d; d = d->next)
			if (!strcmp(d->name, name))
				return d;
	return NULL;
}

int ttydmx_set(struct ttydmx_device *dev, unsigned r, unsigned g, unsigned b)
{
	struct ttydmx_sink *sink = dev->sink;
	struct ttydmx_data *newdata, *prevdata;
	struct ttydmx_device *walk;
	bool bump = dev->r != r || dev->g != g || dev->b != b;

	dev->r = r;
	dev->g = g;
	dev->b = b;

	newdata = calloc(offsetof(struct ttydmx_data, data[sink->maxaddr]), 1);
	newdata->len = sink->maxaddr;
	for (walk = sink->devs; walk; walk = walk->next) {
		newdata->data[walk->baseaddr + 0] = walk->r;
		newdata->data[walk->baseaddr + 1] = walk->g;
		newdata->data[walk->baseaddr + 2] = walk->b;
	}
	prevdata = atomic_exchange(&sink->nextdata, newdata);
	if (prevdata)
		free(prevdata);

	if (bump)
		json_bump_longpoll();
	return 0;
}

static void ttydmx_osc_set(void *arg, struct osc_element *e)
{
	struct ttydmx_device *dev = arg;
	unsigned r,g,b;

	if (!e || !e->next || !e->next->next)
		return;

	if (e->type == OSC_INT32
	    && e->next->type == OSC_INT32
	    && e->next->next->type == OSC_INT32) {
		r = ((struct osc_int32*)e)->value;
		g = ((struct osc_int32*)e->next)->value;
		b = ((struct osc_int32*)e->next->next)->value;
	} else if (e->type == OSC_FLOAT32
	    && e->next->type == OSC_FLOAT32
	    && e->next->next->type == OSC_FLOAT32) {
		r = 255 * ((struct osc_float32*)e)->value;
		g = 255 * ((struct osc_float32*)e->next)->value;
		b = 255 * ((struct osc_float32*)e->next->next)->value;
	} else {
		return;
	}

	ttydmx_set(dev, r, g, b);
}

void ttydmx_get(struct ttydmx_device *dev,
		unsigned *r, unsigned *g, unsigned *b)
{
	*r = dev->r;
	*g = dev->g;
	*b = dev->b;
}

static void ttydmx_json_one(struct ttydmx_sink *sink,
		struct ttydmx_device *dev,
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

static void ttydmx_json_handler(void *arg, json_t *json,
		enum json_subtype type)
{
	struct ttydmx_sink *sink = arg;
	for (struct ttydmx_device *d = sink->devs; d; d = d->next)
		ttydmx_json_one(sink, d, json, type);
}

static void ttydmx_can_handler(void *arg, struct can_message *msg)
{
}

static struct ttydmx_device *ttydmx_add_dev(struct ttydmx_sink *sink,
		json_t *config)
{
	struct ttydmx_device *d;

	if (!json_is_object(config)) {
		lprintf("ttyDMX device config must be an object/dictionary");
		return NULL;
	}
	if (!json_is_string(json_object_get(config, "name"))) {
		lprintf("ttyDMX device config must specify str 'name'");
		return NULL;
	}
	if (!json_is_integer(json_object_get(config, "baseaddr"))) {
		lprintf("ttyDMX device config must specify int 'baseaddr'");
		return NULL;
	}

	d = calloc(sizeof(*d), 1);
	d->sink = sink;
	d->baseaddr = json_integer_value(json_object_get(config, "baseaddr"));
	d->name = strdup(json_string_value(json_object_get(config, "name")));

	if (d->baseaddr + 4 > sink->maxaddr)
		sink->maxaddr = d->baseaddr + 4;

	char buf[1024];
	snprintf(buf, sizeof(buf), "/dmx/%s/%s/value", sink->ttydev, d->name);
	lprintf("Adding dmx osc endpoint %s", buf);
	osc_server_add_method(osc_server, buf, ttydmx_osc_set, d);

	return d;
}

static void *ttydmx_thread(void *arg)
{
	struct ttydmx_sink *sink = arg;
	struct ttydmx_data *data = NULL, *newdata;
	uint8_t dummy = 0;

	while (1) {
		newdata = atomic_exchange(&sink->nextdata, NULL);
		if (newdata) {
			free(data);
			data = newdata;
		}

		ioctl(sink->fd, TIOCSBRK, 0);
		usleep(100);
		ioctl(sink->fd, TIOCCBRK, 0);
		usleep(12);
		write(sink->fd, data ? data->data : &dummy,
				data ? data->len : 1);
		tcdrain(sink->fd);
		usleep(10000);
	}
	return NULL;
}

int ttydmx_init_conf(json_t *config)
{
	struct ttydmx_sink *sink;
	struct ttydmx_device **devp;
	const char *ttydev;
	char ttydevfull[PATH_MAX];
	struct stat st;
	struct termios termios;
	struct serial_struct serial;

	if (!json_is_object(config)) {
		lprintf("ttyDMX config must be an object/dictionary");
		return 1;
	}
	if (!json_is_string(json_object_get(config, "ttydev"))) {
		lprintf("ttyDMX config must have a string 'ttydev' key");
		return 1;
	}
	if (!json_is_array(json_object_get(config, "devices"))) {
		lprintf("ttyDMX config must have an array 'devices' key");
		return 1;
	}

	ttydev = json_string_value(json_object_get(config, "ttydev"));
	if (strchr(ttydev, '/'))
		snprintf(ttydevfull, sizeof(ttydevfull), "%s", ttydev);
	else
		snprintf(ttydevfull, sizeof(ttydevfull), "/dev/%s", ttydev);

	if (stat(ttydevfull, &st)) {
		lprintf("ttyDMX: stat(\"%s\") failed: %s",
			ttydevfull, strerror(errno));
		return 1;
	}
	if (!S_ISCHR(st.st_mode)) {
		lprintf("ttyDMX: \"%s\" needs to be a character device!",
			ttydevfull);
		return 1;
	}

	sink = calloc(sizeof(*sink), 1);
	sink->ttydev = strdup(ttydevfull);
	sink->fd = open(ttydevfull, O_RDWR | O_NOCTTY);
	if (sink->fd < 0) {
		lprintf("ttyDMX: failed to open \"%s\": %s",
			ttydevfull, strerror(errno));
		free(sink->ttydev);
		free(sink);
		return 1;
	}

	memset(&serial, 0, sizeof(serial));
	if (ioctl(sink->fd, TIOCGSERIAL, &serial))
		goto out_setup_err;

	serial.flags &= ~ASYNC_SPD_MASK;
	serial.flags |= ASYNC_SPD_CUST;
	serial.custom_divisor = serial.baud_base / 250000;

	if (ioctl(sink->fd, TIOCSSERIAL, &serial))
		goto out_setup_err;
	if (ioctl(sink->fd, TIOCGSERIAL, &serial))
		goto out_setup_err;
	lprintf("ttyDMX: \"%s\": divisor: %d, result rate: %d\n",
		ttydevfull,
		(int)serial.custom_divisor,
		(int)serial.baud_base / serial.custom_divisor);

	if (tcgetattr(sink->fd, &termios))
		goto out_setup_err;
	cfsetispeed(&termios, B38400);
	cfsetospeed(&termios, B38400);
	cfmakeraw(&termios);
	termios.c_cflag |= CLOCAL | CREAD | CSTOPB;
	termios.c_cflag &= ~CRTSCTS;
	if (tcsetattr(sink->fd, TCSANOW, &termios))
		goto out_setup_err;

	json_t *devcfg = json_object_get(config, "devices");
	devp = &sink->devs;
	for (size_t i = 0; i < json_array_size(devcfg); i++) {
		struct ttydmx_device *dev;
		json_t *c = json_array_get(devcfg, i);
		dev = ttydmx_add_dev(sink, c);
		if (!dev) {
			close(sink->fd);
			free(sink->ttydev);
			free(sink);
			return 1;
		}

		*devp = dev;
		devp = &dev->next;
	}

	sink->u = can_register_alloc(sink, ttydmx_can_handler,
		"ttyDMX[%s]", sink->ttydev);
	sink->u->json = ttydmx_json_handler;

	if (pthread_create(&sink->pusher, NULL, ttydmx_thread, sink))
		lprintf("ttyDMX[%s]: failed to start thread!", sink->ttydev);

	*psinks = sink;
	psinks = &sink->next;
	return 0;

out_setup_err:
	lprintf("ttyDMX: setup error on \"%s\": %s",
		ttydevfull, strerror(errno));
	close(sink->fd);
	free(sink->ttydev);
	free(sink);
	return 1;
}
