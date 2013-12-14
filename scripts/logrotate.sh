/var/lib/blockchain/*.log {
    size 5M
    missingok
    rotate 10
    compress
    delaycompress
    notifempty
    create 644 ob ob
    sharedscripts
}
