#!/bin/bash
while [ 1 ]; do
    echo "Polling a bunch of addresses..."
    ADDRS=$(python latest-addrs.py)
    for ADDR in $ADDRS; do
        echo "Query start: $ADDR $(date +%s)"
        if [[ "$ADDR" == "1VayNert3x1KzbpzMGt2qdqrAThiRovi8" ]]; then
            continue
        fi
        sx history $ADDR
        echo $?
        if [ "$?" -ne "0" ]; then
            echo "Bad return code! $?"
            exit 1
        fi
    done
done

