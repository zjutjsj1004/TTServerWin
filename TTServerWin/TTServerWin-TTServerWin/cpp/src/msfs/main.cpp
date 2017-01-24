/*================================================================
 *   Copyright (C) 2014 All rights reserved.
 *
 *   @       File              :  main.cpp
 *   @       Author      :  Zhang Yuanhao
 *   @       Email         :  bluefoxah@gmail.com
 *   @       Date           :  2014年7月29日
 *   @       Version     :   1.0
 *   @  Description	:msfs  是小文件存储系统，主要是用来保存用户头像以及聊天中产生的图片、语音等小文件。msfs 提供的一个简单的http服务
     msfs在启动的时候，会在该目录下产生256个目录，每个目录下面再产生256个子目录。
     FileCnt用来记录已经存储的文件数目,该配置在msfs关闭的时候会被程序重写。
     FilesPerDir每个目录下面最多保存多少个小文件，GetThreadCount获取小文件的线程数目，PostThreadCount上传小文件的线程数目。
     建议GetThreadCount + PostThreadCount = 内核数目，GetThreadCount >= PostThreadCount。
     所以msfs总共可以存储的文件数目为:256*256*FilesPerDir
 ================================================================*/

#include <iostream>
#include <signal.h>
#include "netlib.h"
#include "ConfigFileReader.h"
#include "HttpConn.h"
#include "FileManager.h"
#include "ThreadPool.h"
#ifdef WIN32
#include <time.h>
#include <direct.h>
#define mkdir(path,mode) _mkdir(path)
#endif

using namespace std;
using namespace msfs;


FileManager* FileManager::m_instance = NULL;
FileManager* g_fileManager = NULL;
CThreadPool g_PostThreadPool;
CThreadPool g_GetThreadPool;

#ifndef WIN32
void closeall(int fd)
{
    int fdlimit = sysconf(_SC_OPEN_MAX);

    while (fd < fdlimit)
        close(fd++);
}

int daemon(int nochdir, int noclose, int asroot)
{
    switch (fork())
    {
        case 0:  break;
        case -1: return -1;
        default: _exit(0);          /* exit the original process */
    }

    if (setsid() < 0)               /* shoudn't fail */
        return -1;

    if ( !asroot && (setuid(1) < 0) )              /* shoudn't fail */
        return -1;

    /* dyke out this switch if you want to acquire a control tty in */
    /* the future -- not normally advisable for daemons */

    switch (fork())
    {
        case 0:  break;
        case -1: return -1;
        default: _exit(0);
    }

    if (!nochdir)
        chdir("/");

    if (!noclose)
    {
        closeall(0);
        dup(0); dup(0);
    }

    return 0;
}
#endif

// for client connect in
void http_callback(void* callback_data, uint8_t msg, uint32_t handle,
        void* pParam)
{
    if (msg == NETLIB_MSG_CONNECT)
    {
        CHttpConn* pConn = new CHttpConn();
//        CHttpTask* pTask = new CHttpTask(handle, pConn);
//        g_ThreadPool.AddTask(pTask);
        pConn->OnConnect(handle);
    } else
    {
        log("!!!error msg: %d\n", msg);
    }
}

#ifndef WIN32
void doQuitJob()
{
	char fileCntBuf[20] = {0};
	snprintf(fileCntBuf, 20, "%llu", g_fileManager->getFileCntCurr());
    	config_file.SetConfigValue("FileCnt", fileCntBuf);
	FileManager::destroyInstance();
netlib_destroy();
    log("I'm ready quit...\n");
}
void Stop(int signo)
{
    switch(signo)
    {
    case SIGINT:
    case SIGTERM:
    case SIGQUIT:
        doQuitJob();
        _exit(0);
        break;
    case SIGHUP:
        break;
    default:
        cout<< "unknown signal"<<endl;
        _exit(0);
    }
}
#endif

int main(int argc, char* argv[])
{

#ifndef WIN32
    for(int i=0; i < argc; ++i)
       {
           if(strncmp(argv[i], "-d", 2) == 0)
           {
               if(daemon(1, 0, 1) < 0)
               {
                   cout<<"daemon error"<<endl;
                   return -1;
               }
               break;
           }
	}
	log("MsgServer max files can open: %d\n", getdtablesize());
#endif

	CConfigFileReader config_file("msfs.conf");

    char* listen_ip = config_file.GetConfigName("ListenIP");
    char* str_listen_port = config_file.GetConfigName("ListenPort");
    char* base_dir = config_file.GetConfigName("BaseDir");
    char* str_file_cnt = config_file.GetConfigName("FileCnt");
    char* str_files_per_dir = config_file.GetConfigName("FilesPerDir");
    char* str_post_thread_count = config_file.GetConfigName("PostThreadCount");
    char* str_get_thread_count = config_file.GetConfigName("GetThreadCount");

    if (!listen_ip || !str_listen_port || !base_dir || !str_file_cnt || !str_files_per_dir || !str_post_thread_count || !str_get_thread_count)
    {
        log("config file miss, exit...\n");
        return -1;
    }
    
    log("%s,%s\n",listen_ip, str_listen_port);
    uint16_t listen_port = atoi(str_listen_port);
    long long int  fileCnt = atoll(str_file_cnt);
    int filesPerDir = atoi(str_files_per_dir);
    int nPostThreadCount = atoi(str_post_thread_count);
    int nGetThreadCount = atoi(str_get_thread_count);
    if(nPostThreadCount <= 0 || nGetThreadCount <= 0)
    {
        log("thread count is invalied");
        return -1;
    }
    g_PostThreadPool.Init(nPostThreadCount);
    g_GetThreadPool.Init(nGetThreadCount);

    g_fileManager = FileManager::getInstance(listen_ip, base_dir, fileCnt, filesPerDir);
	int ret = g_fileManager->initDir();
	if (ret) {
		printf("The BaseDir is set incorrectly :%s\n",base_dir);
		return ret;
    }
	ret = netlib_init();
    if (ret == NETLIB_ERROR)
        return ret;

    CStrExplode listen_ip_list(listen_ip, ';');
    for (uint32_t i = 0; i < listen_ip_list.GetItemCnt(); i++)
    {
        ret = netlib_listen(listen_ip_list.GetItem(i), listen_port,
                http_callback, NULL);
        if (ret == NETLIB_ERROR)
            return ret;
    }

#ifndef WIN32
    signal(SIGINT, Stop);
    signal (SIGTERM, Stop);
    signal (SIGHUP, Stop);
    signal (SIGQUIT, Stop);
#endif

    printf("server start listen on: %s:%d\n", listen_ip, listen_port);
    init_http_conn();
    printf("now enter the event loop...\n");

    netlib_eventloop();
    return 0;
}


