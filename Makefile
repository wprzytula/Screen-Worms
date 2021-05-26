flags="-std=c++17 -O2 -Wall -Wextra"

all: screen-worms-server screen-worms-client

screen-worms-server: build/server_main.o build/Server.o build/err.o build/Game.o build/Buffer.o
	g++ $(flags) -o $@ $<

screen-worms-client: build/client_main.o build/Client.o build/err.o
	g++ $(flags) -o $@ $<

build/err.o: Common/err.cpp Common/err.h
	g++ $(flags) -c -o $@ $<

build/gai_sock_factory.o: Client/gai_sock_factory.cpp Common/err.h
	g++ $(flags) -c -o $@ $<

build/Server.o: Server/Server.cpp Server/Server.h Server/GameConstants.h Server/Game.h Server/RandomGenerator.h Server/Board.h Server/Player.h Server/Pixel.h Common/Epoll.h
	g++ $(flags) -c -o $@ $<

build/Game.o: Server/Game.cpp Server/GameConstants.h Server/Game.h Server/RandomGenerator.h Server/Board.h Server/Player.h Server/Pixel.h Common/Buffer.h
	g++ $(flags) -c -o $@ $<

build/Buffer.o: Common/Buffer.cpp Common/Buffer.h Common/Crc32Computer.h
	g++ $(flags) -c -o $@ $<

build/Client.o: Client/Client.cpp Client/Client.h Common/Buffer.h Common/Crc32Computer.h Common/Epoll.h Common/ClientHeartbeat.h
	g++ $(flags) -c -o $@ $<

clean:
	rm -rf build