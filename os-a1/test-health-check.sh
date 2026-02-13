#!/bin/bash

echo "=== DISK USAGE ==="

df -h | awk '
  $1 == "Filesystem" { next }   # skip header
  NF >= 6 {
    mount_point = $6
    usage_with_percent = $5

    usage_number = usage_with_percent
    gsub(/%/, "", usage_number)

    health_status = (usage_number+0 > 90) ? "CRITICAL" : "OK"
    print mount_point " : " usage_with_percent " [" health_status "]"
  }
'
