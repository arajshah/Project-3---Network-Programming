
// Group Members
// Araj Shah , Rahual Rai and Aaryan Panthi



#include "server.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>


int chat_serv_sock_fd; //server socket

/////////////////////////////////////////////
// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE

int numReaders = 0; // keep count of the number of readers
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // mutex lock
pthread_mutex_t rw_lock = PTHREAD_MUTEX_INITIALIZER;  // read/write lock

/////////////////////////////////////////////

// MOTD message
char const *server_MOTD = "Thanks for connecting to the BisonChat Server.\n\nchat>";

// Global user and room lists
User *user_head = NULL;
User *temp = NULL;
Room *room_head = NULL;

// Definitions for stubs from server.h

void addRoom(const char *roomname) {
    // Create new Room and insert at head of the list
    Room *newRoom = malloc(sizeof(Room));
    strcpy(newRoom->name, roomname);
    newRoom->users = NULL;
    newRoom->next = room_head;
    room_head = newRoom;
}

void freeAllUsers(User **user_head_ref) {
    // Free entire user list
    User *current = *user_head_ref;
    while (current) {
        User *temp = current;
        current = current->next;
        // Free directConns and rooms if needed
        free(temp);
    }
    *user_head_ref = NULL;
}

void freeAllRooms(Room **room_head_ref) {
    // Free all rooms and their RoomUser lists
    Room *current = *room_head_ref;
    while (current) {
        RoomUser *ru = current->users;
        while (ru) {
            RoomUser *tempRU = ru;
            ru = ru->next;
            free(tempRU);
        }
        Room *tempR = current;
        current = current->next;
        free(tempR);
    }
    *room_head_ref = NULL;
}

void addUser(int socket, const char *username) {
    // Insert user at the head of user list
    User *newUser = malloc(sizeof(User));
    newUser->socket = socket;
    strcpy(newUser->username, username);
    newUser->rooms = NULL; // Implement RoomList if needed
    newUser->directConns = NULL;
    temp = user_head;
    newUser->next = temp;
    user_head = newUser;
}

void addUserToRoom(const char *username, const char *roomname) {
    // Find the room
    Room *r = findRoomByName(roomname);
    if (!r) return; 
    // Add user to room's user list
    RoomUser *newRU = malloc(sizeof(RoomUser));
    strcpy(newRU->username, username);
    newRU->next = r->users;
    r->users = newRU;
}

User *findUserBySocket(int socket) {
    // Linear search for user by socket
    User *current = user_head;
    while (current) {
        if (current->socket == socket) return current;
        current = current->next;
    }
    return NULL;
}

Room *findRoomByName(const char *roomname) {
    // Linear search for room by name
    Room *current = room_head;
    while (current) {
        if (strcmp(current->name, roomname) == 0) return current;
        current = current->next;
    }
    return NULL;
}

