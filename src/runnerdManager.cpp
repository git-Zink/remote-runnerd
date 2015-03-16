#include "runnerdManager.h"

void runnerdManager::printDebugOutput(const char *message, ...)
{
#ifdef RUNNERDMANAGER_DEBUG
    va_list args;
    va_start (args, message);
    vprintf (message, args);
    fflush(stdout);
    va_end (args);
#endif //runnerdManager_DEBUG
}

int runnerdManager::startWaitClients()
{
    printDebugOutput("ascceptingClients:\tsockets size = %lu\n", sockets.size());
    if (sockets.empty())
    {
        printDebugOutput("ascceptingClients:\tsockets list is empty\n");
        return -1;
    }

    while (true)
    {
        int res = poll (&sockets[0], sockets.size(), -1); //wait for ever
        printDebugOutput("acceptClients:\tta-da\n");
        if (res == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                printDebugOutput("acceptClients:\tpoll serious err - %s\n", strerror (errno));
                exit (-1);
            }
        }

        for (unsigned i = 0; i < sockets.size(); ++i)
        {
            printDebugOutput("acceptClients:\trevents = 0x%X\n", sockets[i].revents);
            if (sockets[i].revents & POLLIN)
            {
                printDebugOutput("acceptClients:\tNew client\n");
                int new_client = accept (sockets[i].fd, 0, 0);
                if (new_client >= 0)
                {
                    std::string hello = "Hi from server\n";
                    write (new_client, hello.c_str(), hello.length());
                    printDebugOutput("acceptingClients:\tsend hello to client\n");

                    if (worker.addClient(new_client) != 0)
                    {
                        printDebugOutput("acceptingClients:\tadd client failed\n");
                    }
                }
                else
                {
                    printDebugOutput("acceptingClients:\taccept failed\n");
                }
            }
        }
    }
}

void runnerdManager::setTimeout(const int t)
{
    worker.setTimeout (t);
}

int runnerdManager::addNewSocket(const int tcp_port)
{
    sockaddr_in settings;
    memset (&settings, 0, sizeof (settings));

    settings.sin_family = AF_INET;
    settings.sin_addr.s_addr = htonl (INADDR_ANY);
    settings.sin_port = htons (tcp_port);

    short new_socket = socket (AF_INET, SOCK_STREAM, 0);

    if (bind (new_socket, (sockaddr*)&settings, sizeof (settings)) != 0)
    {
        printDebugOutput("addNewSocket:\tAF_INET bind failed with %s\n", strerror (errno));
        return -1;
    }

    if (listen (new_socket, 10) != 0)
    {
        printDebugOutput("addNewSocket:\tSet AF_INET to passive failed\n");
        return -2;
    }

    sockets.emplace_back((pollfd){new_socket, POLLIN, 0});
    return 0;
}

int runnerdManager::addNewSocket(const char *unex_socket)
{
    sockaddr_un settings;
    memset (&settings, 0, sizeof (settings));

    unlink (unex_socket);
    settings.sun_family = AF_LOCAL;
    strcpy (settings.sun_path, unex_socket);

    short new_socket = socket (AF_LOCAL, SOCK_STREAM, 0);

    if (bind (new_socket, (sockaddr*)&settings, sizeof (settings)) != 0)
    {
        printDebugOutput("addNewSocket:\tAF_LOCAL bind failed with %s\n", strerror (errno));
        return -1;
    }

    if (listen (new_socket, 10) != 0)
    {
        printDebugOutput("addNewSocket:\tSet AF_LOCAL to passive failed\n");
        return -2;
    }

    sockets.emplace_back((pollfd){new_socket, POLLIN, 0});
    return 0;
}
