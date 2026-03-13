#!/bin/sh
# Robust service check that handles multiple `systemctl is-active` output formats.
# Supports:
#  - Single-word output: "active" / "inactive" / "failed" ...
#  - A list of service names (one per line) -> presence means active
#  - A table/section format:
#      [services]
#      nginx active
#      mysql active
#
# Usage: check_service.sh <service-name>
# Output: "<service> : <state>" where <state> is one-word (active|inactive|failed|unknown|...)
# Exit: 0 when state == "active", non-zero otherwise
#
# Notes:
#  - This script favors machine-readable queries and tolerates varied output from wrappers.
#  - It falls back to `systemctl show -p ActiveState --value` when parsing is ambiguous.
#  - If systemd/tools are not available, it will attempt simple process checks as a best-effort.
#
# POSIX/sh compatible.

set -u

SERVICE=${1:-}
if [ -z "$SERVICE" ]; then
  printf 'Usage: %s <service-name>\n