#ifndef runnerdManager_H
#define RUNNERDMANAGER_H

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>
#include <cstdlib>
#include <cstring>

#include <string>
#include <vector>

#include "runnerdWorker.h"

#define RUNNERDMANAGER_DEBUG

class runnerdManager
{
private:
    std::vector <pollfd> sockets;
    runnerdWorker worker;    

    static void printDebugOutput(const char *message, ...);
public:
    runnerdManager (){}
    ~runnerdManager (){}

    void setTimeout (const int t);

    int addNewSocket (const int tcp_port);
    int addNewSocket (const char *unex_socket);

    int startWaitClients ();
};

#endif //RUNNERDMANAGER_H
