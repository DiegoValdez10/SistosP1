#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define DEFAULT_PORT 8888

// Estructura para representar a un cliente
typedef struct {
    int socket;
    char username[50];
    char ip[INET_ADDRSTRLEN];
    char status[20];
} Client;

// Estructura para almacenar la información del servidor
typedef struct {
    Client clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t mutex;
} ServerInfo;

ServerInfo server;

// Prototipos de funciones
void *manejar_cliente(void *arg);
void manejar_solicitud(int client_socket, int option);
void registrar_usuario(int client_socket, char *username, char *ip);
void enviar_usuarios_conectados(int client_socket);
void cambiar_estado(int client_socket, const char *new_status);
void enviar_mensaje(char *recipient_username, char *message, int sender_socket);
void enviar_info_usuario(int client_socket, char *username);
void enviar_respuesta(int client_socket, int option, int code, char *message);
void mensaje_broadcast(char *message, int sender_socket);
void enviar_respuesta_simple(int client_socket, const char *message); 

// Función para manejar las conexiones de los clientes
void *manejar_cliente(void *arg) {
    int client_socket = *((int *)arg);
    int option;

    // Leer la opción del cliente
    if (recv(client_socket, &option, sizeof(int), 0) <= 0) {
        perror("Error al recibir la opción del cliente");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Manejar la solicitud del cliente
    manejar_solicitud(client_socket, option);

    close(client_socket);
    pthread_exit(NULL);
}

// Función para manejar las solicitudes de los clientes
void manejar_solicitud(int client_socket, int option) {
    int code;
    char message[BUFFER_SIZE];

    switch (option) {
        case 1: // Registro de Usuarios
            {
                char username[50], ip[INET_ADDRSTRLEN];
                // Recibir los datos de registro del cliente
                if (recv(client_socket, &username, sizeof(username), 0) <= 0 || recv(client_socket, &ip, sizeof(ip), 0) <= 0) {
                    perror("Error al recibir los datos de registro del cliente");
                    close(client_socket);
                    pthread_exit(NULL);
                }
                registrar_usuario(client_socket, username, ip);
            }
            break;
        case 2: // Usuarios Conectados
            {
                char recipient[50], message_text[BUFFER_SIZE];
                if (recv(client_socket, recipient, sizeof(recipient), 0) <= 0 || recv(client_socket, message_text, sizeof(message_text), 0) <= 0) {
                    perror("Error receiving message data from client");
                    close(client_socket);
                    pthread_exit(NULL);
                }

                int sender_index = -1;
                for (int i = 0; i < server.client_count; i++) {
                    if (server.clients[i].socket == client_socket) {
                        sender_index = i;
                        break;
                    }
                }

                if (sender_index == -1) {
                    perror("Error: Sender not found");
                    close(client_socket);
                    pthread_exit(NULL);
                }

                enviar_mensaje(recipient, message_text, client_socket);


                printf("Mensaje privado recibido de %s: %s\n", server.clients[sender_index].username, message_text);
            }
            break;
        case 3: // Cambio de Estado
            {
                char status[20];
                if (recv(client_socket, &status, sizeof(status), 0) <= 0) {
                    perror("Error receiving status change data from client");
                    close(client_socket);
                    pthread_exit(NULL);
                }
                
                cambiar_estado(client_socket, status);
            }
            break;
        case 4: 
            {
                enviar_usuarios_conectados(client_socket);
            }
            break;
        case 5: // Información de un usuario en particular
            {
                char username[50];
                if (recv(client_socket, &username, sizeof(username), 0) <= 0) {
                    perror("Error receiving username from client");
                    close(client_socket);
                    pthread_exit(NULL);
                }
                enviar_info_usuario(client_socket, username);
            }
            break;
        case 6:
           {

                if (recv(client_socket, message, BUFFER_SIZE, 0) <= 0) {
                    perror("Error receiving broadcast message from client");
                    close(client_socket);
                    pthread_exit(NULL);
                }
                
                mensaje_broadcast(message, client_socket);
            }
            break;
        default:
            // Opción no válida
            code = 500;
            sprintf(message, "Error: Opción inválida");
            enviar_respuesta(client_socket, option, code, message);
            break;
    }
}

// Función para registrar a un nuevo usuario
void registrar_usuario(int client_socket, char *username, char *ip) {
    int code;
    char message[BUFFER_SIZE];

    // Bloquear el acceso a la estructura del servidor para evitar condiciones de carrera
    pthread_mutex_lock(&server.mutex);

    // Verificar si el usuario ya está registrado
    for (int i = 0; i < server.client_count; i++) {
        if (strcmp(server.clients[i].username, username) == 0) {
            code = 500; // Usuario ya registrado
            sprintf(message, "Error: El usuario '%s' ya existe", username);
            enviar_respuesta(client_socket, 1, code, message);
            pthread_mutex_unlock(&server.mutex);
            return;
        }
    }

    // Registrar al nuevo usuario
    strcpy(server.clients[server.client_count].username, username);
    strcpy(server.clients[server.client_count].ip, ip);
    strcpy(server.clients[server.client_count].status, "ACTIVO");
    server.clients[server.client_count].socket = client_socket;
    server.client_count++;

    // Enviar respuesta exitosa al cliente
    code = 200;
    sprintf(message, "Usuario '%s' registrado con éxito", username);
    enviar_respuesta(client_socket, 1, code, message);

    pthread_mutex_unlock(&server.mutex);
}

// Función para enviar la lista de usuarios conectados al cliente
void enviar_usuarios_conectados(int client_socket) {
    int count = server.client_count;
    send(client_socket, &count, sizeof(int), 0);
    for (int i = 0; i < server.client_count; i++) {
        send(client_socket, server.clients[i].username, sizeof(server.clients[i].username), 0);
    }
}
void enviar_respuesta_simple(int client_socket, const char *message) {
    send(client_socket, message, strlen(message) + 1, 0); 
}
// Función para cambiar el estado de un usuario
void cambiar_estado(int client_socket, const char *new_status) {
    pthread_mutex_lock(&server.mutex);
    for (int i = 0; i < server.client_count; i++) {
        if (server.clients[i].socket == client_socket) {
            strcpy(server.clients[i].status, new_status);
            enviar_respuesta_simple(client_socket, "Estado actualizado con éxito.");
            break;
        }
    }
    pthread_mutex_unlock(&server.mutex);
}
// Función para enviar un mensaje a un usuario
void enviar_mensaje(char *recipient_username, char *message, int sender_socket) {
    pthread_mutex_lock(&server.mutex);

    int recipient_found = 0;
    for (int i = 0; i < server.client_count; i++) {
        if (strcmp(server.clients[i].username, recipient_username) == 0) {
            send(server.clients[i].socket, message, strlen(message), 0);
            recipient_found = 1;
            break;
        }
    }

    if (recipient_found) {
        printf("Mensaje privado enviado a %s: %s\n", recipient_username, message);
    } else {
        printf("El usuario %s no está conectado.\n", recipient_username);
    }

    pthread_mutex_unlock(&server.mutex);
}

// Función para enviar la información de un usuario al cliente
void enviar_info_usuario(int client_socket, char *username) {
    pthread_mutex_lock(&server.mutex);
    int user_found = 0; 

    for (int i = 0; i < server.client_count; i++) {
        if (strcmp(server.clients[i].username, username) == 0) {
            user_found = 1; 
            send(client_socket, &user_found, sizeof(int), 0);
            
            send(client_socket, &server.clients[i].username, sizeof(server.clients[i].username), 0);
            send(client_socket, &server.clients[i].ip, sizeof(server.clients[i].ip), 0);
            send(client_socket, &server.clients[i].status, sizeof(server.clients[i].status), 0);
            break;
        }
    }

    if (!user_found) {
        send(client_socket, &user_found, sizeof(int), 0);  
    }

    pthread_mutex_unlock(&server.mutex);
}

// Función para enviar una respuesta al cliente
void enviar_respuesta(int client_socket, int option, int code, char *message) {
    send(client_socket, &option, sizeof(int), 0);
    send(client_socket, &code, sizeof(int), 0);
    send(client_socket, message, strlen(message), 0);
}


void mensaje_broadcast(char *message, int sender_socket) {
    pthread_mutex_lock(&server.mutex);

    printf("Mensaje broadcast recibido: %s\n", message);

    for (int i = 0; i < server.client_count; i++) {
        if (server.clients[i].socket != sender_socket) {
            send(server.clients[i].socket, message, strlen(message), 0);
        }
    }

    pthread_mutex_unlock(&server.mutex);
}


int main(int argc, char *argv[]) {
    int server_socket, client_socket, port;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t tid;

    // Verificar si se proporcionó un puerto como argumento, de lo contrario, usar el puerto predeterminado
    port = (argc < 2) ? DEFAULT_PORT : atoi(argv[1]);

    // Crear socket del servidor
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error en la creación del socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Enlazar el socket del servidor
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en el enlace del socket");
        exit(EXIT_FAILURE);
    }

    // Escuchar por conexiones entrantes
    if (listen(server_socket, 5) < 0) {
        perror("Error en la escucha del socket");
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en el puerto %d...\n", port);

    // Inicializar la estructura del servidor
    server.client_count = 0;
    pthread_mutex_init(&server.mutex, NULL);

    while (1) {
        // Aceptar la conexión entrante
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            perror("Error al aceptar la conexión");
            exit(EXIT_FAILURE);
        }

        // Crear un hilo para manejar al cliente
        if (pthread_create(&tid, NULL, manejar_cliente, &client_socket) != 0) {
            perror("Error en la creación del hilo");
            exit(EXIT_FAILURE);
        }
    }

    close(server_socket);
    pthread_mutex_destroy(&server.mutex);

    return 0;
}

