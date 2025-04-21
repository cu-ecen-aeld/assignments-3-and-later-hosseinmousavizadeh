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

struct addrinfo *serverinfo;

void handle_sigint(int sig) {
    syslog(LOG_INFO, "Received SIGINT, exiting...");
    closelog();
    freeaddrinfo(serverinfo);
    exit(0);
}

int main()
{
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    signal(SIGQUIT, handle_sigint);
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting aesdsocket");
    int StreamSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (StreamSocket == -1){
        syslog(LOG_ERR, "failed to create socket");
        return -1;
    }
    syslog(LOG_INFO, "Socket created successfully.");
    
    //Binding to the Port.
    int status;
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
    char buffer[150000];
    memset(buffer, 0, sizeof(buffer));
    int bytesReceived = 0;
    while(1) {
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
        syslog(LOG_INFO, "Accepted connection from client: %s", inet_ntoa(((struct sockaddr_in *)&client)->sin_addr));
    
        memset(buffer, 0, sizeof(buffer));
        status = recv(dataSocket, buffer, sizeof(buffer), 0);
        if (status == -1) {
            syslog(LOG_ERR, "recv failed");
            close(dataSocket);
            freeaddrinfo(serverinfo);
            closelog();
            return -1;
        }

        syslog(LOG_INFO, "Received data: %s", buffer);
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
        ssize_t bytesRead = read(readFile, fileBuffer, st.st_size);
        if (bytesRead == -1) {
            free(fileBuffer);
            close(readFile);
            close(dataFile);
            close(dataSocket);
            syslog(LOG_ERR, "read failed");
            closelog();
            freeaddrinfo(serverinfo);
            return -1;
        }
        syslog(LOG_INFO, "Read %ld bytes from file: %s", bytesRead, fileBuffer);
        int status = send(dataSocket, fileBuffer, bytesRead, 0);
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
    
    syslog(LOG_INFO, "Data written to file: /var/tmp/aesdsocketdata");
    close(dataFile);
    close(dataSocket);
    syslog(LOG_INFO, "Connection closed");
}
    closelog();
    freeaddrinfo(serverinfo);
    return 0;
}