#!/bin/bash

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
  # find "load average:" then take the remainder of the line
  pos = index($0, "load average:")
  if (pos == 0) pos = index($0, "load averages:")  # just in case
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
