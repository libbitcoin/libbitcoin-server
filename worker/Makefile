default: obworker

worker.o: worker.cpp
	g++ -I../ -c worker.cpp -o worker.o $(shell pkg-config --cflags libbitcoin)
main.o: main.cpp
	g++ -I../ -c main.cpp -o main.o $(shell pkg-config --cflags libbitcoin)

obworker: main.o worker.o
	g++ ../shared/zmq_message.o main.o ../shared/message.o worker.o -lzmq $(shell pkg-config --libs libbitcoin) -o obworker

