#include <iostream>

#include "runnerdManager.h"

int main(int argc, char *argv[])
{
    if (argc == 2 || argc > 3)
    {
        std::cerr << "Wrong arguments count\n";
        std::cerr << "Run #remote-runnerd [--timeout <value>]\n";
        return 0;
    }

    int cl_value = 5;

    if (argc == 3)
    {        
        if (strcmp (argv[1], "--timeout") != 0)
        {
            std::cerr << "Unknown argument: " << argv[1] << "\n";
            std::cerr << "Run #remote-runnerd [--timeout <value>]\n";
            return 0;
        }

        //not safe
        cl_value = atoi (argv[2]);
    }


    std::cerr << "I will be a daemon, muhaha\n";
    int res = daemon (0, 0);
    if (res != 0)
    {
        std::cerr << "Not enough mana!\n";
        exit (res);
    }

    runnerdManager manager;
    manager.setTimeout (cl_value);
    manager.addNewSocket(12345);
    manager.addNewSocket("/tmp/simple-telnetd");
    manager.startWaitClients();

    std::cerr << "cant run server!\n";
    return 0;
}
