# SimOS

SimOS is a distributed miniature operating system: a central kernel-style controller manages child nodes over TCP. Its shell can inspect node health, test connectivity, schedule commands, broadcast work, and retain an event journal.

## Build and run

Install a C11 compiler, `make`, and libyaml (`brew install libyaml` on macOS or `apt install libyaml-dev` on Ubuntu), then:

```sh
make
./simos
```

In other terminals, attach child nodes:

```sh
./simos-agent --name node1
./simos-agent --name node2
```

At the SimOS shell, run `help`, `nodes`, `status all`, `ping node1`, or `exec node1 uname -a`. Use another configuration with `./simos --config path/to/config.yaml`.

## Containers

```sh
docker compose build
docker compose up
```

## Protocol and safety

Controller and agents exchange newline-delimited JSON. Agents register with `hello`; the controller acknowledges with `ack`; runtime messages include `ping`, `pong`, `status`, `exec`, and `result`.

This is an educational simulator. `exec` invokes `/bin/sh -c`; run it only on a trusted private network.
