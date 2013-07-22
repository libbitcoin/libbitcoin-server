INSTALL_PREFIX=/usr/local

all: libobelisk.a bin/obbalancer bin/obworker

src/zmq_message.o:
	cd src && $(MAKE) zmq_message.o
src/message.o:
	cd src && $(MAKE) message.o
shared: src/zmq_message.o src/message.o

bin/obbalancer: shared
	cd balancer && $(MAKE)
	mkdir -p bin/
	mv balancer/obbalancer bin/

bin/obworker: shared
	cd worker && $(MAKE)
	mkdir -p bin/
	mv worker/obworker bin/

client/backend.o:
	cd client && $(MAKE) backend.o
client/interface.o:
	cd client && $(MAKE) interface.o
client: client/backend.o client/interface.o

libobelisk.a: shared client
	ar rvs libobelisk.a src/zmq_message.o client/interface.o src/message.o client/backend.o

install:
	cp bin/obbalancer bin/obworker $(INSTALL_PREFIX)/bin/
	cp worker/download-blockchain.sh $(INSTALL_PREFIX)/download-blockchain
	chmod +x $(INSTALL_PREFIX)/download-blockchain
	cp -r include/obelisk $(INSTALL_PREFIX)/include/
	cp worker/obworker.cfg /etc/
	cp libobelisk.a $(INSTALL_PREFIX)/lib/
	mkdir -p $(INSTALL_PREFIX)/lib/pkgconfig
	cp libobelisk.pc $(INSTALL_PREFIX)/lib/pkgconfig/

