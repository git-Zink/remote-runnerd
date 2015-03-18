#include "runnerdWorker.h"

volatile bool runnerdWorker::need_reload_config;

void runnerdWorker::sigHandler(int signum)
{
    if (signum == SIGHUP)    
        need_reload_config = true;

    if (signum == SIGINT || signum == SIGSEGV)
        exit (signum);
}

int runnerdWorker::refreshCommands()
{    
    int fd = open (config_path.c_str(), O_RDONLY);
    if (fd == -1)
    {
        printDebugOutput ("refreshCommands:\tCant open file\n");
        return -1;
    }

    struct stat file_info;
    fstat (fd, &file_info);

    //read file here
    char command_source_file[file_info.st_size + 1];

    //read
    size_t tail = file_info.st_size;
    ssize_t real_read = 1;
    char *cur_data_pointer = &command_source_file[0];
    while (tail > 0)
    {        
        if ((real_read = read (fd, cur_data_pointer, tail)) == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                printDebugOutput ("refreshCommands:\tget some problems with read\n");
                close (fd);
                return -2;
            }
        }

        tail -= real_read;
        cur_data_pointer += real_read;
    }
    close (fd);

    //save to std::set allowed_commands
    pthread_mutex_lock (&mutex_commands_list); //lock

    allowed_commands.clear();
    int pos = 0;
    while (pos < file_info.st_size)
    {
        if (command_source_file[pos] != ' ' && command_source_file[pos] != '\n')
        {
            char *begining = &command_source_file[pos];

            while (pos < file_info.st_size && command_source_file[pos] != ' ' && command_source_file[pos] != '\n')
                ++pos;

            command_source_file[pos] = '\0';
            allowed_commands.emplace (begining);

            printDebugOutput("refreshCommands:\tadd commands <<%s>>\n", begining);
        }
        ++pos;
    }
    pthread_mutex_unlock (&mutex_commands_list); //lock

    return 0;
}

void *runnerdWorker::refreshBusyLoop(void *arg)
{
    runnerdWorker *source = static_cast <runnerdWorker*> (arg);
    if (source == NULL)
    {
        printDebugOutput("refreshBusyLoop:\tstatic cast failed, exiting...\n");
        exit (-1);
    }

    while (true)
    {
        if (source->need_reload_config)
        {
            int res = source->refreshCommands();
            need_reload_config = false;
            if (res == 0)
            {
                printDebugOutput("refreshBusyLoop:\trefresh allowed commands successful\n");
            }
            else
            {
                printDebugOutput("refreshBusyLoop:\trefresh allowed commands err #%d\n", res);
            }
        }
    }
}

int runnerdWorker::addClient(int clientfd)
{
    printDebugOutput("Add clients fd %d\n", clientfd);

    pthread_mutex_lock(&mutex_q_clients);   //lock
    q_clients.push (clientfd);
    pthread_mutex_unlock(&mutex_q_clients); //unlock

    if (!is_selecting)
    {
        printDebugOutput("run selectingClients thread\n");
        int res = pthread_create (&pth_select, 0, runnerdWorker::selectingClients, this);
        if (res != 0)
        {
            printDebugOutput("addClient:\tCant create pthread\n");
            return -1;
        }
        pthread_detach(pth_select);
        is_selecting = true;
    }

    return 0;
}

