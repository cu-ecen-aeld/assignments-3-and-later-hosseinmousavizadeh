//This the aesdsocket socket based prrogram
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <syslog.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sys/queue.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

#define FILE_PATH "/var/tmp/aesdsocketdata"
struct connectionData_s {
    int socketHandler;
    pthread_t threadId;
    bool finished;
    LIST_ENTRY(connectionData_s) entries;
};

typedef struct connectionData_s connectionData_t;

LIST_HEAD(connectionList_t, connectionData_s);

int signalReceived = 0;
int StreamSocket = -1;
pthread_mutex_t mutex;
int threadCount = 0;

void handle_sigint(int sig) 
{
    syslog(LOG_INFO, "Caught signal, exiting");
    int status = 0;
    signalReceived = 1;
    syslog(LOG_INFO, "Exiting aesdsocket");
    if (StreamSocket != -1) {
        shutdown(StreamSocket, SHUT_RDWR);
        if (close(StreamSocket) == -1) {
            syslog(LOG_ERR, "Error in closing the Stream socket.");
        }
        syslog(LOG_INFO, "Closed the streaming socket");
    }
}

void *timeStampThreadFunction(void *arg) 
{
    int status = 0;
    char buffer[64];
    int dataFile = -1;
    time_t nextTimeLog = 0;
    struct timespec ts = {0};

    if (!status) {
        dataFile = open(FILE_PATH, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (dataFile == -1) {
            syslog(LOG_ERR, "open failed");
            status = -9;
        } else {       
            memset(buffer, 0, sizeof(buffer));
        }
    }
    while ((!signalReceived) && (!status)) {
        time_t now = time(NULL);
        if (now >= nextTimeLog) {
            struct tm *tm_info = localtime(&now);
            strftime(buffer, sizeof(buffer), "timestamp:%a, %d %b %Y %H:%M:%S\n", tm_info);
            syslog(LOG_INFO, "Writing current time: %s", buffer);
            pthread_mutex_lock(&mutex);
            if (write(dataFile, buffer, strlen(buffer)) == -1) {
                syslog(LOG_ERR, "write timestamp failed");
                status = -10;
            }
            pthread_mutex_unlock(&mutex);
            nextTimeLog = now + 10;
        }
        ts.tv_nsec = 1000;
        nanosleep(&ts, NULL);
    }
    if (dataFile != -1) {
        close(dataFile);
        dataFile = -1;
    }
    return NULL;
}

void *connectionThreadFunction (void* arg) 
{
    int dataFile = -1;
    char *fileBuffer = NULL;
    int readFile = -1;
    ssize_t lengthRead = -1;
    int status = 0;
    char buffer[25000];

    if (arg == NULL) {
        syslog(LOG_ERR, "Invalid argument passed to thread function");
        status = -1;
    }
    connectionData_t* ConnectionData = (connectionData_t*)arg;
    if (!status) {
        dataFile = open(FILE_PATH, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (dataFile == -1) {
            syslog(LOG_ERR, "open failed");
            status = -9;
        } else {       
            memset(buffer, 0, sizeof(buffer));
        }
    }
    if (!status) {
        char end = 0;
        int bytesReceived = 0;
        char connection = 1;
        do {
            int bytesRead = recv(ConnectionData->socketHandler, buffer + bytesReceived, sizeof(buffer) - bytesReceived, 0);
            if (bytesRead == -1) {
                syslog(LOG_ERR, "recv failed");
                status = -9;
            }
            if (bytesRead == 0) {
                connection = 0; // Connection closed
            }
            bytesReceived += bytesRead;
            end = buffer[bytesReceived - 1];
        } while ((end != 0x0A) && (!status) && (connection) && (!signalReceived) );
    }
    if (!status) {
        pthread_mutex_lock(&mutex);
        if (write(dataFile, buffer, strlen(buffer)) == -1) {
            syslog(LOG_ERR, "write failed");
            status = -10;
        }
        pthread_mutex_unlock(&mutex);
    }
    if (!status) {
        readFile = open(FILE_PATH, O_RDONLY);
        if (readFile == -1) {
            syslog(LOG_ERR, "open failed");
            status = -11;
        } else {
            struct stat st;
            if (fstat(readFile, &st) == -1) {
                status = -12;
            }
            if (!status) {
                fileBuffer = malloc(st.st_size);
                if (fileBuffer == NULL) {
                    status = -13;
                }
            }
            if (!status) {
                lengthRead = read(readFile, fileBuffer, st.st_size);
                if (lengthRead == -1) {
                    status = -14;
                }
            }
        }
    }
    if (!status) {
        if (send(ConnectionData->socketHandler, fileBuffer, lengthRead, 0) == -1) {
            syslog(LOG_ERR, "send failed");
            status = -15;
        }
    }
    if(fileBuffer != NULL) {
        free(fileBuffer);
        fileBuffer = NULL;
    }
    if (readFile != -1) {
        close(readFile);
        readFile = -1;
    }
    if (dataFile != -1) {
        close(dataFile);
        dataFile = -1;
    }
    if (ConnectionData->socketHandler != -1) {
        close(ConnectionData->socketHandler);
        ConnectionData->socketHandler = -1;
    }
    ConnectionData->finished = true;
    if (status) {
        syslog(LOG_ERR, "Error occurred: %d", status);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int status = 0;
    int deamon = 0;
    struct addrinfo *serverinfo;
    struct addrinfo hints;
    struct sockaddr client;
    socklen_t clientSize = sizeof(client);
    int socketConnection = -1;
    pthread_t timeStampThread = -1;

    if(argc > 2) {
        syslog(LOG_ERR, "Invalid number of arguments");
        status = -1;
    }
    if (!status) {
        if (argc > 1 && ((!strcmp(argv[1], "-d")) || (!strcmp(argv[1], "--deamon")))) {
            deamon = 1;
        }
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handle_sigint;
        // Do NOT set SA_RESTART, so accept() is interrupted
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);
        openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "Starting aesdsocket");
        StreamSocket = socket(PF_INET, SOCK_STREAM, 0);
        if (StreamSocket == -1) {
            syslog(LOG_ERR, "failed to create socket");
            status = -2;
        }
    }
    if (!status) {
        if (deamon) {
            int pid = fork();
            if (pid < 0) {
                syslog(LOG_ERR, "fork failed");
                status = -6;
            } else if (pid > 0) {
                syslog(LOG_INFO, "Daemon process created with PID: %d", pid);
                status = 2;
            }
        }
    }
    if (!status) {
        syslog(LOG_INFO, "Socket created successfully.");
        int optval = 1;
        if(setsockopt(StreamSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) {
            syslog(LOG_ERR, "setsockopt SO_REUSEADDR failed");
            status = -3;
        }
    }
    if (!status) {
        memset(&hints, 0 , sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        status = getaddrinfo(NULL, "9000", &hints, &serverinfo);
        if (status != 0) {
            syslog(LOG_ERR, "getaddrinfo failed");
            status = -4;
        }
    }
    if (!status) {
        if (bind(StreamSocket, serverinfo->ai_addr, serverinfo->ai_addrlen) == -1) {
            syslog(LOG_ERR, "bind failed");
            status = -5;
        }
    }
    //Set up the timeStamp thread
    if (!status) {
        if (pthread_create(&timeStampThread, NULL, timeStampThreadFunction, NULL) != 0) {
            syslog(LOG_ERR, "pthread_create failed");
            status = -12;
        }
    }
    if (!status) {
        if (listen(StreamSocket, 1) == -1) {
            syslog(LOG_ERR, "listen failed");
            status = -8;
        } else {
            syslog(LOG_INFO, "Listening on port 9000");
        }
    }
    if (pthread_mutex_init(&mutex, NULL)) {
        syslog(LOG_ERR, "Mutex initialization failed");
        status = -9;
    }
    struct connectionList_t connectionList;
    LIST_INIT(&connectionList);
    while ((!signalReceived) && (!status)) {
        syslog(LOG_INFO, "Waiting for connection...");
        socketConnection = accept(StreamSocket, &client, &clientSize);
        if (socketConnection == -1) {
            syslog(LOG_WARNING, "accept failed");
            status = 1;
        } else {
            syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(((struct sockaddr_in *)&client)->sin_addr));
            connectionData_t *newConnection = malloc(sizeof(connectionData_t));
            if (newConnection == NULL) {
                syslog(LOG_ERR, "Memory allocation for new connection failed");
                status = -11;
            } else {
                newConnection->socketHandler = socketConnection;
                newConnection->finished = false;
                if (pthread_create(&newConnection->threadId, NULL, connectionThreadFunction, newConnection) != 0) {
                    syslog(LOG_ERR, "pthread_create failed");
                    free(newConnection);
                    status = -12;
                } else {
                    LIST_INSERT_HEAD(&connectionList, newConnection, entries);
                    threadCount++;
                }
            }
        }
        connectionData_t *connection = LIST_FIRST(&connectionList);
        while (connection != NULL) {
            connectionData_t* next = LIST_NEXT(connection, entries);
            if (connection->finished) {
                pthread_join(connection->threadId, NULL);
                threadCount--;
                LIST_REMOVE(connection, entries);
                free(connection);
                syslog(LOG_INFO, "Thread count after join: %d", threadCount);
            }
            connection = next;
        }
    }
    syslog(LOG_INFO, "Exiting aesdsocket, status: %d", status);
    if (timeStampThread != -1) {
        pthread_join(timeStampThread, NULL);
    }
    if (status != 2) {
        if (unlink(FILE_PATH) == -1) {
            syslog(LOG_ERR, "unlink unsuccessful");
            status = -16;
        }
    }
    if ((status < -1) || (status >= 0)) {
        closelog();
    }
    if ((status < -4) || (status >= 0 && status <= 1)) {
        freeaddrinfo(serverinfo);
    }
    if (status < 0) {
        return 1;
    } else {
        return 0;
    }
}
