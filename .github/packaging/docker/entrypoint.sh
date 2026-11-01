#!/bin/sh
set -e

directory=/opt/libbitcoin-server
features="$("${directory}/selector")"

supports()
{
    for feature in "$@"; do
        echo "${features}" | grep -qx "${feature}" || return 1
    done
}

variant=
while read -r name requires; do
    if supports ${requires}; then
        variant="${name}"
        break
    fi
done < "${directory}/variants"

if [ -z "${variant}" ]; then
    echo "libbitcoin-server: this processor supports no packaged variant of bs." >&2
    exit 1
fi

if [ ! -f /data/bs.cfg ]; then
    cp "${directory}/bs.cfg" /data/bs.cfg
fi

exec "${directory}/${variant}" --config /data/bs.cfg "$@"
