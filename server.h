/* System Header Files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <ctype.h>
#include <pthread.h>

/* Local Header Files */
#include "list.h"

#define MAX_READERS 25
#define TRUE   1  
#define FALSE  0  
#define PORT 8888  
#define delimiters " "
#define max_clients  30
#define DEFAULT_ROOM "Lobby"
#define MAXBUFF   2096
#define BACKLOG 2
#define MAX_NAME_LEN 50
#define MAX_ROOMS 100
#define MAX_USERS 100
#define MAX_DIRECT_CONN 50

typedef struct DirectConn {
    char username[MAX_NAME_LEN];
    struct DirectConn *next;
} DirectConn;

typedef struct User {
    int socket;
    char username[MAX_NAME_LEN];
    // A linked list of rooms the user belongs to
    struct RoomList *rooms; 
    // A linked list of direct connections (DMs)
    DirectConn *directConns;
    struct User *next;
} User;

typedef struct RoomUser {
    char username[MAX_NAME_LEN];
    struct RoomUser *next;
} RoomUser;

typedef struct Room {
    char name[MAX_NAME_LEN];
    RoomUser *users;  
    struct Room *next;
} Room;

extern User *user_head;
extern Room *room_head;


// prototypes

int get_server_socket();
int start_server(int serv_socket, int backlog);
int accept_client(int serv_sock);
void sigintHandler(int sig_num);
void *client_receive(void *ptr);