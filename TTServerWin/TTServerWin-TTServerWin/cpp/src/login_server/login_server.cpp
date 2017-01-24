/*
 * login_server.cpp
 *
 *  Created on: 2013-6-21
 *      Author: ziteng@mogujie.com
 */
//login_server：一台负载均衡服务器，用来通知客户端连接到负载最小的msg_server
/************************************************************************/
/* login_server在整个TT的架构中，可以简单的理解为一个负载均衡的作用，在login_server中，
同样在内存中维护了所有的msg_server的地址以及其目前的负载情况
在LoginConn.cpp中定义了UserConnCntMap_t结构体来保存msg_server的状态机负载*/
/************************************************************************/

#include "LoginConn.h"
#include "netlib.h"
#include "ConfigFileReader.h"
//#include "version.h"

void client_callback(void* callback_data, uint8_t msg, uint32_t handle, void* pParam)
{
	if (msg == NETLIB_MSG_CONNECT)
	{
		CLoginConn* pConn = new CLoginConn();
		pConn->OnConnect2(handle, LOGIN_CONN_TYPE_CLIENT);
	}
	else
	{
		log("!!!error msg: %d\n", msg);
	}
}

// this callback will be replaced by imconn_callback() in OnConnect()
void msg_serv_callback(void* callback_data, uint8_t msg, uint32_t handle, void* pParam)
{
	if (msg == NETLIB_MSG_CONNECT)
	{
		CLoginConn* pConn = new CLoginConn();
		pConn->OnConnect2(handle, LOGIN_CONN_TYPE_MSG_SERV);
	}
	else
	{
		log("!!!error msg: %d\n", msg);
	}
}

int main(int argc, char* argv[])
{
	if ((argc == 2) && (strcmp(argv[1], "-v") == 0)) {
		//printf("Server Version: LoginServer/%s\n", VERSION);
		printf("Server Build: %s %s\n", __DATE__, __TIME__);
		return 0;
	}

#ifndef WIN32
/************************************************************************/
    /* 当服务器close一个连接时，若client端接着发数据。
    根据TCP 协议的规定，会收到一个RST响应，client再往这个服务器发送数据时，系统会发出一个SIGPIPE信号给进程，告诉进程这个连接已经断开了，不要再写了。 
    根据信号的默认处理规则SIGPIPE信号的默认执行动作是terminate(终止、退出),所以client会退出。
    若不想客户端退出可以把SIGPIPE设为SIG_IGN 
    如:    signal(SIGPIPE,SIG_IGN); 
    这时SIGPIPE交给了系统处理。                                                                     */
/************************************************************************/
	signal(SIGPIPE, SIG_IGN);//http://www.cnblogs.com/wainiwann/p/3899176.html
#endif

	CConfigFileReader config_file("loginserver.conf");

	char* client_listen_ip = config_file.GetConfigName("ClientListenIP");
	char* str_client_port = config_file.GetConfigName("ClientPort");
	char* msg_server_listen_ip = config_file.GetConfigName("MsgServerListenIP");
	char* str_msg_server_port = config_file.GetConfigName("MsgServerPort");

	if (!client_listen_ip || !str_client_port || !msg_server_listen_ip || !str_msg_server_port) {
		log("config item missing, exit...\n");
		return -1;
	}

	uint16_t client_port = atoi(str_client_port);
	uint16_t msg_server_port = atoi(str_msg_server_port);

	int ret = netlib_init();

	if (ret == NETLIB_ERROR)
		return ret;

	CStrExplode client_listen_ip_list(client_listen_ip, ';');
	for (uint32_t i = 0; i < client_listen_ip_list.GetItemCnt(); i++) {
		ret = netlib_listen(client_listen_ip_list.GetItem(i), client_port, client_callback, NULL); //设置回调:CBaseSocket.m_callback 和 CBaseSocket.m_callback_data 
		if (ret == NETLIB_ERROR)
			return ret;
	}

	CStrExplode msg_server_listen_ip_list(msg_server_listen_ip, ';');
	for (uint32_t i = 0; i < msg_server_listen_ip_list.GetItemCnt(); i++) {
		ret = netlib_listen(msg_server_listen_ip_list.GetItem(i), msg_server_port, msg_serv_callback, NULL);
		if (ret == NETLIB_ERROR)
			return ret;
	}

	printf("server start listen on:\nFor client %s:%d\nFor MsgServer: %s:%d\n",
			client_listen_ip, client_port, msg_server_listen_ip, msg_server_port);

	init_login_conn();

	printf("now enter the event loop...\n");

	netlib_eventloop();//进入事件循环(接收请求/发送响应)

	return 0;
}
