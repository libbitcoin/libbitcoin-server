default: obclient libobelisk_client.a

backend.o: backend.cpp
	g++ -I../include -c backend.cpp -o backend.o $(shell pkg-config --cflags libbitcoin)
interface.o: interface.cpp
	g++ -I../include -c interface.cpp -o interface.o $(shell pkg-config --cflags libbitcoin)

libobelisk_client.a: interface.o main.o backend.o
	ar rvs libobelisk_client.a ../src/zmq_message.o interface.o ../src/message.o backend.o

main.o: main.cpp
	g++ -I../include -c main.cpp -o main.o $(shell pkg-config --cflags libbitcoin)

obclient: libobelisk_client.a main.o
	g++ main.o libobelisk_client.a -lzmq $(shell pkg-config --libs libbitcoin) -o obclient

