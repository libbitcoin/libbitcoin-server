default: obclient

backend.o: backend.cpp
	g++ -c backend.cpp -o backend.o $(shell pkg-config --cflags libbitcoin)
message.o: message.cpp
	g++ -c message.cpp -o message.o $(shell pkg-config --cflags libbitcoin)
main.o: main.cpp
	g++ -c main.cpp -o main.o $(shell pkg-config --cflags libbitcoin)
interface.o: interface.cpp
	g++ -c interface.cpp -o interface.o $(shell pkg-config --cflags libbitcoin)

obclient: interface.o main.o message.o backend.o
	g++ interface.o main.o message.o backend.o -lzmq $(shell pkg-config --libs libbitcoin) -o obclient

