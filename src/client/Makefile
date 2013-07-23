default: client_test

backend.o: backend.cpp
	g++ -I../include -c backend.cpp -o backend.o $(shell pkg-config --cflags libbitcoin)
interface/blockchain.o: interface/blockchain.cpp
	g++ -I../include -c interface/blockchain.cpp -o interface/blockchain.o $(shell pkg-config --cflags libbitcoin)
interface/interface.o: interface/interface.cpp
	g++ -I../include -c interface/interface.cpp -o interface/interface.o $(shell pkg-config --cflags libbitcoin)

main.o: main.cpp
	g++ -I../include -c main.cpp -o main.o $(shell pkg-config --cflags libbitcoin)

client_test: main.o
	g++ main.o $(OBELISK_LIB_PATH) -lobelisk -lzmq $(shell pkg-config --libs libbitcoin) -o client_test