void *runnerdWorker::selectingClients(void *arg)
{
    runnerdWorker *source = static_cast <runnerdWorker*> (arg);
    if (source == NULL)
    {
        printDebugOutput("runnerdWorker:\tstatic cast failed\n");
        pthread_exit(NULL); //потенциальная ошибка, никак не сделать is_selecting = false;
    }

    std::vector <pollfd> fd_clients;

    while (source->is_selecting)
    {
        pthread_mutex_lock (&source->mutex_q_clients);
        while (source->q_clients.size() > 0)
        {
            fd_clients.emplace_back ( pollfd{source->q_clients.back(), POLLIN} );
            source->q_clients.pop();
        }
        pthread_mutex_unlock (&source->mutex_q_clients);

        int res = poll (&fd_clients[0], fd_clients.size(), poll_timeout);
        switch (res)
        {
        case -1:
            {
                if (errno == EINTR)
                    continue;
                printDebugOutput("selectingClients:\tOoops, poll err\n");
                exit (-1);
            }
            break;
        case 0:
            //just continue
            //printDebugOutput("selectingClients:\tNo Clients\n");
            break;
        default:
            {
                printDebugOutput("selectingClients:\tWe get something!\n");
                for (size_t i = 0; i < fd_clients.size(); ++i)
                {
                    printDebugOutput("selectingClients:\trevents = 0x%X, we wait for 0x%X\n", fd_clients[i].revents, POLLIN);

                    if (fd_clients[i].revents & (POLLERR | POLLNVAL | POLLHUP))
                    {                        
                        printDebugOutput("selectingClients:\tWe get poll revent 0x%X, client will be disconnected\n", fd_clients[i].revents);
                        close (fd_clients[i].fd);
                        fd_clients[i].fd = disconnected_fd;
                    }
                    else if (fd_clients[i].revents & POLLIN)
                    {                        
                        data_do_client * info = new data_do_client; //и кто это будет удалять?
                        info->fd = fd_clients[i].fd;
                        info->p_strcut_fd = &fd_clients[i];
                        fd_clients[i].fd = -1; //игнонируем клиента, пока обрабываем команду

                        pthread_mutex_lock (&source->mutex_commands_list); //lock
                        info->commands = source->allowed_commands; //потенциально долго
                        pthread_mutex_unlock (&source->mutex_commands_list); //unlock

                        info->timeout = source->timeout; //столько копировать, может правда проще fork() сделать?

                        pthread_t temp;
                        printDebugOutput("selectingClients:\tstart work with Client\n");
                        pthread_create (&temp, 0, dealWithClient, info);
                        pthread_detach (temp); //гуляй где хочешь, живи свободно
                    }                    
                }
            }
        };

        //очищаем отсоеденившихся
        fd_clients.erase(std::remove_if (fd_clients.begin(), fd_clients.end(), detectDisconnected), fd_clients.end());
    }

    pthread_exit(NULL);
}

