all : server client

server : server.c
	gcc -o server server.c -W -Wall -DUSE_LUA -llua -lm
	
client : client.c
	gcc -o client client.c -W -Wall -DUSE_LUA -llua -lm
	
clean :
	rm server client
