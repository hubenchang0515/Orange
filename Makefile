all : server.c client.c
	gcc -o server server.c -W -Wall
	gcc -o client client.c -W -Wall

USE_LUA : 
	gcc -o server server.c -W -Wall -DUSE_LUA -llua -lm
	gcc -o client client.c -W -Wall -DUSE_LUA -llua -lm

clean :
	rm server client
