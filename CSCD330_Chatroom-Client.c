#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_LENGTH    1000


int sockfd;
char sendline[BUFFER_LENGTH];
char recvline[BUFFER_LENGTH];

/*******************  Function prototypes begin here  ************************/

void *clientOperation_ReadThreadFunction(void *in_ptr);

void clientOperation_StripNewLine(char* str);

/*******************  Function prototypes end here  ************************/


int main() {

    struct sockaddr_in servaddr;
    pthread_t readThread;
    int connect_result;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &(servaddr.sin_addr));

    printf("Finding server...\n");
    connect_result = connect(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr));
    if (connect_result != 0) {
        printf("No server found, exiting...");
        return -1;
    }

    printf("Server found!\n");

    pthread_create(&readThread, NULL, clientOperation_ReadThreadFunction, NULL);


    while (1) {
        bzero(sendline, BUFFER_LENGTH);

        fgets(sendline, BUFFER_LENGTH, stdin);

        if (write(sockfd, sendline, strlen(sendline) + 1) == 0) {
            printf("Server gone missing, exiting...\n");
            break;
        }
    }

    close(sockfd);

    return 0;
}

void *clientOperation_ReadThreadFunction(void *in_ptr) {

    while (1) {
        bzero(recvline, BUFFER_LENGTH);

        if (read(sockfd, recvline, BUFFER_LENGTH) != 0) {
            printf("Other: %s", recvline);
        } else {
            break;
        }

    }

    return NULL;
}

void clientOperation_StripNewLine(char* str) {
    
}
