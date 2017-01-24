/*
 * route_server.cpp
 *
 *  Created on: 2013-7-4
 *      Author: ziteng@mogujie.com
 一台消息转发服务器，客户端消息发送到msg_server。msg_server判断接收者是否在本地，是的话，直接转发给目标客户端。否的话，转发给route_server。
 route_server接收到msg_server的消息后，获取to_id所在的msg_server，将消息转发给msg_server。msg_server再将消息转发给目标接收者。
 1、route_server 在整个tt中的作用是一个消息转发的地方，其在内存中维护了全局用户信息。当有多个msg_server的时候，route_server的作用就是在多个msg_server之间中转消息。
 2、g_rs_user_map是一个hash_map，保存了全局用户信息,当有用户上线的时候，msg_server会将该用户的状态发送到route_server，route_server就会在g_rs_user_map里面插入一条记录
 */

#include "RouteConn.h"
#include "netlib.h"
#include "ConfigFileReader.h"
#include "time.h"
//#include "version.h"

// this callback will be replaced by imconn_callback() in OnConnect()
void route_serv_callback(void* callback_data, uint8_t msg, uint32_t handle, void* pParam)
{
	if (msg == NETLIB_MSG_CONNECT)
	{
		CRouteConn* pConn = new CRouteConn();
		pConn->OnConnect(handle);
	}
	else
	{
		log("!!!error msg: %d\n", msg);
	}
}

int main(int argc, char* argv[])
{
	if ((argc == 2) && (strcmp(argv[1], "-v") == 0)) {
	// 	printf("Server Version: RouteServer/%s\n", VERSION);
		printf("Server Build: %s %s\n", __DATE__, __TIME__);
		return 0;
	}

#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
	srand(time(NULL));
#endif

	CConfigFileReader config_file("routeserver.conf");

	char* listen_ip = config_file.GetConfigName("ListenIP");
	char* str_listen_msg_port = config_file.GetConfigName("ListenMsgPort");

	if (!listen_ip || !str_listen_msg_port) {
		log("config item missing, exit...\n");
		return -1;
	}

	uint16_t listen_msg_port = atoi(str_listen_msg_port);

	int ret = netlib_init();

	if (ret == NETLIB_ERROR)
		return ret;

	CStrExplode listen_ip_list(listen_ip, ';');
	for (uint32_t i = 0; i < listen_ip_list.GetItemCnt(); i++) {
		ret = netlib_listen(listen_ip_list.GetItem(i), listen_msg_port, route_serv_callback, NULL);
		if (ret == NETLIB_ERROR)
			return ret;
	}

	printf("server start listen on: %s:%d\n", listen_ip,  listen_msg_port);

	init_routeconn_timer_callback();

	printf("now enter the event loop...\n");

	netlib_eventloop();

	return 0;
}

