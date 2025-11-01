#include <string.h>
#include <stdlib.h>
#include "../include/cli.h"

CliArgs parse_cli_args(int argc, char **argv) {
    CliArgs args = {0};
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            args.config = argv[++i];
    }
    return args;
}