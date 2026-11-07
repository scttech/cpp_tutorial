#!/usr/bin/env bash
# Runs a job through the sandbox image with the same hardening the service uses.
# usage: smoke.sh '<job json>'
set -euo pipefail
printf '%s' "$1" | docker run --rm -i \
  --network none --read-only --tmpfs /work:rw,exec,nosuid,size=64m,uid=10001,gid=10001,mode=0700 --tmpfs /tmp:rw,noexec,nosuid,size=64m,uid=10001,gid=10001,mode=1777 \
  --cap-drop ALL --security-opt no-new-privileges \
  --pids-limit 128 --memory 512m --cpus 1 \
  zth-sandbox
