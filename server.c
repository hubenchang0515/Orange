#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#ifdef USE_LUA
#	include <lualib.h>
#	include <lauxlib.h>
#endif

#ifndef SOCKET_MAX
#	define SOCKET_MAX 1024
#endif
/* Porwaiding map */
int sockets[SOCKET_MAX];


/* port to listen MC Player */
uint16_t player_port;

/* port to listen Orange Client */
uint16_t orange_client_port;


/* read config file */
int config();
/* Get listener to Orange Client */
int client_listen();
/* Get connection to Orange Client */
int start(int client);
/* get listener to player */
int player_listen();
/* main loop */
int mainloop(int client,int client_listener, int player_listener);


int main(void)
{
	while(1)
	{
		if(config() != 0)
		{
			break;
		}
		
		int client_listener = client_listen();
		if(client_listener == -1)
		{
			break;
		}
		
		int client = start(client_listener);
		if(client == -1)
		{
			break;
		}
		
		int player_listener = player_listen();
		if(player_listener == -1)
		{
			break;
		}
		
		if(mainloop(client, client_listener, player_listener) == -1)
		{
			break;
		}
		
		return 0;
	}
	printf("%s\n",strerror(errno));
	return 1;
}


/* read config file */
int config()
{
#ifdef USE_LUA
	lua_State* L = luaL_newstate();
	if(L == NULL) // create lua state failed
	{
		return -1;
	}
	
	if(luaL_dofile(L,"server_config.lua") != 0)
	{
		lua_error(L);
	}
	
	lua_getglobal(L,"PLAYER_PORT");
	player_port = luaL_checkinteger(L,-1);
	lua_getglobal(L,"ORANGE_CLIENT_PORT");
	orange_client_port = luaL_checkinteger(L,-1);
	lua_close(L);
#else
	player_port = 25565;
	orange_client_port = 25566;
#endif
	return 0;
}


/* Get listener to Orange Client */
int client_listen()
{
	int fd = socket(AF_INET,SOCK_STREAM,0);
	if(fd == -1) // create socket failed
	{
		return -1;
	}
	
	int n = 1;
	if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR, &n, sizeof(n)) < 0)
	{
		printf("Set Socket Option Failed\n");
	}
	
	/* Port listen to Orange client */
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(orange_client_port);
	
	if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
	{
		close(fd);
		return -1;
	}
	
	if(listen(fd,1) == -1)
	{
		close(fd);
		return -1;
	}
	printf("Listen %u to wait Orange Client.\n",orange_client_port);
	return fd;

}


/* get a connection to Orange Client */
int start(int listener)
{
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	int orange_client = accept(listener, (struct sockaddr*)&addr, &len);
	printf("Orange Client %s:%u .\n",inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));
	
	return orange_client;
}


/* get listener to player */
int player_listen()
{
	int fd = socket(AF_INET,SOCK_STREAM,0);
	if(fd == -1) // create socket failed
	{
		return -1;
	}
	
	int n = 1;
	if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR, &n, sizeof(n)) < 0)
	{
		printf("Set Socket Option Failed\n");
	}
	
	/* Port listen to Orange client */
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(player_port);
	
	if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
	{
		close(fd);
		return -1;
	}
	
	if(listen(fd,1) == -1)
	{
		close(fd);
		return -1;
	}
	printf("Listen %u to wait Player.\n",player_port);
	
	return fd;
}


/* main loop */
int mainloop(int client,int client_listener, int player_listener)
{
	/* Create epoll */
	int epoll = epoll_create(SOCKET_MAX);
	if(epoll == -1)
	{
		return -1;
	}
	
	/* Add player_listener to epoll */
	struct epoll_event event;
	event.data.fd = player_listener;
	event.events = EPOLLIN;
	epoll_ctl(epoll, EPOLL_CTL_ADD, player_listener, &event);
	
	
	/* get addr */
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
			
	/* get data */
	char data[1024];
	
	while(epoll_wait(epoll, &event, 1, -1) > 0)
	{
		if(event.data.fd == player_listener) // new player join
		{
			len = sizeof(addr);
			int fd = accept(player_listener, (struct sockaddr*)&addr, &len);
			if(fd == -1)
			{
				printf("Cannot accept.\n");
			}
			
			printf("Player %s:%u .\n",inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));

			write(client,"new",3); // tell client create a new connection
			sockets[0] = fd; //sockets[0] means wait connection
			
			/* donot accept more player momentarily */
			event.data.fd = player_listener;
			epoll_ctl(epoll, EPOLL_CTL_DEL, player_listener, &event);
			
			/* allow to accept from Orange Client */
			event.data.fd = client_listener;
			epoll_ctl(epoll, EPOLL_CTL_ADD, client_listener, &event);
		}
		else if(event.data.fd == client_listener)
		{
			len = sizeof(addr);
			int fd = accept(client_listener, (struct sockaddr*)&addr, &len);
			if(fd == -1)
			{
				printf("Cannot accept.\n");
			}
			/* bind two sockets */
			sockets[fd] = sockets[0];
			sockets[sockets[0]] = fd;
			event.data.fd = fd;
			epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event);
			event.data.fd = sockets[0];
			epoll_ctl(epoll, EPOLL_CTL_ADD, sockets[0], &event);
			
			/* donot accept from Orange Client momentarily */
			event.data.fd = client_listener;
			epoll_ctl(epoll, EPOLL_CTL_DEL, client_listener, &event);
			
			/* allow to accept more player */
			event.data.fd = player_listener;
			epoll_ctl(epoll, EPOLL_CTL_ADD, player_listener, &event);
		}
		else // forwarding
		{
			int fd = event.data.fd;
			ssize_t data_len = read(fd, data, 1024);
			if(data_len > 0)
			{
				write(sockets[fd], data, data_len);
			}
			else
			{
				epoll_ctl(epoll, EPOLL_CTL_DEL, fd, &event);
				epoll_ctl(epoll, EPOLL_CTL_DEL, sockets[fd], &event);
				close(sockets[fd]);
				close(fd);
			}
		}
	}
	
	return 0;
}
