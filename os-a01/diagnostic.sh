#!/bin/bash
echo "========================================"
echo "       SYSTEM DIAGNOSTIC REPORT"
echo "========================================"

echo "=== DISK USAGE ==="

df -h | awk '
$1 == "Filesystem" { next }
NF >= 6 {
  mount_point = $NF
  usage_with_percent = $(NF-1)

  usage_number = usage_with_percent
  gsub("%", "", usage_number)

  health_status = (usage_number+0 > 90) ? "CRITICAL" : "OK"
  print mount_point " : " usage_with_percent " [" health_status "]"
}'

echo "=== MEMORY ==="
free -m | awk '
$1 == "Mem:" {
  total_mb = $2
  used_mb  = $3
  if (total_mb == 0) total_mb = 1

  usage_percent = int(used_mb * 100 / total_mb)
  health_status = (usage_percent >= 80) ? "CRITICAL" : "OK"

  print used_mb "/" total_mb " MB (" usage_percent "%) [" health_status "]"
}'

echo "=== LOAD AVERAGE ==="

uptime | awk '
{
  pos = index($0, "load average:")
  if (pos == 0) pos = index($0, "load averages:")  # mac os bullshi
  if (pos == 0) { exit 0 }

  s = substr($0, pos + length("load average:"))
  gsub("^[ \t]+", "", s)

  # s now like: "0.50, 0.30, 0.20"
  n = split(s, a, ",")

  # trim leading spaces from each
  for (i = 1; i <= n; i++) {
    gsub("^[ \t]+", "", a[i])
    gsub("[ \t]+$", "", a[i])
  }

  print "1-min: " a[1] " | 5-min: " a[2] " | 15-min: " a[3]
}'



echo "=== SERVICE MONITOR ==="

while IFS= read -r service; do
  [ -z "$service" ] && continue

  service_status=$(systemctl is-active "$service")

  if [ "$service_status" = "active" ]; then
    echo "$service : $service_status [OK]"
  else
    printf "%s : %s [ALERT] -> Restarting... " "$service" "inactive"
    systemctl start "$service"
    echo "done"
  fi
done


echo "=== BRUTE FORCE DETECTION ==="

threats=$(
  (grep -s "Failed password" "$AUTH_LOG" || true) | awk '{
      for (i=1; i<=NF; i++) if ($i=="from") { print $(i+1); break }
    }' | sort | uniq -c | sort -nr | awk '$1 > 10 { print $2 " : " $1 " failed attempts [THREAT]" }'
)

if [ -z "$threats" ]; then
  echo "No threats detected"
else
  echo "$threats"
fi

echo "=== SYSTEM EVENTS ==="

oom=$(
  (grep -s "Out of memory" "$SYSLOG" || true) | awk '{
      for (i=1; i<=NF; i++) {
        if ($i=="process") {
          pid=$(i+1)
          name=$(i+2)
          gsub(/[()]/, "", name)
          print "OOM: Process " name " (PID " pid ") killed"
          break
        }
      }
    }'
)

io=$(
  (grep -s -E "I/O error|I / O error" "$SYSLOG" || true) | awk '{
      for (i=1; i<=NF; i++) {
        if ($i=="dev") {
          dev=$(i+1)
          gsub(/,/, "", dev)
          print "I/O: Error on device " dev
          break
        }
      }
    }'
)

if [ -z "$oom" ] && [ -z "$io" ]; then
  echo "No system events detected"
else
  [ -n "$oom" ] && echo "$oom"
  [ -n "$io" ] && echo "$io"
fi

echo "========================================"
echo "         END OF REPORT"
echo "========================================"

exit 0