void *runnerdWorker::dealWithClient(void *arg)
{
    data_do_client * info = static_cast <data_do_client*> (arg);
    if (info == NULL)
    {
        printDebugOutput("dealWithClient:\tcast failed\n");
        pthread_exit (NULL);
    }

    /* ============================== read from client ============================== */

    int rr;
    std::string recv = "";
    char tempo;

    do
    {
        rr = read (info->fd, &tempo, 1);
        if (tempo != '\n' && tempo != '\r')
            recv += tempo;
    }while (rr > 0 && tempo != '\n' && tempo != '\r');

    /* ============================== trash handle ==============================
     * telnet присылает в конце "\n\r", netcat только '\n'.
     */
    if (rr > 0 && recv.length() == 0)
    {
        //read trash from client
        printDebugOutput("dealWithClient:\tget some trash from client. Ignored.\n");

        info->p_strcut_fd->fd = info->fd; //poll in select-thread will listen it again
        delete info; //delete for select-thread;

        pthread_exit (NULL);
    }

    recv += '\0';
/*
    // ============================== for debug only with telnet problem ==============================
    for (int i = 0; recv[i] != 0; ++i)
        printDebugOutput("%d = %c (%d)\n", i, recv[i], recv[i]);
*/

    /* ============================== if client disconnected ============================== */
    if (rr == 0)
    {
        printDebugOutput("dealWithClient:\t client shutdown connection\n");
        close (info->fd);
        info->p_strcut_fd->fd = disconnected_fd; //mark fd for delete
        delete info; //delete for select-thread;

        pthread_exit (NULL);
    }

    printDebugOutput("dealWithClient:\tget from client - %s\n", recv.c_str());

    /* ============================== get command & args from client input string ============================== */
    std::vector<char*> args;
    size_t pos = 0;
    while (pos < recv.length())
    {
        if (recv[pos] != ' ')
        {
            args.push_back (&recv[pos]);

            while (pos < recv.length() && recv[pos] != ' ')
                ++pos;

            if (pos != recv.length())
            {
                recv[pos] = '\0';
            }
        }
        ++pos;
    }

    for (size_t i = 0; i < args.size(); ++i)
        printf ("args[%ld] = %s\n", i, args[i]);

    args.push_back(NULL);

    /* ============================== check allowed commands ============================== */
    if (info->commands.find(args[0]) == info->commands.end())
    {
        //access denied
        std::string message = args[0];
        message += ":\taccess denied\n";
        printDebugOutput("dealWithClient:\tthis command isnt allowed\n");

        write (info->fd, message.c_str(), message.length());

        info->p_strcut_fd->fd = info->fd; //poll in select-thread will listen it again
        delete info; //delete for select-thread;

        pthread_exit (NULL);
    }    

    printDebugOutput("dealWithClient:\tfork for run %s command\n", args[0]);

    /* ============================== make pipe for fork() ============================== */
    int pipefd [2];
    pipe (pipefd);

    /* ============================== fork() & exec() ============================== */
    int pid = fork();
    if (pid == 0)
    {

        //stdout, stderr -> pipefd[1]
        close (1);
        close (2);

        dup2 (pipefd[1], 1);
        dup2 (pipefd[1], 2);

        int res = execvp (args[0], &args[0]);

        if (res == -1 && errno == ENOENT)
        {
            //to stdout (-> pipedfd[1])
            printf ("%s:\tunknown command\n", args[0]);
        }
        else
        {
            perror (args[0]);
        }

        exit (0);
    }    

    /* ============================== wait for child proc; max timeout secs ============================== */
    int res_wait;
    int sleep_delta = 100 * 1000; //100 ms
    int sleep_total = info->timeout * 1000000;
    do
    {
        res_wait = waitpid (pid, 0, WNOHANG);
        if (res_wait == 0)
        {
            usleep (sleep_delta);
            sleep_total -= sleep_delta;
        }
    }while (res_wait == 0 && sleep_total > 0);

    /* ============================== if child proc not finished ============================== */
    if (res_wait == 0)
    {
        printDebugOutput ("dealWithClient:\tProc time is up. Killed\n");
        kill (pid, SIGKILL);
        int res = waitpid (pid, 0, 0);//а если он потомков наплодил? да позаботится init о всех нас.
        if (res != pid)
            printDebugOutput("dealWithClient:\tCant handle deadchild proc\n");
        else
            printDebugOutput("dealWithClient:\thandle deadchild proc\n");
    }


    /* ============================== check child's output ============================== */
    pollfd child;
    child.events = POLLIN;
    child.fd = pipefd[0];

    int ret = poll (&child, 1, 0);
    std::string message_to_client = "";

    switch (ret)
    {
    case -1: //err
        printDebugOutput("dealWithClient:\tCant get child stdout, poll err = %s\n", strerror (errno));
        message_to_client = "We cant get command output, sry";
        break;
    case 0: //time is up and no data
        printDebugOutput("dealWithClient:\tTime is up! And there isnt any data\n");
        message_to_client = "Command's time is up. We kill your process, very sry";
        break;
    default: //got something
        {
            int local_poll_ret;
            printDebugOutput("dealWithClient:\tget from child proc:\n");

            const int size = 100;
            char buf [size + 1];

            do
            {
                int rr = read (pipefd[0], buf, size);
                buf [rr] = '\0';

                printDebugOutput("%s", buf);

                message_to_client += buf;

                //читаем output полностью
                do
                {
                    local_poll_ret = poll (&child, 1, 0);
                }while (local_poll_ret == -1 && errno == EINTR);

            }while (local_poll_ret > 0); //-1 = err; 0 = times up = no data
        }
    };

    /* ============================== send answer to client ============================== */
    message_to_client += '\n';
    printDebugOutput("dealWithClient:\tsend to client:\t%s\n", message_to_client.c_str());

    char *write_pointer = &message_to_client[0];
    int want_write = message_to_client.length();
    int res;

    while (want_write != 0 && (res = write (info->fd, message_to_client.c_str(), message_to_client.length())) != 0)
    {
        if (res == -1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                printDebugOutput("dealWithClient:\tSerious write error\n");
                break;
            }
        }

        want_write -= res;
        write_pointer += res;
    }

    info->p_strcut_fd->fd = info->fd; //poll in select-thread will listen it again
    delete info; //delete for select-thread;

    pthread_exit (NULL);
}

bool runnerdWorker::detectDisconnected(const pollfd client)
{
    return (client.fd == disconnected_fd);
}

runnerdWorker::runnerdWorker()
{
    timeout = 5;
    config_path = "/etc/remote-runnerd.conf";

    mutex_q_clients = PTHREAD_MUTEX_INITIALIZER;
    mutex_commands_list = PTHREAD_MUTEX_INITIALIZER;

    is_selecting = false;
    need_reload_config = false;

    for (int i = 1; i < signals_count; ++i)
        signal (i, runnerdWorker::sigHandler);

    refreshCommands();
    pthread_create (&pth_refresh_loop, 0, runnerdWorker::refreshBusyLoop, this);
    pthread_detach (pth_refresh_loop);
}

runnerdWorker::~runnerdWorker()
{

}

void runnerdWorker::setTimeout(const int t)
{
    timeout = t;
}

void runnerdWorker::printDebugOutput(const char *message, ...)
{
#ifdef RUNNERDWORKER_DEBUG
    va_list args;
    va_start (args, message);
    vprintf (message, args);
    fflush(stdout);
    va_end (args);
#endif //RUNNERDWORKER_DEBUG
}
