#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_LENGTH       1000
#define CONNECT_PORT        8080

int sockfd;
char sendline[BUFFER_LENGTH];
char recvline[BUFFER_LENGTH];

/*******************  Function prototypes begin here  ************************/

void *clientOperation_ReadThreadFunction(void *in_ptr);

void clientOperation_StripNewLine(char* str);

char *parseFileName(char *input);

int getFileSize(char *fileName);

char *getFileContents(char *fileName);

char *receiverParseFilePacket(char *filePacket);

/*******************  Function prototypes end here  ************************/


int main() {

    struct sockaddr_in servaddr;
    pthread_t readThread;
    int connect_result;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(CONNECT_PORT);
    inet_pton(AF_INET, "127.0.0.1", &(servaddr.sin_addr));

    printf("Finding server...\n");
    connect_result = connect(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr));
    if (connect_result != 0) {
        printf("No server found, exiting...\n");
        return -1;
    }

    printf("Server found!\n");

    pthread_create(&readThread, NULL, &clientOperation_ReadThreadFunction, NULL);


    while (1) {
        bzero(sendline, BUFFER_LENGTH);

        fgets(sendline, BUFFER_LENGTH, stdin);
        clientOperation_StripNewLine(sendline);

        if (sendline[0] == '/' && sendline[1] == 'f' && strlen(sendline) > 3) {
            char fileName[30];
            bzero(fileName, 30);
            strncpy(fileName, sendline + 3, 30);
            // read file size
            int fileSize = getFileSize(fileName);
            // read file contents
            if (fileSize > 0) {
                char *contents = getFileContents(fileName);
                // append these in proper format to sendline
                if(strlen(contents)) {
                    clientOperation_StripNewLine(contents);
                    char fileContents[BUFFER_LENGTH];
                    strncpy(fileContents, contents, BUFFER_LENGTH);
                    bzero(sendline, BUFFER_LENGTH);
                    sprintf(sendline, "/f %d|%s|%s", fileSize, fileName, fileContents);
                }
            }
        }

        if (write(sockfd, sendline, strlen(sendline) + 1) == 0) {
            printf("Server gone missing, exiting...\n");
            break;
        }

        if (sendline[0] == '/' && sendline[1] == 'f') {
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
            if (recvline[0] == '/' && recvline[1] == 'f') {
                // write file
                receiverParseFilePacket(recvline);
                printf("file transferred\n");
            }
            else {
                printf("%s", recvline);
                fflush(stdout);
            }
        } else {
            break;
        }

    }

    return NULL;
}

void clientOperation_StripNewLine(char* str) {
	int length = strlen(str);

    if (length > 0) {
        if (str[length - 1] == '\r' || str[length - 1] == '\n') {
            str[length - 1] = '\0';
        }
        if (str[length - 2] == '\r' || str[length - 2] == '\n') {
            str[length - 2] = '\0';
        }
    }
}

int getFileSize(char *fileName) {
    int fileSize;
    int fd;
    if((fd = open(fileName, O_RDONLY)) < 0) {
        printf("Could not read '%s' size.\n", fileName);
        return -1;
    }
    else {
        lseek(fd, 0, SEEK_END);
        fileSize = lseek(fd, 0, SEEK_CUR);
        close(fd);
        return fileSize;
    }
}

char *getFileContents(char *fileName) {
    int fd = open(fileName, O_RDONLY);
    if (fd == -1) {
        printf("Could not open '%s' contents.\n", fileName);
    }
    else {
        char fileContents[BUFFER_LENGTH];
        read(fd, &fileContents, BUFFER_LENGTH);
        close(fd);
        char *contents = fileContents + 0;
        return contents;
    }
}

char *receiverParseFilePacket(char *filePacket) {
    filePacket += 3;
    char *byteString = strtok_r(filePacket, "|", &filePacket);
    int bytes = atoi(byteString);
    char *fileName = strtok_r(filePacket, "|", &filePacket);
    char *fileContents = strtok_r(filePacket, "", &filePacket);
    FILE *file = fopen(fileName, "w");
    if (!file) {
        printf("Error creating file %s\n", fileName);
    }
    fprintf(file, fileContents, 1);
    fclose(file);
}
