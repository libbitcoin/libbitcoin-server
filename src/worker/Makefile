default: obworker

node_impl.o: node_impl.cpp
	g++ -I../include -c node_impl.cpp -o node_impl.o $(shell pkg-config --cflags libbitcoin)
config.o: config.cpp
	g++ -I../include -c config.cpp -o config.o $(shell pkg-config --cflags libbitcoin)
echo.o: echo.cpp
	g++ -I../include -c echo.cpp -o echo.o $(shell pkg-config --cflags libbitcoin)
worker.o: worker.cpp
	g++ -I../include -c worker.cpp -o worker.o $(shell pkg-config --cflags libbitcoin)
service/blockchain.o: service/blockchain.cpp
	g++ -I../include -c service/blockchain.cpp -o service/blockchain.o $(shell pkg-config --cflags libbitcoin)
publisher.o: publisher.cpp
	g++ -I../include -c publisher.cpp -o publisher.o $(shell pkg-config --cflags libbitcoin)
main.o: main.cpp
	g++ -I../include -c main.cpp -o main.o $(shell pkg-config --cflags libbitcoin)

obworker: main.o worker.o config.o node_impl.o echo.o publisher.o service/blockchain.o
	g++ ../src/zmq_message.o node_impl.o service/blockchain.o config.o publisher.o main.o ../src/message.o worker.o echo.o -lzmq -lconfig++ $(shell pkg-config --libs libbitcoin) -o obworker

