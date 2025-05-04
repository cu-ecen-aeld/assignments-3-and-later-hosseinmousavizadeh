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

int signalReceived = 0;
int inProgress = 0;
int StreamSocket = -1;
void handle_sigint(int sig) {
    syslog(LOG_INFO, "Caught signal, exiting %d", inProgress);
    signalReceived = 1;
    if (inProgress == 0) {
        syslog(LOG_INFO, "Exiting aesdsocket");
        if(StreamSocket != -1) {
            close(StreamSocket);
        }
        closelog();
        if (unlink("/var/tmp/aesdsocketdata") == -1) {
            syslog(LOG_ERR, "unlink unsuccessful");
            close(StreamSocket);
            closelog();
            exit(1);
        }
        syslog(LOG_INFO, "Unlink successful");
        close(StreamSocket);
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    int deamon = 0;
    if(argc > 2) {
        syslog(LOG_ERR, "Invalid number of arguments");
        return -1;
    }
    if (argc > 1 && ((!strcmp(argv[1], "-d")) || (!strcmp(argv[1], "--deamon")))) {
        deamon = 1;
    }
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    signal(SIGQUIT, handle_sigint);
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting aesdsocket");
    StreamSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (StreamSocket == -1){
        syslog(LOG_ERR, "failed to create socket");
        return -1;
    }
    syslog(LOG_INFO, "Socket created successfully.");
    int optval = 1;
    if (setsockopt(StreamSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        syslog(LOG_ERR, "setsockopt SO_REUSEADDR failed");
        close(StreamSocket);
        return -1;
    }
    //Binding to the Port.
    int status;
    struct addrinfo *serverinfo;
    struct addrinfo hints;

    memset(&hints, 0 , sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, "9000", &hints, &serverinfo);
    if (status != 0){
        syslog(LOG_ERR, "getaddrinfo failed");
        closelog();
        return -1;
    }

    status = bind(StreamSocket, serverinfo->ai_addr, serverinfo->ai_addrlen);
    if (status == -1) {
        syslog(LOG_ERR, "bind failed");
        freeaddrinfo(serverinfo);
        closelog();
        return -1;
    }
    if(deamon) {
        int pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "fork failed");
            freeaddrinfo(serverinfo);
            closelog();
            return -1;
        }
        if(pid > 0) {
            syslog(LOG_INFO, "Daemon process created with PID: %d", pid);
            freeaddrinfo(serverinfo);
            closelog();
            return 0;
        }
    }
    status = listen(StreamSocket, 1);
    if (status == -1) {
        syslog(LOG_ERR, "listen failed");
        freeaddrinfo(serverinfo);
        closelog();
        return -1;
    }
    syslog(LOG_INFO, "Listening on port 9000");
    
    struct sockaddr client;
    socklen_t clientSize = sizeof(client);
    char buffer[25000];
    memset(buffer, 0, sizeof(buffer));
    
    do {
        int dataFile = open("/var/tmp/aesdsocketdata", O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (dataFile == -1) {
            syslog(LOG_ERR, "open failed");
            freeaddrinfo(serverinfo);
            closelog();
            return -1;
        }
        int dataSocket = accept(StreamSocket, &client, &clientSize);
        if (dataSocket == -1) {
            syslog(LOG_ERR, "accept failed");
            freeaddrinfo(serverinfo);
            closelog();
            return -1;
        }
        inProgress = 1;
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(((struct sockaddr_in *)&client)->sin_addr));
    
        memset(buffer, 0, sizeof(buffer));
        syslog(LOG_ERR, "start");
        char end = 0;
        int bytesReceived = 0;
        do {
            int bytesRead = recv(dataSocket, buffer + bytesReceived, sizeof(buffer) - bytesReceived, 0);

            if (bytesRead == -1) {
                syslog(LOG_ERR, "recv failed");
                close(dataSocket);
                freeaddrinfo(serverinfo);
                closelog();
                return -1;
            }
            if (bytesRead == 0) {
                break; // Connection closed
            }
            bytesReceived += bytesRead;
            end = buffer[bytesReceived - 1];
            
        } while (end != 0x0A);
        status = write(dataFile, buffer, strlen(buffer));
        if (status == -1) {
            syslog(LOG_ERR, "write failed");
            close(dataSocket);
            freeaddrinfo(serverinfo);
            closelog();
            return -1;
        }
        int readFile = open("/var/tmp/aesdsocketdata", O_RDONLY);
        if (readFile == -1) {
            syslog(LOG_ERR, "open failed");
            close(dataSocket);
            freeaddrinfo(serverinfo);
            closelog();
            return -1;
        }
        struct stat st;
        if (fstat(readFile, &st) == -1) {
            close(readFile);
            close(dataFile);
            close(dataSocket);
            syslog(LOG_ERR, "fstat failed");
            closelog();
            freeaddrinfo(serverinfo);
            return 1;
        }
        char *fileBuffer = malloc(st.st_size);
        if (fileBuffer == NULL) {
            close(readFile);
            close(dataFile);
            close(dataSocket);
            syslog(LOG_ERR, "malloc failed");
            closelog();
            freeaddrinfo(serverinfo);
            return -1;
        }
        ssize_t lengthRead = read(readFile, fileBuffer, st.st_size);
        if (lengthRead == -1) {
            free(fileBuffer);
            close(readFile);
            close(dataFile);
            close(dataSocket);
            syslog(LOG_ERR, "read failed");
            closelog();
            freeaddrinfo(serverinfo);
            return -1;
        }
        int status = send(dataSocket, fileBuffer, lengthRead, 0);
        if (status == -1) {
            free(fileBuffer);
            close(readFile);
            close(dataFile);
            close(dataSocket);
            syslog(LOG_ERR, "send failed");
            closelog();
            freeaddrinfo(serverinfo);
            return -1;
        }
        free(fileBuffer);
        close(readFile);
        
        close(dataFile);
        close(dataSocket);
        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(((struct sockaddr_in *)&client)->sin_addr));
        inProgress = 0;
    }while(!signalReceived);
    
    if (status == -1) {
        syslog(LOG_ERR, "close failed");
        freeaddrinfo(serverinfo);
        closelog();
        return -1;
    }
    
    if (unlink("/var/tmp/aesdsocketdata") == -1) {
        syslog(LOG_ERR, "unlink unsuccessful");
        closelog();
        freeaddrinfo(serverinfo);
        return 1;
    }
    closelog();
    freeaddrinfo(serverinfo);
    return 0;
}