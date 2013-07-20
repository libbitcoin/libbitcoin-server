default: message.o zmq_message.o

message.o: message.cpp
	g++ -I../ -c message.cpp -o message.o $(shell pkg-config --cflags libbitcoin)
zmq_message.o: zmq_message.cpp
	g++ -I../ -c zmq_message.cpp -o zmq_message.o $(shell pkg-config --cflags libbitcoin)

