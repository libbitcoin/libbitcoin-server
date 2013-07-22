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

install:
	cp bin/obbalancer bin/obworker /usr/local/bin/
	cp worker/download-blockchain.sh /usr/local/bin/download-blockchain
	chmod +x /usr/local/bin/download-blockchain
	cp worker/obworker.cfg /etc/
	cd client && $(MAKE) install

