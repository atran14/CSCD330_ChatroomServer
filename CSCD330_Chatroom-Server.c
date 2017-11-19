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

#define LIST_ROOMS 'r'
#define JOIN_ROOM 'j'
#define LIST_PEOPLE 'l'
#define LOG_OFF 'x'
#define PRIVATE_CHAT 'p'
#define END_PRIVATE_CHAT 'q'
#define SEND_FILE 'f'
#define HELP 'h'

#define UNDEFINED_COMMAND_ID -1
#define LIST_ROOMS_COMMAND_ID 0
#define JOIN_ROOM_COMMAND_ID 1
#define LIST_PEOPLE_COMMAND_ID 2
#define LOG_OFF_COMMAND_ID 3
#define PRIVATE_CHAT_COMMAND_ID 4
#define END_PRIVATE_CHAT_COMMAND_ID 5
#define SEND_FILE_COMMAND_ID 6
#define HELP_COMMAND_ID 7

typedef struct {
    int clisd;
    char name[STANDARD_NAME_LENGTH + 1];
    char chatRoomId[STANDARD_NAME_LENGTH];
} Client;

/*******************  Function prototypes begin here  ************************/

void preServerStart_InitializeClients(Client *, int);

void preServerStart_InitializeNoWaitInterval(struct timeval *interval);

void serverOperation_WipeClientRecord(Client *clientArray, int client_sockDescriptor);

void getClientName(Client *newClient, char *masterBuffer);

void interpretCommand(char *input, Client *clients, int curClient);

char *parseCommand(char *input, int *command);

void listRoomsCommand(Client client);

void joinRoomCommand(char *roomName, Client *client);

void listPeopleCommand(Client *clients, int curClient);

void logOffCommand(Client *clients, int curClient);

void privateChatCommand(char *personName);

void endPrivateChatCommand();

void sendFileCommand(char *fileName);

void helpCommand(Client *client);

void broadcastMessage(Client *clients, int curClient, char *message);

void stripNewLine(char *array);

