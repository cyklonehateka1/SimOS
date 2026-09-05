#!/bin/sh
set -eu

work="$(mktemp -d)"
controller_pid=""
agent_pid=""
cleanup() {
    [ -z "$agent_pid" ] || kill "$agent_pid" 2>/dev/null || true
    [ -z "$controller_pid" ] || kill "$controller_pid" 2>/dev/null || true
    rm -rf "$work"
}
trap cleanup EXIT INT TERM
port=$((20000 + ($$ % 20000)))

printf 'db_path: "%s/events.db"\nlog_path: "%s/controller.log"\nlisten_port: %s\nnodes: []\n' "$work" "$work" "$port" >"$work/config.yaml"

(
    # Leave room for slower CI runners to accept and acknowledge the child.
    sleep 2
    echo nodes
    echo "status test-node"
    echo "ping test-node"
    echo "exec test-node printf 'hello from child'"
    sleep 1
    echo history
    echo shutdown
) | ./simos --config "$work/config.yaml" >"$work/controller.out" 2>"$work/controller.err" &
controller_pid=$!
sleep 0.3
./simos-agent --name test-node --port "$port" >"$work/agent.out" 2>"$work/agent.err" &
agent_pid=$!

wait "$controller_pid"
controller_pid=""
wait "$agent_pid" || true
agent_pid=""

assert_output() {
    if ! grep -q "$1" "$work/controller.out"; then
        echo "missing expected output: $1" >&2
        cat "$work/controller.out" >&2
        cat "$work/controller.err" >&2
        cat "$work/controller.log" >&2
        exit 1
    fi
}
assert_output "test-node"
assert_output "node status"
assert_output "hello from child"
assert_output "event journal"
echo "integration test passed"
