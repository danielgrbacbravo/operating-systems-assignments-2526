
#!/bin/bash

echo "=== DISK USAGE ==="

df -h | awk 'NR > 1 {
  mount_point = $NF
  usage_with_percent = $(NF-1)

  usage_number = usage_with_percent
  gsub(/%/, "", usage_number)

  health_status = (usage_number+0 > 90) ? "CRITICAL" : "OK"
  print mount_point " : " usage_with_percent " [" health_status "]"
}'


echo "=== MEMORY ==="
free -m | awk '/^Mem:/ {
  total_mb = $2
  used_mb  = $3
  if (total_mb == 0) total_mb = 1

  usage_percent = int(used_mb * 100 / total_mb)
  health_status = (usage_percent >= 80) ? "CRITICAL" : "OK"

  print used_mb "/" total_mb " MB (" usage_percent "%) [" health_status "]"
}'

echo "=== LOAD AVERAGE ==="

load_section=$(uptime | sed 's/.*load average: //')

load_1min=$(echo "$load_section" | cut -d',' -f1 | xargs)
load_5min=$(echo "$load_section" | cut -d',' -f2 | xargs)
load_15min=$(echo "$load_section" | cut -d',' -f3 | xargs)

echo "1-min: $load_1min | 5-min: $load_5min | 15-min: $load_15min"
