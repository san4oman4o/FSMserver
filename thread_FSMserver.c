#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFSIZE 1024

pthread_mutex_t lock;

typedef enum 
{
    UP_STATE,
    DOWN_STATE,
    RIGHT_STATE,
    LEFT_STATE,
    LAST_STATE
} eServerState;

typedef enum
{
    UP_EVENT,
    DOWN_EVENT,
    RIGHT_EVENT,
    LEFT_EVENT,
    LAST_EVENT
} eServerEvent;

eServerState state = DOWN_STATE;

typedef eServerState (*const afEventHandler[LAST_STATE][LAST_EVENT])(void);

//typedef eServerEvent (*pfEventHandler)(void);


eServerState UpHandler(void)
{
    return UP_STATE;
}

eServerState DownHandler(void)
{
    return DOWN_STATE;
}

eServerState LeftHandler(void)
{
    return LEFT_STATE;
}

eServerState RightHandler(void)
{
    return RIGHT_STATE;
}

static afEventHandler StateMachine = {
    [DOWN_STATE] = { [DOWN_EVENT] = NULL, [UP_EVENT] = UpHandler, [RIGHT_EVENT] = RightHandler, [LEFT_EVENT] = LeftHandler },
    [UP_STATE] = { [DOWN_EVENT] = DownHandler, [UP_EVENT] = NULL, [RIGHT_EVENT] = NULL, [LEFT_EVENT] = NULL },
    [RIGHT_STATE] = { [DOWN_EVENT] = NULL, [UP_EVENT] = UpHandler, [RIGHT_EVENT] = NULL, [LEFT_EVENT] = NULL },
    [LEFT_STATE] = { [DOWN_EVENT] = NULL, [UP_EVENT] = UpHandler, [RIGHT_EVENT] = NULL, [LEFT_EVENT] = NULL },
};

void Switch(char *buf)
{
    switch(state)
    {
        case DOWN_STATE:
            buf[2] = 'D';
            break;
        case UP_STATE:
            buf[2] = 'U';
            break;
        case LEFT_STATE:
            buf[2] = 'L';
            break;
        case RIGHT_STATE:
            buf[2] = 'R';
            break;
        default:
            break;
    }
}

void *Output(void *connfd)
{
    int val = 0;
    char buf[BUFFSIZE];
    char input[BUFFSIZE];
    int fd = *((int *) connfd);

    pthread_detach(pthread_self());
    free(connfd);
    while (1) 
    {
        memset(buf, 0, BUFFSIZE);
        val = read(fd, buf, sizeof(buf));
        if (val == -1)
        {
            perror("Error on read");
            exit(-1);
        }
        
        printf("Server received %d bytes: %s", val, buf);
        if (strncmp("exit", buf, 4) == 0)
        {
            printf("Exit connection\n");
            break;
        }

        if (val == 0)
        {
            break;
        }

        memset(input, 0, BUFFSIZE);
        strcpy(input, buf);
        memset(buf, 0, BUFFSIZE);
        pthread_mutex_lock(&lock);
        if (strncmp("CQ", input, 2) == 0)
        {
            strcpy(buf, "SR");
            Switch(buf);
        }

        if (strncmp("CS", input, 2) == 0)
        {
            strcpy(buf, "CR");

            printf("c: %s\n", buf);
            switch(input[2])
            {
                case 'U':
                    if (StateMachine[state][UP_EVENT] != NULL)
                    {
                        state = (*StateMachine[state][UP_EVENT]) ();
                        Switch(buf);
                    }
                    break;
                case 'D':
                    if (StateMachine[state][DOWN_EVENT] != NULL)
                    {
                        state = (*StateMachine[state][DOWN_EVENT]) ();
                        Switch(buf);
                    }
                    break;
                case 'L':
                    if (StateMachine[state][LEFT_EVENT] != NULL)
                    {
                        state = (*StateMachine[state][LEFT_EVENT]) ();
                        Switch(buf);
                    }
                    break;
                case 'R':
                    if (StateMachine[state][RIGHT_EVENT] != NULL)
                    {
                        state = (*StateMachine[state][RIGHT_EVENT]) ();
                        Switch(buf);
                    }
                    break;
                default:
                    break;
            }
        }
        pthread_mutex_unlock(&lock);

        /*n = 0;
        printf("Enter a string: ");
        while ((buf[n++] = getchar()) != '\n')
            ;*/
        if (strlen(buf) == 3)
        {
            val = write(fd, buf, sizeof(buf));
            if (val == -1)
            {
                perror("Error on write");
                exit(-1);
            }
        }
        else
        {
            strcpy(buf, "Error");
            val = write(fd, buf, sizeof(buf));
            if (val == -1)
            {
                perror("Error on write");
                exit(-1);
            }
        }
    }

    close(fd);

    return NULL;
}

void Chat(int socketfd)
{
    int clientlen;
    char *hostaddrc;
    struct hostent *hostc;
    struct sockaddr_in clientaddr;
    pthread_t tid;
    //int n;
    //eServerState newState;
    //fd_set rfd;
    //fd_set c_rfd;
    //int rc;

    //FD_ZERO(&rfd);
    //FD_SET(socketfd, &rfd);

    clientlen = sizeof(clientaddr);
    while (1)
    {
        int *connfd = (int *) malloc(sizeof(int));
        //c_rfd = rfd;
        //rc = select(getdtablesize(), &c_rfd, NULL, NULL, (struct timeval *)NULL);
        *connfd = accept(socketfd, (struct sockaddr *)&clientaddr,(unsigned int*) &clientlen);
        if (*connfd == -1)
        {
            perror("Error on accept");
            exit(-1);
        }

        hostc = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
        sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hostc == NULL)
        {
            perror("Error on gethostbyaddr");
            exit(-1);
        }

        hostaddrc = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrc == NULL)
        {
            perror("Error on inet_ntoa");
            exit(-1);
        }

        printf("Connection established with %s(%s)\n", hostc->h_name, hostaddrc);

        pthread_create(&tid, NULL, &Output, connfd);
    }
}

int main(int argc, char **argv)
{
    int socketfd;
    int portno;
    struct sockaddr_in serveraddr;
    int optval;

    //FILE *logFile = fopen("server.log", "wx");

    //eServerState defaultState = DOWN_STATE;

    if (argc != 2)
    {
        fprintf(stderr, "port not defined");
        exit(0);
    }

    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        fprintf(stderr, "Mutex init failed\n");
        exit(-1);
    }
    
    portno = atoi(argv[1]);
    socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketfd == -1)
    {
        perror("Couldn't open socket");
        exit(-1);
    }

    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    memset((char *)&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short) portno);

    if (bind(socketfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
    {
        perror("Error on binding");
        exit(-1);
    }

    if (listen(socketfd, 5) == -1)
    {
        perror("Error on listen");
        exit(-1);
    }

    Chat(socketfd);

    pthread_mutex_destroy(&lock);
    
    close(socketfd);
}