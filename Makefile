all: server client

server: server.c csa.c
	gcc -g -o server server.c csa.c

client: client.c csa.c
	gcc -g -o client client.c csa.c