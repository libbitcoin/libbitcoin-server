all: bin/obclient bin/obbalancer bin/obworker

shared:
	cd src && $(MAKE)

bin/obclient: shared
	cd client && $(MAKE)
	mkdir -p bin
	mv client/obclient bin/

bin/obbalancer: shared
	cd balancer && $(MAKE)
	mv balancer/obbalancer bin/

bin/obworker: shared
	cd worker && $(MAKE)
	mv worker/obworker bin/

PREFIX=/usr/local

install:
	cp bin/obbalancer bin/obworker $(PREFIX)/bin/
	cp worker/download-blockchain.sh $(PREFIX)/download-blockchain
	chmod +x $(PREFIX)/download-blockchain
	cp -r include/obelisk $(PREFIX)/include/
	cp worker/obworker.cfg /etc/
	cd client && $(MAKE) install

