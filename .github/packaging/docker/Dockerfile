FROM ubuntu:24.04

ARG TARGETARCH

COPY --chmod=0755 ${TARGETARCH}/ /opt/libbitcoin-server/
COPY --chmod=0755 entrypoint.sh /opt/libbitcoin-server/entrypoint
COPY --chmod=0644 bs.cfg /opt/libbitcoin-server/bs.cfg

USER 1000:1000
WORKDIR /data
VOLUME /data
EXPOSE 8333 8080 8180

ENTRYPOINT ["/opt/libbitcoin-server/entrypoint"]
