default: zmq_message.o message.o

zmq_message.o: zmq_message.cpp
	g++ -I../include -c zmq_message.cpp -o zmq_message.o $(shell pkg-config --cflags libbitcoin)
message.o: message.cpp
	g++ -I../include -c message.cpp -o message.o $(shell pkg-config --cflags libbitcoin)

