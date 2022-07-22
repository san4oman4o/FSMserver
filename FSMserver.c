#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFSIZE 1024

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
    [DOWN_STATE] = { [UP_EVENT] = UpHandler, [RIGHT_EVENT] = RightHandler, [LEFT_EVENT] = LeftHandler },
    [UP_STATE] = { [DOWN_EVENT] = DownHandler },
    [RIGHT_STATE] = { [UP_EVENT] = UpHandler },
    [LEFT_STATE] = { [UP_EVENT] = UpHandler}
};

void Switch(eServerState state, char *buf)
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

void Chat(int socketfd, eServerState state)
{
    int connfd;
    int clientlen;
    char *hostaddrc;
    struct hostent *hostc;
    struct sockaddr_in clientaddr;
    char buf[BUFFSIZE];
    char input[BUFFSIZE];
    int val;
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
        //c_rfd = rfd;
        //rc = select(getdtablesize(), &c_rfd, NULL, NULL, (struct timeval *)NULL);
        connfd = accept(socketfd, (struct sockaddr *)&clientaddr,(unsigned int*) &clientlen);
        if (connfd == -1)
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

        while (1) 
        {
            memset(buf, 0, BUFFSIZE);
            val = read(connfd, buf, sizeof(buf));
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
            if (strncmp("CQ", input, 2) == 0)
            {
                strcpy(buf, "SR");
                Switch(state, buf);
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
                        }
                        break;
                    case 'D':
                        if (StateMachine[state][DOWN_EVENT] != NULL)
                        {
                            state = (*StateMachine[state][DOWN_EVENT]) ();
                        }
                        break;
                    case 'L':
                        if (StateMachine[state][LAST_EVENT] != NULL)
                        {
                            state = (*StateMachine[state][LEFT_EVENT]) ();
                        }
                        break;
                    case 'R':
                        if (StateMachine[state][RIGHT_EVENT] != NULL)
                        {
                            state = (*StateMachine[state][RIGHT_EVENT]) ();
                        }
                        break;
                    default:
                        break;
                }

                Switch(state, buf);
            }

            /*n = 0;
            printf("Enter a string: ");
            while ((buf[n++] = getchar()) != '\n')
                ;*/
            if (strlen(buf) == 3)
            {
                val = write(connfd, buf, sizeof(buf));
                if (val == -1)
                {
                    perror("Error on write");
                    exit(-1);
                }
            }
            else
            {
                strcpy(buf, "Error");
                val = write(connfd, buf, sizeof(buf));
                if (val == -1)
                {
                    perror("Error on write");
                    exit(-1);
                }
            }
        }

        close(connfd);
    }
}

int main(int argc, char **argv)
{
    int socketfd;
    int portno;
    struct sockaddr_in serveraddr;
    int optval;

    //FILE *logFile = fopen("server.log", "wx");

    eServerState defaultState = DOWN_STATE;

    if (argc != 2)
    {
        fprintf(stderr, "port not defined");
        exit(0);
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

    Chat(socketfd, defaultState);
    
    close(socketfd);
}