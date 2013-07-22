default: client_test

backend.o: backend.cpp
	g++ -I../include -c backend.cpp -o backend.o $(shell pkg-config --cflags libbitcoin)
interface.o: interface.cpp
	g++ -I../include -c interface.cpp -o interface.o $(shell pkg-config --cflags libbitcoin)

main.o: main.cpp
	g++ -I../include -c main.cpp -o main.o $(shell pkg-config --cflags libbitcoin)

client_test: main.o
	g++ main.o $(OBELISK_LIB_PATH) -lobelisk -lzmq $(shell pkg-config --libs libbitcoin) -o client_test

