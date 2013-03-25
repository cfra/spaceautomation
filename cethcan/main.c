#include "cethcan.h"

struct event_base *ev_base;

int main(int argc, char **argv)
{
	int optch = 0;
	const char *cfgfile = "cethcan.json";
	json_error_t je;
	json_t *config, *ethercfg;

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

	ethercfg = json_object_get(config, "ethernet");
	for (size_t i = 0; i < json_array_size(ethercfg); i++) {
		json_t *c = json_array_get(ethercfg, i);
		if (ether_init(c))
			return 1;
	}

	event_base_loop(ev_base, 0);
	return 0;
}
