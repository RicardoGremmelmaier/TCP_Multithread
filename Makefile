all: server client

server: server.c
	gcc -o server server.c -lpthread -lssl -lcrypto -Wno-deprecated-declarations

client: client.c
	gcc -o client client.c -lssl -lcrypto -Wno-deprecated-declarations

clean:
	rm -f client server recebido_*
