# Orange
一个简单的Minecraft服务器NAT穿透代理，采用epoll，仅支持Linux，且仅支持IPv4。
仅随手所写，也未详细测试，可能存在大量BUG，欢迎指正。

## 编译
### 不使用Lua作为配置文件
编译前需要在源码中进行ip地址和端口的设置
server.c第101行
```C
#else
	player_port = 25565;         
	orange_client_port = 25566;
#endif
```

client.c第92行
```C
#else
	strncpy(mc_ipv4,"0.0.0.0",32);
	strncpy(orange_server_ipv4,"0.0.0.0",32); // TODO 通常只需将Orange服务器的ip地址写在这里即可
	mc_port = 25565;
	orange_server_port = 25566;
#endif
```


```bash
make
```


### 使用Lua作为配置文件
由于依赖Lua5.3作为配置文件，因此需要安装Lua。

* 从Lua官网下载Lua5.3的源码
```bash
wget https://www.lua.org/manual/5.3/
```

* 解压后进入源码目录，编译安装
```bash
make
sudo make install
```

* 回到Orange的目录编译Orange
```bash
make USE_LUA
```

## 运行
1. 在具有静态公网ip的服务器上运行Orange的server
2. 在MC服务器所在的计算机上运行Orange的client

## 配置
Orange的server和client各自均有配置文件，但通常只需修改client_config.lua
* server_config.lua的配置，通常不需要修改
```Lua
PLAYER_PORT = 25565  -- 玩家连接Orange服务器用的端口号

ORANGE_CLIENT_PORT = 25566  -- Orange客户端连接Orange服务器的端口号
```

* client_config.lua的配置，通常只需要修改ORANGE_SERVER_IPv4
```Lua
ORANGE_SERVER_IPv4 = 'xx.xx.xx.xx' -- Orange服务器的IP地址
ORANGE_SERVER_PORT = 25566         -- Orange客户端连接Orange服务器的端口号

MC_IPv4 = '0.0.0.0'  -- MC服务器的IP地址，
MC_PORT = 25565      -- MC服务器的端口号，25565是默认端口
```

## 工作原理
1. Orange Server启动，等待Orange Client连接
2. Orange Client启动，连接Orange Server，获得SOCKET A，用于交流
3. 玩家连接Orange Server，获得SOCKET X，用于转发
4. Orange Server通过SOCKET　A 通知Orange Client
5. Orange Client再连接Orange Server，获得SOCKET Y，用于转发
6. Orange Client连接Minecraft的服务器，获得SOCKET Z，用于转发
```
玩家 --X--> Orange Server --Y--> Orange Client --Z--> Minecraft
```
