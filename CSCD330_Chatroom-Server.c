#include <netinet/in.h>
#include <strings.h>
#include <stdio.h>
#include <sys/select.h>
#include <stdlib.h>
#include <unistd.h>

#define STANDARD_NAME_LENGTH 8
#define PORT 8080
#define BUFFER_LENGTH 1000
#define ROOM_COUNT 2
#define USERS_CAP_PER_ROOM 3

#define LIMBO "limbo"
#define ROOM_1_ID "Hobby"
#define ROOM_2_ID "330"

typedef struct {
    int clisd;
    char name[STANDARD_NAME_LENGTH];
    char chatRoomId[STANDARD_NAME_LENGTH];
} Client;

/*******************  Function prototypes begin here  ************************/

void preServerStart_InitializeClients(Client *, int);

void preServerStart_InitializeNoWaitInterval(struct timeval *interval);

void duringServerOperation_WipeClientRecord(Client *clientArray, int client_sockDescriptor);


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

    printf("[INFO] Server online at port %d\n", PORT);

    FD_ZERO(&original_fd_list);
    FD_SET(server_sd, &original_fd_list);
    max_fd = server_sd;


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {

        //<editor-fold desc="STAGE 1 - Server detecting incoming connection">
        current_fd_list = original_fd_list;
        io_ready_count = select(max_fd + 1, &current_fd_list, NULL, NULL, &noWait_Interval);

        if (FD_ISSET(server_sd, &current_fd_list)) {
            int newC_sd = accept(server_sd, NULL, NULL);

            int i;
            for (i = 0; i < USERS_CAP_PER_ROOM * ROOM_COUNT; i++) {
                if (clients[i].clisd < 0) {
                    clients[i].clisd = newC_sd;
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
                    bzero(&masterBuffer, BUFFER_LENGTH);
                    ssize_t read_res = read(clients[i].clisd, masterBuffer, BUFFER_LENGTH - 1); //space a char for the \0

                    if (read_res <= 0) {
                        close(clients[i].clisd);
                        FD_CLR(clients[i].clisd, &original_fd_list);
                        duringServerOperation_WipeClientRecord(clients, clients[i].clisd);
                    } else { //successfully read message from client[i]

                        //todo [2] Interpret commands
                        int j;
                        for (j = 0; j < USERS_CAP_PER_ROOM * ROOM_COUNT; j++) {
                            if (clients[j].clisd > 0 && j != i) {
                                write(clients[j].clisd, &masterBuffer, strlen(masterBuffer));
                            }
                        }

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

void duringServerOperation_WipeClientRecord(Client *clientArray, int client_sockDescriptor) {
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
