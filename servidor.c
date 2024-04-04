#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define MAX_USERNAME_LEN 50
#define DEFAULT_PORT 8888

typedef struct {
    int socket;
    char username[MAX_USERNAME_LEN];
    char ip[INET_ADDRSTRLEN];
    char status[20];
    char message[BUFFER_SIZE];
} Client;

typedef struct {
    Client clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t mutex;
    char broadcast_messages[BUFFER_SIZE * MAX_CLIENTS];
    int broadcast_message_count;
    
} Server;

Server server;

void *handle_client(void *arg);
void handle_request(int client_socket, int option);
void register_user(int client_socket, char *username, char *ip);
void send_connected_users(int client_socket);
void change_status(int client_socket, const char *new_status);
void send_private_message(char *recipient_username, char *message, int sender_socket);
void send_user_info(int client_socket, char *username);
void send_response(int client_socket, int option, int code, char *message);
void broadcast_message(char *message, int sender_socket);
void send_broadcast_messages(int client_socket);

void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    int option;
    char message[BUFFER_SIZE];

if (recv(client_socket, message, sizeof(message), 0) <= 0) {
    perror("Error receiving broadcast message from client");
    close(client_socket);
    pthread_exit(NULL);
}
    handle_request(client_socket, option);
    close(client_socket);
    pthread_exit(NULL);
}

void handle_request(int client_socket, int option) {
    int code;
    char message[BUFFER_SIZE];

    switch (option) {
        case 1: {
            char username[MAX_USERNAME_LEN], ip[INET_ADDRSTRLEN];
            if (recv(client_socket, username, sizeof(username), 0) <= 0 ||
                recv(client_socket, ip, sizeof(ip), 0) <= 0) {
                perror("Error receiving registration data from client");
                close(client_socket);
                pthread_exit(NULL);
            }
            register_user(client_socket, username, ip);
            break;
        }
        case 2: {
            char recipient[MAX_USERNAME_LEN], message_text[BUFFER_SIZE];
            if (recv(client_socket, recipient, sizeof(recipient), 0) <= 0 ||
                recv(client_socket, message_text, sizeof(message_text), 0) <= 0) {
                perror("Error receiving message data from client");
                close(client_socket);
                pthread_exit(NULL);
            }
            send_private_message(recipient, message_text, client_socket);
            break;
        }
        case 3: {
            char status[20];
            if (recv(client_socket, status, sizeof(status), 0) <= 0) {
                perror("Error receiving status change data from client");
                close(client_socket);
                pthread_exit(NULL);
            }
            change_status(client_socket, status);
            break;
        }
        case 4:
            send_connected_users(client_socket);
            break;
        case 5: {
            char username[MAX_USERNAME_LEN];
            if (recv(client_socket, username, sizeof(username), 0) <= 0) {
                perror("Error receiving username from client");
                close(client_socket);
                pthread_exit(NULL);
            }
            send_user_info(client_socket, username);
            break;
        }
case 6: {
    char broadcast_msg[BUFFER_SIZE]; // Renamed variable to avoid conflict
    if (recv(client_socket, broadcast_msg, sizeof(broadcast_msg), 0) <= 0) {
        perror("Error receiving broadcast message from client");
        close(client_socket);
        pthread_exit(NULL);
    }
    broadcast_message(broadcast_msg, client_socket); // Using the function, not the array
    break;
}


        case 7: {
            send_broadcast_messages(client_socket);
            break;
        }
        default:
            code = 500;
            sprintf(message, "Error: Invalid option");
            send_response(client_socket, option, code, message);
            break;
    }
}

void register_user(int client_socket, char *username, char *ip) {
    int code;
    char message[BUFFER_SIZE];

    pthread_mutex_lock(&server.mutex);

    for (int i = 0; i < server.client_count; i++) {
        if (strcmp(server.clients[i].username, username) == 0) {
            code = 500;
            sprintf(message, "Error: Username '%s' already exists", username);
            send_response(client_socket, 1, code, message);
            pthread_mutex_unlock(&server.mutex);
            return;
        }
    }

    int index = server.client_count;
    strcpy(server.clients[index].username, username);
    strcpy(server.clients[index].ip, ip);
    strcpy(server.clients[index].status, "ONLINE");
    server.clients[index].socket = client_socket;
    server.client_count++;

    code = 200;
    sprintf(message, "User '%s' registered successfully", username);
    send_response(client_socket, 1, code, message);
    printf("User '%s' registered successfully\n", username);

    pthread_mutex_unlock(&server.mutex);
}

