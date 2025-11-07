#ifndef CLI_H
#define CLI_H

typedef struct {
    const char *config;
} CliArgs;

CliArgs parse_cli_args(int argc, char **argv);
void parse_cli_command(const char *input_line);

#endif