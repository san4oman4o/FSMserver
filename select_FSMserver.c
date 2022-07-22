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
    [DOWN_STATE] = { [DOWN_EVENT] = NULL, [UP_EVENT] = UpHandler, [RIGHT_EVENT] = RightHandler, [LEFT_EVENT] = LeftHandler },
    [UP_STATE] = { [DOWN_EVENT] = DownHandler, [UP_EVENT] = NULL, [RIGHT_EVENT] = NULL, [LEFT_EVENT] = NULL },
    [RIGHT_STATE] = { [DOWN_EVENT] = NULL, [UP_EVENT] = UpHandler, [RIGHT_EVENT] = NULL, [LEFT_EVENT] = NULL },
    [LEFT_STATE] = { [DOWN_EVENT] = NULL, [UP_EVENT] = UpHandler, [RIGHT_EVENT] = NULL, [LEFT_EVENT] = NULL },
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

void Output(char *input, char *buf, eServerState *state)
{
    memset(input, 0, BUFFSIZE);
    strcpy(input, buf);
    memset(buf, 0, BUFFSIZE);
    if (strncmp("CQ", input, 2) == 0)
    {
        strcpy(buf, "SR");
        Switch((eServerState) *state, buf);
    }

    if (strncmp("CS", input, 2) == 0)
    {
        strcpy(buf, "CR");

        //printf("c: %s\n", buf);
        switch(input[2])
        {
            case 'U':
                if (StateMachine[*state][UP_EVENT] != NULL)
                {
                    *state = (*StateMachine[*state][UP_EVENT]) ();
                    Switch((eServerState) *state, buf);
                }
                break;
            case 'D':
                if (StateMachine[*state][DOWN_EVENT] != NULL)
                {
                    *state = (*StateMachine[*state][DOWN_EVENT]) ();
                    Switch((eServerState) *state, buf);
                }
                break;
            case 'L':
                if (StateMachine[*state][LEFT_EVENT] != NULL)
                {
                    *state = (*StateMachine[*state][LEFT_EVENT]) ();
                    Switch((eServerState) *state, buf);
                }
                break;
            case 'R':
                if (StateMachine[*state][RIGHT_EVENT] != NULL)
                {
                    *state = (*StateMachine[*state][RIGHT_EVENT]) ();
                    Switch((eServerState) *state, buf);
                }
                break;
            default:
                break;
        }
    }
}



void Chat(int socketfd, eServerState state)
{
    int connfd;
    int clientlen;
    char *hostaddrc;
    struct hostent *hostc;
    struct sockaddr_in clientaddr;
    struct timeval timeout;
    char buf[BUFFSIZE];
    char input[BUFFSIZE];
    int val;
    //int n;
    //eServerState newState;
    fd_set rfd;
    fd_set c_rfd;
    int rc;
    int dsize;
    int descready;
    int closeconn;

    FD_ZERO(&rfd);
    FD_SET(socketfd, &rfd);
    FD_SET(STDIN_FILENO, &rfd);
    FD_SET(STDOUT_FILENO, &rfd);
    FD_SET(STDERR_FILENO, &rfd);

    timeout.tv_sec = 3 * 60;
    timeout.tv_usec = 0;

    dsize = socketfd;

    /*for (int i = 0; i < dsize; i++)
    {
        if (i != socketfd)
        {
            close(i);
        }
    }*/

    clientlen = sizeof(clientaddr);
    while (1)
    {
        memcpy(&c_rfd, &rfd, sizeof(rfd));
        //printf("Select. \n");
        rc = select(dsize + 1, &c_rfd, NULL, NULL, NULL);
        if (rc == -1)
        {
            perror("Error on select");
            exit(-1);
        }

        if (rc == 0)
        {
            break;
        }

        if (FD_ISSET(socketfd, &c_rfd))
        {
            connfd = accept(socketfd, (struct sockaddr *) &clientaddr, (unsigned int *) &clientlen);
            if (connfd == -1)
            {
                perror("Error on accept");
                continue;
            }

            FD_SET(connfd, &rfd);

            if (connfd > dsize)
            {
                dsize = connfd;
            }
        }

        descready = rc;
        for (int i = socketfd + 1; i <= dsize && descready > 0; i++)
        {
            if (FD_ISSET(i, &c_rfd))
            {
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
                //fflush(stdout);
                descready--; 

                /*if (i == socketfd)
                { 
                    do
                    {
                        connfd = accept(socketfd, (struct sockaddr *)&clientaddr,(unsigned int *) &clientlen);
                        if (connfd == -1)
                        {
                            perror("Error on accept");
                            break;
                        }

                        FD_SET(connfd, &rfd);

                        if (connfd > dsize)
                        {
                            dsize = connfd;
                        }

                    } while (connfd != -1 && connfd < 1);
                }*/
                closeconn = 0;

                while(1)
                {
                    //memset(buf, 0, BUFFSIZE);
                    val = read(i, buf, sizeof(buf));
                    if (val == -1)
                    {
                        perror("Error on read");
                        closeconn = 1;
                        break;
                        //exit(-1);
                    }
                    
                    printf("Server received %d bytes: %s\n", val, buf);
                    //fflush(stdout);
                    if (strncmp("exit", buf, 4) == 0)
                    {
                        //printf("Exit connection\n");
                        /*close(i);
                        FD_CLR(i, &rfd);*/
                        closeconn = 1;
                        break;
                    }

                    if (val == 0)
                    {
                        /*close(i);
                        FD_CLR(i, &rfd);*/
                        closeconn = 1;
                        break;
                    }

                    Output(input, buf, &state);

                    /*n = 0;
                    printf("Enter a string: ");
                    while ((buf[n++] = getchar()) != '\n')
                        ;*/
                    if (strlen(buf) == 3)
                    {
                        val = write(i, buf, sizeof(buf));
                        if (val == -1)
                        {
                            perror("Error on write");
                            exit(-1);
                        }

                        break;
                    }
                    else
                    {
                        strcpy(buf, "Error");
                        val = write(i, buf, sizeof(buf));
                        if (val == -1)
                        {
                            perror("Error on write");
                            exit(-1);
                        }

                        break;
                    }
                }

                if (closeconn)
                {
                    close(i);
                    FD_CLR(i, &rfd);
                    if (i == dsize)
                    {
                        while (FD_ISSET(dsize, &rfd) == 0)
                        {
                            dsize--;
                        }
                    }

                    memcpy(&c_rfd, &rfd, sizeof(rfd));
                }
            }
        }
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