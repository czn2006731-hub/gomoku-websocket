HEADERS := $(wildcard *.hpp)

go:gobang.cc $(HEADERS)
	g++ -o $@ $< -std=c++11 -pthread -lmysqlclient -ljsoncpp -lcrypto -I/usr/include/mysql -I/usr/include/jsoncpp
