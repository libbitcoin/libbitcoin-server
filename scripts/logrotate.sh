/var/log/libbitcoin/*.log {
    size 5M
    missingok
    rotate 0
    compress
    delaycompress
    notifempty
    create 644 lb lb
    sharedscripts
}

