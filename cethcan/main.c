#include "cethcan.h"

struct event_base *ev_base;

int main(int argc, char **argv)
{
	int optch = 0;
	const char *cfgfile = "cethcan.json";
	json_error_t je;
	json_t *config;

	do {
		optch = getopt(argc, argv, "c:");
		switch (optch) {
		case 'c':
			cfgfile = optarg;
			break;
		case -1:
			break;
		}
	} while (optch != -1);

	if (optind < argc) {
		fprintf(stderr, "leftover arguments\n");
		return 1;
	}

	config = json_load_file(cfgfile, JSON_REJECT_DUPLICATES, &je);
	if (!config) {
		fprintf(stderr, "failed to load config:\n%s:%d:%d %s\n",
			je.source, je.line, je.column, je.text);
		return 1;
	}
	if (!json_is_object(config)) {
		fprintf(stderr, "config must be object/dictionary\n");
		return 1;
	}

	ev_base = event_base_new();

	can_init();

	json_t *ethercfg = json_object_get(config, "ethernet");
	for (size_t i = 0; i < json_array_size(ethercfg); i++) {
		json_t *c = json_array_get(ethercfg, i);
		if (ether_init(c))
			return 1;
	}

	json_t *lightcfg = json_object_get(config, "lights");
	for (size_t i = 0; i < json_array_size(lightcfg); i++) {
		json_t *c = json_array_get(lightcfg, i);
		if (light_init_conf(c))
			return 1;
	}

	json_t *beancfg = json_object_get(config, "beans");
	for (size_t i = 0; i < json_array_size(beancfg); i++) {
		json_t *c = json_array_get(beancfg, i);
		if (bean_init_conf(c))
			return 1;
	}

	json_t *socancfg = json_object_get(config, "socketcan");
	for (size_t i = 0; i < json_array_size(socancfg); i++) {
		json_t *c = json_array_get(socancfg, i);
		if (socan_init(c))
			return 1;
	}

	json_t *espcfg = json_object_get(config, "espnet");
	for (size_t i = 0; i < json_array_size(espcfg); i++) {
		json_t *c = json_array_get(espcfg, i);
		if (espnet_init_conf(c))
			return 1;
	}

	http_init();

	json_decref(config);

	event_base_loop(ev_base, 0);
	return 0;
}
