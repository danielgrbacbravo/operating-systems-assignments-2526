#!/bin/bash

echo "=== SERVICE MONITOR ==="

while IFS= read -r service; do
  [ -z "$service" ] && continue

  status=$(systemctl is-active "$service")

  if [ "$status" = "active" ]; then
    echo "$service : $status [ OK ]"
  else
    printf "%s : %s [ ALERT ] -> Restarting ... " "$service" "$status"
    systemctl start "$service"
    echo "done"
  fi
done
