default: obbalancer

config.o:
	g++ -c config.cpp -o config.o $(shell pkg-config --cflags libbitcoin)
balancer.o:
	g++ -c balancer.cpp -o balancer.o $(shell pkg-config --cflags libbitcoin)

obbalancer: config.o balancer.o
	g++ balancer.o config.o -lzmq -lconfig++ -o obbalancer $(shell pkg-config --libs libbitcoin)

