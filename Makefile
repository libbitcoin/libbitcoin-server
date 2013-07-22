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
	cd client && $(MAKE) install

