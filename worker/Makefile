default: obworker

node_impl.o: node_impl.cpp
	g++ -I../ -c node_impl.cpp -o node_impl.o $(shell pkg-config --cflags libbitcoin)
config.o: config.cpp
	g++ -I../ -c config.cpp -o config.o $(shell pkg-config --cflags libbitcoin)
echo.o: echo.cpp
	g++ -I../ -c echo.cpp -o echo.o $(shell pkg-config --cflags libbitcoin)
worker.o: worker.cpp
	g++ -I../ -c worker.cpp -o worker.o $(shell pkg-config --cflags libbitcoin)
service/blockchain.o: service/blockchain.cpp
	g++ -I../ -c service/blockchain.cpp -o service/blockchain.o $(shell pkg-config --cflags libbitcoin)
main.o: main.cpp
	g++ -I../ -c main.cpp -o main.o $(shell pkg-config --cflags libbitcoin)

obworker: main.o worker.o config.o node_impl.o echo.o service/blockchain.o
	g++ ../shared/zmq_message.o node_impl.o service/blockchain.o config.o main.o ../shared/message.o worker.o echo.o -lzmq -lconfig++ $(shell pkg-config --libs libbitcoin) -o obworker