void send_connected_users(int client_socket) {
    int count = server.client_count;
    send(client_socket, &count, sizeof(int), 0);
    for (int i = 0; i < server.client_count; i++) {
        send(client_socket, server.clients[i].username, sizeof(server.clients[i].username), 0);
        send(client_socket, server.clients[i].status, sizeof(server.clients[i].status), 0);
    }
}

void change_status(int client_socket, const char *new_status) {
    pthread_mutex_lock(&server.mutex);

    for (int i = 0; i < server.client_count; i++) {
        if (server.clients[i].socket == client_socket) {
            strcpy(server.clients[i].status, new_status);
            break;
        }
    }

    pthread_mutex_unlock(&server.mutex);
}

void send_private_message(char *recipient_username, char *message, int sender_socket) {
    pthread_mutex_lock(&server.mutex);

    int recipient_index = -1;
    int sender_index = -1;

    for (int i = 0; i < server.client_count; i++) {
        if (server.clients[i].socket == sender_socket) {
            sender_index = i;
        } else if (strcmp(server.clients[i].username, recipient_username) == 0) {
            recipient_index = i;
        }
    }

    if (recipient_index == -1) {
        printf("User '%s' not found\n", recipient_username);
        pthread_mutex_unlock(&server.mutex);
        return;
    }

    int recipient_socket = server.clients[recipient_index].socket;
    char sender_username[MAX_USERNAME_LEN];
    strcpy(sender_username, server.clients[sender_index].username);

    send(recipient_socket, sender_username, strlen(sender_username), 0);
    send(recipient_socket, message, strlen(message), 0);

    printf("Message sent from '%s' to '%s': %s\n", sender_username,recipient_username, message);

    pthread_mutex_unlock(&server.mutex);
}

void send_user_info(int client_socket, char *username) {
    pthread_mutex_lock(&server.mutex);
    int user_found = 0;

    for (int i = 0; i < server.client_count; i++) {
        if (strcmp(server.clients[i].username, username) == 0) {
            user_found = 1;
            send(client_socket, &user_found, sizeof(int), 0);
            send(client_socket, server.clients[i].username, sizeof(server.clients[i].username), 0);
            send(client_socket, server.clients[i].ip, sizeof(server.clients[i].ip), 0);
            send(client_socket, server.clients[i].status, sizeof(server.clients[i].status), 0);
            break;
        }
    }

    if (!user_found) {
        user_found = 0;
        send(client_socket, &user_found, sizeof(int), 0);
    }

    pthread_mutex_unlock(&server.mutex);
}

void send_response(int client_socket, int option, int code, char *message) {
    send(client_socket, &option, sizeof(int), 0);
    send(client_socket, &code, sizeof(int), 0);
    send(client_socket, message, strlen(message), 0);
}

void broadcast_message(char *message, int sender_socket) {
    pthread_mutex_lock(&server.mutex);

    printf("Broadcast message received: %s\n", message);

    int len = strlen(message);
    memcpy(server.broadcast_messages + server.broadcast_message_count, message, len + 1);
    server.broadcast_message_count += len + 1;

    for (int i = 0; i < server.client_count; i++) {
        if (server.clients[i].socket != sender_socket) {
            send(server.clients[i].socket, message, len, 0);
        }
    }

    pthread_mutex_unlock(&server.mutex);
}

void send_broadcast_messages(int client_socket) {
    pthread_mutex_lock(&server.mutex);

    send(client_socket, server.broadcast_messages, server.broadcast_message_count, 0);
    send(client_socket, "###END_BROADCAST_MESSAGES###", strlen("###END_BROADCAST_MESSAGES###"), 0);

    pthread_mutex_unlock(&server.mutex);
}

int main(int argc, char *argv[]) {
    int server_socket, client_socket, port;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t tid;

    port = (argc < 2) ? DEFAULT_PORT : atoi(argv[1]);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error creating server socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding server socket");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Error listening on server socket");
        exit(EXIT_FAILURE);
    }

    printf("Server running on port %d...\n", port);

    server.client_count = 0;
    pthread_mutex_init(&server.mutex, NULL);
    server.broadcast_message_count = 0;

    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            perror("Error accepting client connection");
            exit(EXIT_FAILURE);
        }

        if (pthread_create(&tid, NULL, handle_client, &client_socket) != 0) {
            perror("Error creating client thread");
            exit(EXIT_FAILURE);
        }
    }

    close(server_socket);
    pthread_mutex_destroy(&server.mutex);

    return 0;
}
