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
    message="This processor supports no packaged variant of bs."

    # The last variant is the baseline, so its features are the minimum.
    required="$(tail -n 1 "${directory}/variants" | cut -s -d ' ' -f 2-)"
    detected="$(echo "${features}" | paste -sd ' ' -)"
    echo "libbitcoin-server: ${message}" >&2
    echo "libbitcoin-server: required: ${required}" >&2
    echo "libbitcoin-server: detected: ${detected}" >&2

    if [ -n "${BS_UNSUPPORTED_PORT}" ]; then
        echo "libbitcoin-server: serving status page on port ${BS_UNSUPPORTED_PORT}." >&2
        exec perl -MIO::Socket::INET -e '
            my ($port, $message, $required, $detected) = @ARGV;
            my $timeout = 5;
            $SIG{TERM} = $SIG{INT} = sub { exit 0 };            my $body = "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                . "<title>Libbitcoin Server: unsupported processor</title></head>"
                . "<body style=\"font-family:sans-serif;max-width:40em;margin:3em auto\">"
                . "<h1>Unsupported processor</h1><p>$message</p>"
                . "<p>Minimum CPU features: <code>$required</code></p>"
                . "<p>Detected: <code>$detected</code></p>"
                . "<p>Libbitcoin Server is not running.</p></body></html>";
            my $server = IO::Socket::INET->new(LocalPort => $port, Listen => 16,
                ReuseAddr => 1) or die "listen on $port: $!\n";
            while (my $client = $server->accept) {
                eval {
                    local $SIG{ALRM} = sub { die "timeout\n" };
                    alarm $timeout;
                    while (defined(my $line = <$client>)) { last if $line =~ /^\r?$/ }
                    print $client "HTTP/1.1 503 Service Unavailable\r\n"
                        . "Content-Type: text/html; charset=utf-8\r\n"
                        . "Content-Length: " . length($body) . "\r\n"
                        . "Connection: close\r\n\r\n" . $body;
                    alarm 0;
                };
                alarm 0;
                close $client;
            }
            die "accept on $port: $!\n";' "${BS_UNSUPPORTED_PORT}" "${message}" "${required}" "${detected}"
    fi

    exit 1
fi

if [ ! -f /data/bs.cfg ]; then
    cp "${directory}/bs.cfg" /data/bs.cfg
fi

exec "${directory}/${variant}" --config /data/bs.cfg "$@"