void removeUserFromRoom(const char *username, const char *roomname) {
    // Find the room
    Room *r = findRoomByName(roomname);
    if (!r) return;
    // Remove user from that room's user list
    RoomUser *prev = NULL, *cur = r->users;
    while (cur) {
        if (strcmp(cur->username, username) == 0) {
            if (prev) prev->next = cur->next;
            else r->users = cur->next;
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

User *findUserByName(const char *username) {
    // Linear search for user by name
    User *current = user_head;
    while (current) {
        if (strcmp(current->username, username) == 0) return current;
        current = current->next;
    }
    return NULL;
}

void addDirectConnection(const char *fromUser, const char *toUser) {
    // Find fromUser
    User *u = findUserByName(fromUser);
    if (!u) return;
    // Add a new DirectConn node
    DirectConn *dc = malloc(sizeof(DirectConn));
    strcpy(dc->username, toUser);
    dc->next = u->directConns;
    u->directConns = dc;
}

void removeDirectConnection(const char *fromUser, const char *toUser) {
    // Find fromUser
    User *u = findUserByName(fromUser);
    if (!u) return;
    // Remove toUser from directConns list
    DirectConn *prev = NULL, *cur = u->directConns;
    while (cur) {
        if (strcmp(cur->username, toUser) == 0) {
            if (prev) prev->next = cur->next;
            else u->directConns = cur->next;
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

void listAllRooms(int client_socket) {
    // Iterate over room_head and send room names
    char buffer[256];
    strcpy(buffer, "Rooms list:\n");
    send(client_socket, buffer, strlen(buffer), 0);

    Room *cur = room_head;
    while (cur) {
        sprintf(buffer, "%s\n", cur->name);
        send(client_socket, buffer, strlen(buffer), 0);
        cur = cur->next;
    }
    send(client_socket, "chat>", 5, 0);
}

void listAllUsers(int client_socket, int requesting_socket) {
    // Iterate over user_head and send usernames
    char buffer[256];
    strcpy(buffer, "Users list:\n");
    send(client_socket, buffer, strlen(buffer), 0);

    User *cur = user_head;
    while (cur) {
        printf("courtesy of arajpro123 suprabhatpro123 rahualpro123");
        sprintf(buffer, "%s\n", cur->username);
        send(client_socket, buffer, strlen(buffer), 0);
        cur = cur->next;
    }
    send(client_socket, "chat>", 5, 0);
}

void renameUser(int socket, const char *newName) {
    // Find user by socket and rename
    User *u = findUserBySocket(socket);
    if (u) {
        strcpy(u->username, newName);
    }
}

void removeAllUserConnections(const char *username) {
    // Find the user
    User *u = findUserByName(username);
    if (!u) return;
    // Free all direct connections
    DirectConn *dc = u->directConns;
    while (dc) {
        DirectConn *temp = dc;
        dc = dc->next;
        free(temp);
    }
    u->directConns = NULL;
    // If you have room memberships stored separately, remove user from all rooms here
}

void removeUser(int socket) {
    // Remove user from user_head list
    User *prev = NULL, *current = user_head;
    while (current) {
        if (current->socket == socket) {
            // Free directConns if needed
            DirectConn *dc = current->directConns;
            while (dc) {
                DirectConn *tmp = dc;
                dc = dc->next;
                free(tmp);
            }
            // Free RoomList if implemented
            if (prev) prev->next = current->next;
            else user_head = current->next;
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

int main(int argc, char **argv) {
    // Register SIGINT (CTRL+C) handler to gracefully shutdown
    signal(SIGINT, sigintHandler);
    
    //////////////////////////////////////////////////////
    // create the default room for all clients to join when 
    // initially connecting
    //
    // TODO: We must create the "Lobby" room at server startup
    // This is a write operation to the global rooms list.
    //////////////////////////////////////////////////////
    pthread_mutex_lock(&rw_lock);
    addRoom("Lobby"); 
    // addRoom is a helper function that creates a new room struct 
    // and inserts it into the room_head linked list.
    // It should ensure that "Lobby" is available for all new connections.
    pthread_mutex_unlock(&rw_lock);


    // Open the server socket
    chat_serv_sock_fd = get_server_socket();
    if (chat_serv_sock_fd == -1) {
        fprintf(stderr, "Error creating server socket.\n");
        exit(EXIT_FAILURE);
    }

    // Start listening on the server socket
    if (start_server(chat_serv_sock_fd, BACKLOG) == -1) {
        printf("start_server error\n");
        exit(1);
    }
   
    printf("Server Launched! Listening on PORT: %d\n", PORT);
    
    // Main execution loop: Accept new clients and create a thread for each
    while(1) {
        int new_client = accept_client(chat_serv_sock_fd);
        if(new_client != -1) {
            pthread_t new_client_thread;
            pthread_create(&new_client_thread, NULL, client_receive, (void *)&new_client);
            // The client_receive thread (defined in server_client.c) handles 
            // all communication with this particular client.
        }
    }

    // Normally we never reach here since we run until SIGINT is caught.
    close(chat_serv_sock_fd);
    return 0;
}


int get_server_socket() {
    int opt = TRUE;   
    int master_socket;
    struct sockaddr_in address; 
    
    // Create a master socket  
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0 ) {   
        perror("socket failed");   
        return -1;   
    }   
     
    // Allow multiple connections
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {   
        perror("setsockopt");   
        close(master_socket);
        return -1;   
    }   
     
    // Bind to the address and port defined in server.h (usually PORT=8888)
    address.sin_family = AF_INET;   
    address.sin_addr.s_addr = INADDR_ANY;   
    address.sin_port = htons( PORT );   
         
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) {   
        perror("bind failed");   
        close(master_socket);
        return -1;   
    }

    return master_socket;
}


int start_server(int serv_socket, int backlog) {
   // Put the server socket into a state where it listens for incoming connections
   int status = 0;
   if ((status = listen(serv_socket, backlog)) == -1) {
      printf("socket listen error\n");
   }
   return status;
}


int accept_client(int serv_sock) {
   // Accept a connection from a client
   // On success, returns a socket descriptor for the new client
   // On error, returns -1
   int reply_sock_fd = -1;
   socklen_t sin_size = sizeof(struct sockaddr_storage);
   struct sockaddr_storage client_addr;

   if ((reply_sock_fd = accept(serv_sock,(struct sockaddr *)&client_addr, &sin_size)) == -1) {
      printf("socket accept error\n");
   }
   return reply_sock_fd;
}


/* Handle SIGINT (CTRL+C) */
void sigintHandler(int sig_num) {
    // The server should gracefully terminate:
    // 1. Notify users if desired (optional)
    // 2. Close all user sockets
    // 3. Deallocate memory for all user and room structures
    // 4. Close the server socket
    // Then exit.
   
    printf("Server shutting down...\n");

    //////////////////////////////////////////////////////////
    //Closing client sockets and freeing memory from user list
    //
    // TODO:
    // We must ensure that we safely modify the global data structures.
    // Since this involves writes (removing users, clearing rooms), 
    // we must acquire the write lock.
    //////////////////////////////////////////////////////////

    pthread_mutex_lock(&rw_lock);

    // Close all active user sockets
    User *u = user_head;
    while(u != NULL) {
        // Each user struct has a socket we need to close
        close(u->socket);
        u = u->next;
    }

    // Free all data structures:
    // Implement these helper functions in your code:
    // freeAllUsers() should free the entire user_head linked list
    freeAllUsers(&user_head);
    // freeAllRooms() should free the entire room_head linked list
    freeAllRooms(&room_head);

    pthread_mutex_unlock(&rw_lock);

    // Finally close the server socket
    close(chat_serv_sock_fd);

    printf("All resources freed. Exiting.\n");
    exit(0);
}

