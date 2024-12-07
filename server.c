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
Room *room_head = NULL;

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

