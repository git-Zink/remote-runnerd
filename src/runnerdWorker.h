#ifndef RUNNERDWORKER_H
#define RUNNERDWORKER_H

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <pthread.h>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <cstdarg>
#include <errno.h>
#include <cstdlib>
#include <cstring>
#include <wait.h>


#include <string>
#include <iostream>
#include <fstream>
#include <set>

#include <queue>
#include <algorithm>


#define RUNNERDWORKER_DEBUG

class runnerdWorker
{
private:
    static const int signals_count = 32;

    //all for allowed commands
    std::string config_path;
    std::set <std::string> allowed_commands;
    static volatile bool need_reload_config;

    static void sigHandler (int signum);

    pthread_mutex_t mutex_commands_list;
    pthread_t pth_refresh_loop;
    static void* refreshBusyLoop (void * arg);

    int refreshCommands();

    //work with client
    struct data_do_client
    {
        int fd;
        pollfd *p_strcut_fd;
        std::set <std::string> commands;
        int timeout;
    };
    static void* dealWithClient (void *arg);
    int timeout; //time for execute one client's command

    //general thread "selecting"
    std::queue <int> q_clients;
    static const int disconnected_fd = -2;
    static const int poll_timeout = 100;
    static bool detectDisconnected(const pollfd client);

    pthread_t pth_select;
    volatile bool is_selecting;
    static void* selectingClients (void *arg);

    pthread_mutex_t mutex_q_clients;

    static void printDebugOutput(const char *message, ...);

public:
    runnerdWorker();
    ~runnerdWorker();

    int addClient (int clientfd);
    void setTimeout (const int t);
};

#endif //RUNNERDWORKER_H
