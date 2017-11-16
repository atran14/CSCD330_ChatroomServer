#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <stdlib.h>
#include <unistd.h>

#define STANDARD_NAME_LENGTH 8
#define PORT 8080
#define BUFFER_LENGTH 1000
#define ROOM_COUNT 2
#define USERS_CAP_PER_ROOM 3

#define LIMBO "LIMBO"
#define ROOM_1_ID "Hobby"
#define ROOM_2_ID "330"

#define LIST_ROOMS "/r"
#define JOIN_ROOM "/j"
#define LIST_PEOPLE "l"
#define LOG_OFF "/x"
#define PRIVATE_CHAT "/p"
#define END_PRIVATE_CHAT "/q"
#define SEND_FILE "/f"
#define HELP "/h"

typedef struct {
    int clisd;
    char name[STANDARD_NAME_LENGTH + 1];
    char chatRoomId[STANDARD_NAME_LENGTH];
} Client;

/*******************  Function prototypes begin here  ************************/

void preServerStart_InitializeClients(Client *, int);

void preServerStart_InitializeNoWaitInterval(struct timeval *interval);

void serverOperation_WipeClientRecord(Client *clientArray, int client_sockDescriptor);

void interpretCommand(char * input, Client * clients, int curClient);

char * parseCommand(char * input, char ** command);

void listRoomsCommand();

void joinRoomCommand(char * roomName, Client client);

void listPeopleCommand(Client * clients, int curClient);

void logOffCommand(Client  * clients, int curClient);

void privateChatCommand(char * personName);

void endPrivateChatCommand();

void sendFileCommand(char * fileName);

void helpCommand();

void broadcastMessage(Client * clients, int curClient, char * message);

/*******************  Function prototypes end here  ************************/

int main() {

    Client clients[USERS_CAP_PER_ROOM * ROOM_COUNT];
    char masterBuffer[BUFFER_LENGTH];
    fd_set current_fd_list, original_fd_list;
    struct timeval noWait_Interval;
    int io_ready_count;
    int server_sd, max_fd;
    struct sockaddr_in servaddr;

    preServerStart_InitializeNoWaitInterval(&noWait_Interval);
    preServerStart_InitializeClients(clients, USERS_CAP_PER_ROOM * ROOM_COUNT);

    server_sd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);

    bind(server_sd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    listen(server_sd, 10);

    printf("[INFO] Online at port %d\n", PORT);

    FD_ZERO(&original_fd_list);
    FD_SET(server_sd, &original_fd_list);
    max_fd = server_sd;

    bzero(&masterBuffer, BUFFER_LENGTH);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

    printf("[INFO] Server loop begins\n");
    while (1) {
        
        //<editor-fold desc="STAGE 1 - Server detecting incoming connection">
        current_fd_list = original_fd_list;
        io_ready_count = select(max_fd + 1, &current_fd_list, NULL, NULL, &noWait_Interval);

        if (FD_ISSET(server_sd, &current_fd_list)) {
            printf("[INFO] Non-block select() returns %d\n", io_ready_count);
            int newC_sd = accept(server_sd, NULL, NULL);
            printf("[INFO] Accept client's connection w/ sock_desc of %d\n", newC_sd);

            int i;
            for (i = 0; i < USERS_CAP_PER_ROOM * ROOM_COUNT; i++) {
                if (clients[i].clisd < 0) {
                    clients[i].clisd = newC_sd;
                    printf("[INFO] Client %d is initialized with sock_def of %d w/ default name \"LIMBO\"\n", i, newC_sd);
                    break;
                }
            }

            //todo [1] Send client "ok" signal for it to send name & and another "ok" signal for it to join the server
            //todo [1.1] Figure out where should this be handled
            FD_SET(newC_sd, &original_fd_list);
            if (newC_sd > max_fd) max_fd = newC_sd;
        }
        //</editor-fold>

        //<editor-fold desc="STAGE 2 - Server going through the fd_set list and acting accordingly (interpret/broadcast/disconnect)">
        if (io_ready_count > 0) {
            int i;
            for (i = 0; i < USERS_CAP_PER_ROOM * ROOM_COUNT; i++) {
                if (clients[i].clisd > 0 && FD_ISSET(clients[i].clisd, &current_fd_list)) {
                    ssize_t read_res = read(clients[i].clisd, masterBuffer, BUFFER_LENGTH - 1); //space a char for the \0

                    if (read_res <= 0) {
                        int disc_sd = clients[i].clisd;
                        printf("[INFO] Client %d (%s@%d) disconnected\n", i, clients[i].chatRoomId , disc_sd);
                        
                        close(disc_sd);
                        FD_CLR(disc_sd, &original_fd_list);
                        serverOperation_WipeClientRecord(clients, disc_sd);

                    } else { //successfully read message from client[i]

                        interpretCommand(masterBuffer, clients, i);

                        bzero(&masterBuffer, BUFFER_LENGTH);
                        if (--io_ready_count <= 0) break;
                    }
                }
            }
        }
        //</editor-fold>

    }
#pragma clang diagnostic pop

    return 0;
}

