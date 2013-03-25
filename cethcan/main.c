#include "cethcan.h"

struct event_base *ev_base;
json_t *config;

int main(int argc, char **argv)
{
	int optch = 0;
	const char *cfgfile = "cethcan.json";
	json_error_t je;

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

	event_base_loop(ev_base, 0);
	return 0;
}
