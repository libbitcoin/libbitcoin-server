default: obclient

backend.o: backend.cpp
	g++ -I../ -c backend.cpp -o backend.o $(shell pkg-config --cflags libbitcoin)
message.o: message.cpp
	g++ -I../ -c message.cpp -o message.o $(shell pkg-config --cflags libbitcoin)
main.o: main.cpp
	g++ -I../ -c main.cpp -o main.o $(shell pkg-config --cflags libbitcoin)
interface.o: interface.cpp
	g++ -I../ -c interface.cpp -o interface.o $(shell pkg-config --cflags libbitcoin)

obclient: interface.o main.o message.o backend.o
	g++ ../shared/zmq_message.o interface.o main.o message.o backend.o -lzmq $(shell pkg-config --libs libbitcoin) -o obclient

