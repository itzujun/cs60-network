all: p2p/p2pApp
p2p/p2p.o: p2p/p2p.c p2p/p2p.h common/config.h
	gcc -g -pthread -c p2p/p2p.c -o p2p/p2p.o
p2p/p2pApp: p2p/p2p.o p2p/p2pApp.c 
	gcc -g -pthread p2p/p2pApp.c p2p/p2p.o -o p2p/p2pApp

clean:
	rm -rf p2p/*.o
	rm -rf p2p/p2pApp



