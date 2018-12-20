#!/bin/bash

ssh-add -l

set -e

: "${HETZNER_USER=cldrdrinc}"
: "${HETZNER_PASSWD=}"

function fatal_error {
  echo "$*"
  exit 1
}

function get_hosts {
  curl -su "$HETZNER_USER:$HETZNER_PASSWD" "https://robot-ws.your-server.de/server"
}

function get_prod_hosts {
  FILE=$(mktemp)
  get_hosts | jq -r '.[].server.server_name | select(contains("ext.prd.rnfrst.com"))' > "$FILE"
  echo "$FILE"
}

function get_stg_hosts {
  FILE=$(mktemp)
  get_hosts | jq -r '.[].server.server_name | select(contains("ext.stg.rnfrst.com"))' > "$FILE"
  echo "$FILE"
}

case $1 in
  production)
    SERVERS=$(get_prod_hosts)
    ;;
  staging)
    SERVERS=$(get_stg_hosts)
    ;;
  development)
    ;;
  *)
    echo "Unknown environment '$1' (valid choices are 'production', 'staging', or 'development')"
    exit 1
    ;;
esac

if [ -n "$SERVERS" ]; then
  parallel-ssh -h "$SERVERS" -l root service webrtc restart
fi
