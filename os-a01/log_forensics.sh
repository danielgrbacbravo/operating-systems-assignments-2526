#!/bin/bash

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

exit 0
