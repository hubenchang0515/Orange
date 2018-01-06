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


/* port to connect MC server */
char mc_ipv4[32];
uint16_t mc_port;

/* port to connect Orange Client */
char orange_server_ipv4[32];
uint16_t orange_server_port;



/* read config file */
int config();
/* connect to Orange Server */
int connect_server();
/* connect to MC Server */
int connect_mc();
/* main loop */
int mainloop(int orange);

int main()
{
	while(1)
	{
		if(config() != 0)
		{
			break;
		}
		
		int server = connect_server();
		if(server == -1)
		{
			break;
		}

		if( mainloop(server) == -1)
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
	
	if(luaL_dofile(L,"client_config.lua") != 0)
	{
		lua_error(L);
	}
	
	lua_getglobal(L,"MC_PORT");
	mc_port = luaL_checkinteger(L,-1);
	lua_getglobal(L,"ORANGE_SERVER_PORT");
	orange_server_port = luaL_checkinteger(L,-1);
	lua_getglobal(L,"MC_IPv4");
	strncpy(mc_ipv4, luaL_checkstring(L,-1),32);
	lua_getglobal(L,"ORANGE_SERVER_IPv4");
	strncpy(orange_server_ipv4, luaL_checkstring(L,-1),32);
	lua_close(L);
#else
	strncpy(mc_ipv4,"0.0.0.0",32);
	strncpy(orange_server_ipv4,"0.0.0.0",32); // TODO set IPv4 address of orange_server here
	mc_port = 25565;
	orange_server_port = 25566;
#endif
	return 0;
}


/* connect to Orange Server */
int connect_server()
{
	int server = socket(AF_INET, SOCK_STREAM, 0);
	
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(orange_server_ipv4);
	addr.sin_port = htons(orange_server_port);
	connect(server, (struct sockaddr*)&addr, sizeof(addr) );
	
	return server;
}


/* connect to MC Server */
int connect_mc()
{
	int mc = socket(AF_INET, SOCK_STREAM, 0);
	
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(mc_ipv4);
	addr.sin_port = htons(mc_port);
	connect(mc, (struct sockaddr*)&addr, sizeof(addr) );
	
	return mc;
}

/* main loop */
int mainloop(int orange)
{
	/* Create epoll */
	int epoll = epoll_create(SOCKET_MAX);
	if(epoll == -1)
	{
		return -1;
	}
	
	/* Add orange to epoll */
	struct epoll_event event;
	event.data.fd = orange;
	event.events = EPOLLIN;
	epoll_ctl(epoll, EPOLL_CTL_ADD, orange, &event);
	
	/* data */
	char data[1024];

	while(epoll_wait(epoll, &event, 1, -1) > 0)
	{
		if(event.data.fd == orange)
		{
			if(read(orange, data, 1024) <= 0)
			{
				return -1;
			}
			
			int orange2 = connect_server();
			int mc = connect_mc();
			
			if(mc == -1 || orange2 == -1)
			{
				return -1;
			}

			printf("New forwarding.\n");
			sockets[mc] = orange2;
			sockets[orange2] = mc;
			
			event.data.fd = mc;
			epoll_ctl(epoll, EPOLL_CTL_ADD, mc, &event);
			event.data.fd = orange2;
			epoll_ctl(epoll, EPOLL_CTL_ADD, orange2, &event);
		}
		else
		{
			int fd = event.data.fd;
			socklen_t len = read(fd, data, 1024);
			if(len > 0)
			{
				write(sockets[fd], data, len);
			}
			else
			{
				epoll_ctl(epoll, EPOLL_CTL_DEL, fd, &event);
				epoll_ctl(epoll, EPOLL_CTL_DEL, sockets[fd], &event);
				close(fd);
				close(sockets[fd]);
			}
		}
	}
	
	
	return 0;
}
