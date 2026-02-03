#!/usr/bin/env bash
# Tiny64 convenience script – builds and boots the OS in QEMU.
# Accepts any extra args and forwards them to `make`.
set -euo pipefail

cd "$(dirname "$0")"

# Rebuild everything and launch QEMU (see Makefile `run` target).
make run "$@"
