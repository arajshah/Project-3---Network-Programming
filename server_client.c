#include "server.h"

#define DEFAULT_ROOM "Lobby"

// External variables
extern int numReaders;
extern pthread_mutex_t rw_lock;
extern pthread_mutex_t mutex;

extern User *user_head;
extern Room *room_head;
extern char const *server_MOTD;

char *trimwhitespace(char *str) {
  char *end;
  while(isspace((unsigned char)*str)) str++;
  if(*str == 0)  
    return str;
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;
  end[1] = '\0';
  return str;
}

void *client_receive(void *ptr) {
   int client = *(int *) ptr;  
   int received;
   char buffer[MAXBUFF], sbuffer[MAXBUFF];  
   char tmpbuf[MAXBUFF];  
   char cmd[MAXBUFF], username[MAX_NAME_LEN];
   char *arguments[80];
   const char *delimiters = " \t\n\r";

   send(client , server_MOTD , strlen(server_MOTD) , 0 );

   // Create a guest username
   sprintf(username,"guest%d", client);

   // Add user with write lock
   pthread_mutex_lock(&rw_lock);
   addUser(client, username);
   addUserToRoom(username, DEFAULT_ROOM);
   pthread_mutex_unlock(&rw_lock);

   while (1) {
      if ((received = read(client , buffer, MAXBUFF)) > 0) {
            buffer[received] = '\0'; 
            strcpy(cmd, buffer);  
            strcpy(sbuffer, buffer);

            // Tokenize
            arguments[0] = strtok(cmd, delimiters);
            int i = 0;
            while(arguments[i] != NULL) {
                arguments[++i] = strtok(NULL, delimiters);
                if(arguments[i] != NULL)
                    arguments[i] = trimwhitespace(arguments[i]);
            }

            if(arguments[0] == NULL) {
                // Empty command
                sprintf(buffer, "\nchat>");
                send(client , buffer , strlen(buffer) , 0 );
                continue;
            }

            // Locking strategy:
            // For commands that read from data: use reader lock
            // For commands that modify data: use writer lock

            if(strcmp(arguments[0], "create") == 0 && arguments[1] != NULL) {
               pthread_mutex_lock(&rw_lock);
               addRoom(arguments[1]);
               pthread_mutex_unlock(&rw_lock);

               sprintf(buffer, "Room '%s' created.\nchat>", arguments[1]);
               send(client , buffer , strlen(buffer) , 0 );
            }
            else if (strcmp(arguments[0], "join") == 0 && arguments[1] != NULL) {
               pthread_mutex_lock(&rw_lock);
               // Get user and add to room
               User *u = findUserBySocket(client);
               if(u && findRoomByName(arguments[1])) {
                  addUserToRoom(u->username, arguments[1]);
                  sprintf(buffer, "Joined room '%s'.\nchat>", arguments[1]);
               } else {
                  sprintf(buffer, "Room '%s' does not exist.\nchat>", arguments[1]);
               }
               pthread_mutex_unlock(&rw_lock);
               send(client , buffer , strlen(buffer) , 0 );
            }
            else if (strcmp(arguments[0], "leave") == 0 && arguments[1] != NULL) {
               pthread_mutex_lock(&rw_lock);
               User *u = findUserBySocket(client);
               if(u) {
                  removeUserFromRoom(u->username, arguments[1]);
                  sprintf(buffer, "Left room '%s'.\nchat>", arguments[1]);
               } else {
                  sprintf(buffer, "User not found.\nchat>");
               }
               pthread_mutex_unlock(&rw_lock);
               send(client , buffer , strlen(buffer) , 0 );
            }
            else if (strcmp(arguments[0], "connect") == 0 && arguments[1] != NULL) {
               pthread_mutex_lock(&rw_lock);
               User *u = findUserBySocket(client);
               User *target = findUserByName(arguments[1]);
               if(u && target) {
                  addDirectConnection(u->username, target->username);
                  sprintf(buffer, "Connected (DM) with '%s'.\nchat>", target->username);
               } else {
                  sprintf(buffer, "User '%s' not found.\nchat>", arguments[1]);
               }
               pthread_mutex_unlock(&rw_lock);
               send(client , buffer , strlen(buffer) , 0 );
            }
            else if (strcmp(arguments[0], "disconnect") == 0 && arguments[1] != NULL) {
               pthread_mutex_lock(&rw_lock);
               User *u = findUserBySocket(client);
               if(u) {
                  removeDirectConnection(u->username, arguments[1]);
                  sprintf(buffer, "Disconnected from '%s'.\nchat>", arguments[1]);
               } else {
                  sprintf(buffer, "User not found.\nchat>");
               }
               pthread_mutex_unlock(&rw_lock);
               send(client , buffer , strlen(buffer) , 0 );
            }
            else if (strcmp(arguments[0], "rooms") == 0) {
               // Reader lock for listing
               pthread_mutex_lock(&mutex);
               numReaders++;
               if(numReaders == 1) pthread_mutex_lock(&rw_lock);
               pthread_mutex_unlock(&mutex);

               // Perform read
               // Write the list of rooms into buffer
               // (Implement listAllRooms(client) to send directly)
               listAllRooms(client);

               pthread_mutex_lock(&mutex);
               numReaders--;
               if(numReaders == 0) pthread_mutex_unlock(&rw_lock);
               pthread_mutex_unlock(&mutex);
            }
            else if (strcmp(arguments[0], "users") == 0) {
               // Reader lock for listing
               pthread_mutex_lock(&mutex);
               numReaders++;
               if(numReaders == 1) pthread_mutex_lock(&rw_lock);
               pthread_mutex_unlock(&mutex);

               // Perform read
               listAllUsers(client, client);

               pthread_mutex_lock(&mutex);
               numReaders--;
               if(numReaders == 0) pthread_mutex_unlock(&rw_lock);
               pthread_mutex_unlock(&mutex);
            }
            else if (strcmp(arguments[0], "login") == 0 && arguments[1] != NULL) {
               pthread_mutex_lock(&rw_lock);
               renameUser(client, arguments[1]);
               pthread_mutex_unlock(&rw_lock);

               sprintf(buffer, "Logged in as '%s'.\nchat>", arguments[1]);
               send(client , buffer , strlen(buffer) , 0 );
            }
            else if (strcmp(arguments[0], "help") == 0 ) {
                sprintf(buffer, "Commands:\nlogin <username>\ncreate <room>\njoin <room>\nleave <room>\nusers\nrooms\nconnect <user>\ndisconnect <user>\nexit\n");
                send(client , buffer , strlen(buffer) , 0 );
                continue;
            }
            else if (strcmp(arguments[0], "exit") == 0 || strcmp(arguments[0], "logout") == 0) {
                pthread_mutex_lock(&rw_lock);
                User *u = findUserBySocket(client);
                if(u) {
                   removeAllUserConnections(u->username);
                   // remove user from all rooms
                   // free user resources
                   removeUser(client);
                }
                pthread_mutex_unlock(&rw_lock);
                close(client);
                pthread_exit(NULL);
            } else {
                 // Sending a message:
                 // Find the user who sent it
                 pthread_mutex_lock(&mutex);
                 numReaders++;
                 if(numReaders == 1) pthread_mutex_lock(&rw_lock);
                 pthread_mutex_unlock(&mutex);

                 User *sender = findUserBySocket(client);

                 pthread_mutex_lock(&mutex);
                 numReaders--;
                 if(numReaders == 0) pthread_mutex_unlock(&rw_lock);
                 pthread_mutex_unlock(&mutex);

                 if(sender == NULL) {
                    sprintf(tmpbuf,"\nchat>");
                    send(client, tmpbuf, strlen(tmpbuf), 0);
                    continue;
                 }

                 // Format the message
                 sprintf(tmpbuf,"\n::%s> %s\nchat>", sender->username, sbuffer);
                 strcpy(sbuffer, tmpbuf);

                 // Broadcast the message to all users in same rooms and direct connections
                 // Get list of recipients from data structure:
                 // Lock read to find recipients
                 pthread_mutex_lock(&mutex);
                 numReaders++;
                 if(numReaders == 1) pthread_mutex_lock(&rw_lock);
                 pthread_mutex_unlock(&mutex);

                 // For simplicity, let’s assume we send to all users sharing at least one room or in direct connections:
                 // (Implement a function that finds all recipients based on sender’s rooms and direct connections)
                 // Example: sendMessageToRecipients(sender, sbuffer);

                 pthread_mutex_lock(&mutex);
                 numReaders--;
                 if(numReaders == 0) pthread_mutex_unlock(&rw_lock);
                 pthread_mutex_unlock(&mutex);
            }

            memset(buffer, 0, sizeof(buffer));
      }
   }

   return NULL;
}
