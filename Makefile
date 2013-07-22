all: bin/obclient bin/obbalancer bin/obworker

bin/obclient:
	cd client && $(MAKE)
	mkdir -p bin
	mv client/obclient bin/

bin/obbalancer:
	cd balancer && $(MAKE)
	mv balancer/obbalancer bin/

bin/obworker:
	cd worker && $(MAKE)
	mv worker/obworker bin/

install:
	cp bin/obbalancer bin/obworker /usr/local/bin/
	cd worker && $(MAKE) install