/****************************    Function implementation begins here    *********************************************/

void preServerStart_InitializeClients(Client *clientArray, int clientCount) {
    int i;
    for (i = 0; i < clientCount; i++) {
        bzero(clientArray[i].name, STANDARD_NAME_LENGTH);
        bzero(clientArray[i].chatRoomId, STANDARD_NAME_LENGTH);

        strncpy(clientArray[i].chatRoomId, LIMBO, strlen(LIMBO));
        clientArray[i].clisd = -1;
    }
}

void preServerStart_InitializeNoWaitInterval(struct timeval *interval) {
    interval->tv_usec = 0;
    interval->tv_sec = 0;
}

void serverOperation_WipeClientRecord(Client *clientArray, int client_sockDescriptor) {
    int i;
    for (i = 0; i < USERS_CAP_PER_ROOM * ROOM_COUNT; i++) {
        if (clientArray[i].clisd == client_sockDescriptor) {
            bzero(clientArray[i].name, STANDARD_NAME_LENGTH);
            bzero(clientArray[i].chatRoomId, STANDARD_NAME_LENGTH);

            strncpy(clientArray[i].chatRoomId, LIMBO, strlen(LIMBO));
            clientArray[i].clisd = -1;
            return;
        }
    }
    exit(-1);
}

void interpretCommand(char * input, Client * clients, int curClient) {
    char * command;
    input = parseCommand(input, &command);

    switch (command) {
        case LIST_ROOMS:
            listRoomsCommand();
            break;
        case JOIN_ROOM:
            joinRoomCommand(input, clients[curClient]);
            break;
        case LIST_PEOPLE:
            listPeopleCommand(clients, curClient);
            break;
        case LOG_OFF:
            logOffCommand(clients, curClient);
            break;
        case PRIVATE_CHAT:
            privateChatCommand(input);
        case END_PRIVATE_CHAT:
            endPrivateChatCommand();
        case SEND_FILE:
            sendFileCommand(input);
        case HELP:
            helpCommand();
        default:
            broadcastMessage(clients, curClient, input);
    }
}

char * parseCommand(char * input, char ** command) {
    if (input[0] == '/') {
        *command = strtok_r(input, " ", &input);
        // input should now be everything after the command name and space;
        // e.g. if input was '/j Hobby', strtok_r will return '/r', pass over the space, and turn input into just 'Hobby'
    }
    return input;
}

void listRoomsCommand(Client client) {
    printf("%s, %s\n", ROOM_1_ID, ROOM_2_ID);
    // write(client.clisd, &("%s, %s\n"), strlen(client.name));
}

void joinRoomCommand(char * roomName, Client client) {
    
}

void listPeopleCommand(Client * clients, int curClient) {
    char * roomToCheck;
    if ((clients[curClient].chatRoomId).strcmp(ROOM_1_ID) == 0) {
        roomToCheck = ROOM_1_ID;
    } else if ((clients[curClient].chatRoomId).strcmp(ROOM_2_ID) == 0) {
        roomToCheck = ROOM_2_ID;
    } else { // Client is not in a room/is in limbo
        printf("[INFO] Client is not in a room!\n");
        return ;
    }
    int j;
    for (j = 0; j < USERS_CAP_PER_ROOM * ROOM_COUNT; j++) {
        if (clients[j].clisd > 0
            && (clients[j].chatRoomId).strcmp(roomToCheck) == 0 {
            printf("%s ", clients[j].name);
            // write(clients[curClient].clisd, &(clients[j].name + " "), strlen(clients[j].name));
        }
    }
    printf("\n");
    // write instead of printf
}

void logOffCommand(Client  * clients, int curClient) {
    printf("[INFO] %s is logging off", clients[curClient].name);
    serverOperation_WipeClientRecord(clients, clients[curClient].clisd);
}

void privateChatCommand(char * personName) {

}

void endPrivateChatCommand() {

}

void sendFileCommand(char * fileName) {

}

void helpCommand() {
    printf("Here are the available commands:\n");
    printf("'/r'          -  list the rooms on the server\n");
    printf("'/j roomname' -  joins the given room\n");
    printf("'/l'          -  lists people in the current roo\n");
    printf("'/x'          -  close the connection and log off the server\n");
    printf("'/p name'     -  private chat\n");
    printf("'/q'          -  end private chat\n");
    printf("'/f filename' -  send file\n");
    printf("'/h'          -  help\n");
}

void broadcastMessage(Client * clients, int curClient, char * message) {
    int j;
    for (j = 0; j < USERS_CAP_PER_ROOM * ROOM_COUNT; j++) {
        if (clients[j].clisd > 0
            && j != curClient
            && (clients[j].chatRoomId).strcmp(clients[curClient].chatRoomId) == 0) {
            write(clients[j].clisd, &message, strlen(message));
        }
    }
}
