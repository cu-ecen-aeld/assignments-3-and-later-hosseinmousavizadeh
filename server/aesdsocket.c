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

#define MAX_SIMOULTANEOUS_CONNECTIONS 10

int signalReceived = 0;
int inProgress = 0; //TODO: change the structure of this flag
int StreamSocket = -1;
pthread_mutex_t mutex;
pthread_t connectionThread[MAX_SIMOULTANEOUS_CONNECTIONS];
int threadCount = 0;

void handle_sigint(int sig) 
{
    syslog(LOG_INFO, "Caught signal, exiting %d", inProgress);
    int status = 0;
    signalReceived = 1;
    if (threadCount == 0) {
        syslog(LOG_INFO, "Exiting aesdsocket");
        if(StreamSocket != -1) {
            close(StreamSocket);
        }
        if (unlink("/var/tmp/aesdsocketdata") == -1) {
            syslog(LOG_ERR, "unlink unsuccessful");
            status = 1;
        } else {
            syslog(LOG_INFO, "Unlink successful");
        }
        closelog();
        exit(status);
    }
}

void *connectionThreadFunction (void* arg) 
{
    int dataFile = -1;
    char *fileBuffer = NULL;
    int readFile = -1;
    ssize_t lengthRead = -1;
    int status = 0;
    char buffer[25000];
    int* connectionSocket = NULL;

    pthread_mutex_lock(&mutex);
    if (arg == NULL) {
        syslog(LOG_ERR, "Invalid argument passed to thread function");
        status = -1;
    }
    connectionSocket = (int*)arg;

    if (!status) {
        dataFile = open("/var/tmp/aesdsocketdata", O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (dataFile == -1) {
            syslog(LOG_ERR, "open failed");
            status = -9;
        } else {       
            memset(buffer, 0, sizeof(buffer));
            syslog(LOG_ERR, "start");
        }
    }
    if (!status) {
        char end = 0;
        int bytesReceived = 0;
        char connection = 1;
        do {
            syslog(LOG_WARNING, "In thread: Thread count: %d, socket handler: %d", threadCount, *connectionSocket);
            int bytesRead = recv(*connectionSocket, buffer + bytesReceived, sizeof(buffer) - bytesReceived, 0);

            if (bytesRead == -1) {
                syslog(LOG_ERR, "recv failed");
                status = -9;
            }
            if (bytesRead == 0) {
                connection = 0; // Connection closed
            }
            bytesReceived += bytesRead;
            end = buffer[bytesReceived - 1];
        } while ((end != 0x0A) && (!status) && (connection));
    }
    if (!status) {
        if (write(dataFile, buffer, strlen(buffer)) == -1) {
            syslog(LOG_ERR, "write failed");
            status = -10;
        }
    }
    if (!status) {
        readFile = open("/var/tmp/aesdsocketdata", O_RDONLY);
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
        if (send(*connectionSocket, fileBuffer, lengthRead, 0) == -1) {
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
    threadCount--;
    if (*connectionSocket != -1) {
        close(*connectionSocket);
        *connectionSocket = -1;
    }
    pthread_mutex_unlock(&mutex);
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

    if(argc > 2) {
        syslog(LOG_ERR, "Invalid number of arguments");
        status = -1;
    }
    if (!status) {
        if (argc > 1 && ((!strcmp(argv[1], "-d")) || (!strcmp(argv[1], "--deamon")))) {
            deamon = 1;
        }
        signal(SIGINT, handle_sigint);
        signal(SIGTERM, handle_sigint);
        signal(SIGQUIT, handle_sigint);
        openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "Starting aesdsocket");
        StreamSocket = socket(PF_INET, SOCK_STREAM, 0);
        if (StreamSocket == -1) {
            syslog(LOG_ERR, "failed to create socket");
            status = -2;
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
        if (status != 0){
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
    if (!status) {
        if (deamon) {
            int pid = fork();
            if (pid < 0) {
                syslog(LOG_ERR, "fork failed");
                status = -6;
            } else if (pid > 0) {
                syslog(LOG_INFO, "Daemon process created with PID: %d", pid);
                status = 1;
            }
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
    while ((!signalReceived) && (!status)) {
        socketConnection = accept(StreamSocket, &client, &clientSize);
        if (socketConnection == -1) {
            syslog(LOG_ERR, "accept failed");
            status = -10;
        } else {
            syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(((struct sockaddr_in *)&client)->sin_addr));
            syslog(LOG_WARNING, "Thread count: %d, socket handler: %d", threadCount, socketConnection);
            if (pthread_create(&connectionThread[threadCount], NULL, connectionThreadFunction, &socketConnection) != 0) {
                syslog(LOG_ERR, "pthread_create failed");
                status = -10;
            } else {
                syslog(LOG_INFO, "Connection thread %d created", threadCount);
                threadCount++;
                if (threadCount >= MAX_SIMOULTANEOUS_CONNECTIONS) {
                    syslog(LOG_ERR, "Maximum simultaneous connections reached");
                    status = -11;
                }
            }
            syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(((struct sockaddr_in *)&client)->sin_addr));
        }
    }
    if (unlink("/var/tmp/aesdsocketdata") == -1) {
        syslog(LOG_ERR, "unlink unsuccessful");
        status = -16;
    } 
    if ((status < -1) || (status == 0)) {
        closelog();
    }
    if ((status < -4) || (status == 0)) {
        freeaddrinfo(serverinfo);
    }
    if (status) {
        return 1;
    } else {
        return 0;
    }
}