void trim(char ** word);

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

    bzero(masterBuffer, BUFFER_LENGTH);

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
                    printf("[INFO] Client %d is initialized with sock_def of %d w/ default name \"LIMBO\"\n", i,
                           newC_sd);
                    getClientName(&(clients[i]), masterBuffer);
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
                    ssize_t read_res = read(clients[i].clisd, masterBuffer,
                                            BUFFER_LENGTH - 1); //space a char for the \0

                    if (read_res <= 0) {
                        int disc_sd = clients[i].clisd;
                        printf("[INFO] Client %d (%s@%d) disconnected\n", i, clients[i].chatRoomId, disc_sd);

                        close(disc_sd);
                        FD_CLR(disc_sd, &original_fd_list);
                        serverOperation_WipeClientRecord(clients, disc_sd);

                    } else { //successfully read message from client[i]

                        interpretCommand(masterBuffer, clients, i);

                        bzero(masterBuffer, BUFFER_LENGTH);
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

void getClientName(Client *newClient, char *masterBuffer) {
    write(newClient->clisd, "Enter your name: \n", BUFFER_LENGTH - 1);
    bzero(masterBuffer, BUFFER_LENGTH);
    read(newClient->clisd, masterBuffer, BUFFER_LENGTH - 1);
    stripNewLine(masterBuffer);
    strncpy(newClient->name, masterBuffer, STANDARD_NAME_LENGTH);
    write(newClient->clisd, "Welcome to E-Chat.\n", BUFFER_LENGTH - 1);
    bzero(masterBuffer, BUFFER_LENGTH);
}

void interpretCommand(char *input, Client *clients, int curClient) {
    int commandId = -1;
    char *arguments = parseCommand(input, &commandId);

    switch (commandId) {
        case LIST_ROOMS_COMMAND_ID:
            listRoomsCommand(clients[curClient]);
            break;
        case JOIN_ROOM_COMMAND_ID:
            joinRoomCommand(arguments, &(clients[curClient]));
            break;
        case LIST_PEOPLE_COMMAND_ID:
            listPeopleCommand(clients, curClient);
            break;
        case LOG_OFF_COMMAND_ID:
            logOffCommand(clients, curClient);
            break;
        case PRIVATE_CHAT_COMMAND_ID:
            privateChatCommand(arguments);
            break;
        case END_PRIVATE_CHAT_COMMAND_ID:
            endPrivateChatCommand();
            break;
        case SEND_FILE_COMMAND_ID:
            sendFileCommand(arguments);
            break;
        case HELP_COMMAND_ID:
            helpCommand(&(clients[curClient]));
            break;
        default:
            broadcastMessage(clients, curClient, input);
    }
    bzero(input, BUFFER_LENGTH);
}

char *parseCommand(char *input, int *commandId) {
    if (input[0] == '/') {
        char *command = strtok_r(input, " ", &input);

        switch ( command[1] ) {
            case LIST_ROOMS:
                *commandId = LIST_ROOMS_COMMAND_ID;
                break;
            case JOIN_ROOM:
                *commandId = JOIN_ROOM_COMMAND_ID;
                break;
            case LIST_PEOPLE:
                *commandId = LIST_PEOPLE_COMMAND_ID;
                break;
            case LOG_OFF:
                *commandId = LOG_OFF_COMMAND_ID;
                break;
            case PRIVATE_CHAT:
                *commandId = PRIVATE_CHAT_COMMAND_ID;
                break;
            case END_PRIVATE_CHAT:
                *commandId = END_PRIVATE_CHAT_COMMAND_ID;
                break;
            case SEND_FILE:
                *commandId = SEND_FILE_COMMAND_ID;
                break;
            case HELP:
                *commandId = HELP_COMMAND_ID;
                break;
            default:
                *commandId = UNDEFINED_COMMAND_ID;
                break;
        }

    }
    return input;
}

void listRoomsCommand(Client client) {
    char roomList[80];
    sprintf(roomList, "%s, %s\n", ROOM_1_ID, ROOM_2_ID);
    write(client.clisd, roomList, strlen(roomList));
}

void joinRoomCommand(char *roomName, Client *client) {
    trim(&roomName);
    if(strcmp(roomName, ROOM_1_ID) == 0) {
        strncpy(client->chatRoomId, ROOM_1_ID, STANDARD_NAME_LENGTH);
        printf("[DEBUG] client->chatRoomId: %s\n", client->chatRoomId);
    }
    else if(strcmp(roomName, ROOM_2_ID) == 0) {
        strncpy(client->chatRoomId, ROOM_2_ID, STANDARD_NAME_LENGTH);
        printf("[DEBUG] client->chatRoomId: %s\n", client->chatRoomId);
    }
    else {
        write(client->clisd, "Invalid room name.\n", 30);
    }
    //todo [2.1.joinRoom] Need to prefix the client's prompt with their current room name
}

void listPeopleCommand(Client *clients, int curClient) {
    char *roomToCheck;
    if (strcmp(clients[curClient].chatRoomId, ROOM_1_ID) == 0) {
        roomToCheck = ROOM_1_ID;
    } else if (strcmp(clients[curClient].chatRoomId, ROOM_2_ID) == 0) {
        roomToCheck = ROOM_2_ID;
    } else { // Client is not in a room/is in limbo
        write(clients[curClient].clisd, "You are not in a room!\n", 30);
        return;
    }
    char personInRoom[STANDARD_NAME_LENGTH + 1];
    int j;
    for (j = 0; j < USERS_CAP_PER_ROOM * ROOM_COUNT; j++) {
        if (clients[j].clisd > 0
            && strcmp(clients[j].chatRoomId, roomToCheck) == 0) {
            sprintf(personInRoom, "%s ", clients[j].name);
            write(clients[curClient].clisd, personInRoom, strlen(personInRoom));
        }
    }
    write(clients[curClient].clisd, "\n", 3);
}

void logOffCommand(Client *clients, int curClient) {
    printf("[DEBUG] logOffCommand\n");
    // //todo [2.1.LogOff] Need to follow the procedure in the server loop for closing client socket (close() -> clear fd in the fd_set -> serverOp_WipeCleanRecord)
    // printf("[INFO] %s is logging off", clients[curClient].name);
    // serverOperation_WipeClientRecord(clients, clients[curClient].clisd);
}

void privateChatCommand(char *personName) {
    printf("[DEBUG] privateChatCommand\n");
}

void endPrivateChatCommand() {
    printf("[DEBUG] endPrivateChatCommand\n");
}

void sendFileCommand(char *fileName) {
    printf("[DEBUG] sendFileCommand\n");
}

void helpCommand(Client *client) {
    int stringLength = 600;
    char helpMessage[600];
    snprintf(helpMessage, stringLength, "%s%s%s%s%s%s%s%s%s",
        "Here are the available commands:\n",
        "'/r'          -  list the rooms on the server\n",
        "'/j roomname' -  joins the given room\n",
        "'/l'          -  lists people in the current room\n",
        "'/x'          -  close the connection and log off the server\n",
        "'/p name'     -  private chat\n",
        "'/q'          -  end private chat\n",
        "'/f filename' -  send file\n",
        "'/h'          -  help\n"
    );
    write(client->clisd, helpMessage, stringLength);
}

void broadcastMessage(Client *clients, int curClient, char *message) {
    printf("[DEBUG] broadcastMessage to %s\n", clients[curClient].chatRoomId);
    char nameAndMessage[BUFFER_LENGTH + 15];
    sprintf(nameAndMessage, "(%s) %s", clients[curClient].name, message);
    int j;
    for (j = 0; j < USERS_CAP_PER_ROOM * ROOM_COUNT; j++) {
        if (clients[j].clisd > 0
            && j != curClient
            && strcmp(clients[j].chatRoomId, clients[curClient].chatRoomId) == 0) {
            write(clients[j].clisd, nameAndMessage, strlen(nameAndMessage));
        }
    }
    //todo [2.1.Broadcast] Need to alter the condition to check if client is in private chat
}

void stripNewLine(char *array) {
	if(array == NULL) {
		perror("array is null");
		exit(-99);
	}

	int length = strlen(array), x = 0;
   
	while(array[x] != '\0' && x < length) {
	  if(array[x] == '\r') {
          array[x] = '\0';
      }
	  else if(array[x] == '\n') {
          array[x] = '\0';
      }

	  x++;
    }
}

// remove a single space ' ' on either end of the string. Does NOT remove it in the middle; does not remove multiple spaces.
void trim(char ** word) {
	if(word == NULL) {
		perror("word is null");
		exit(-99);
	}// end if

    stripNewLine(*word);

	int length = strlen(*word);

    if(length > 1) {
        if((*word)[0] == ' ') {
            *word = *word + 1;
        }
        if((*word)[length - 1] == ' ') {
            (*word)[length - 1] = '\0';
        }
    }
}
