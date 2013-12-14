/var/lib/blockchain/*.log {
    size 5M
    missingok
    rotate 10
    compress
    delaycompress
    notifempty
    sharedscripts
}
