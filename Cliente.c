#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define MAX_USERNAME_LEN 20
#define MAX_MESSAGE_LEN 256
#define BUFFER_SIZE 1024

typedef struct {
    int sockfd;
    char username[MAX_USERNAME_LEN];
    char status[20];
} Client;
typedef struct {
    char username[MAX_USERNAME_LEN];
    char ip[INET_ADDRSTRLEN];
    char status[20];
} UserInfo;

Client clients[MAX_CLIENTS];
int num_clients = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg);
void broadcast_message(const char *message, const char *sender);
void process_command(int client_index, char *command);

void displayMenu() {
    printf("Menu:\n");
    printf("1. Registrar\n");
    printf("2. Enviar mensaje privado\n");
    printf("3. Cambiar de estado\n");
    printf("4. Listar los usuarios conectados\n");
    printf("5. Obtener información de un usuario\n");
    printf("6. Broadcast\n");
    printf("7. Ayuda\n");
    printf("8. Salir\n");
    printf("Elige una opción: ");
}

void displayBroadcastMenu() {
    printf("\nSubmenú de Chatear grupalmente:\n");
    printf("1. Ver mensajes de broadcast\n");
    printf("2. Enviar mensaje al broadcast\n");
    printf("3. Volver al menú principal\n");
    printf("Elige una opción: ");
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <nombredeusuario> <IPdelservidor> <puertodelservidor>\n", argv[0]);
        exit(1);
    }

    char *username = argv[1];
    char *ip = argv[2];
    char status[20];
    int port = atoi(argv[3]);

    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[-]Socket error");
        exit(1);
    }

    printf("[+]TCP server socket created.\n");

    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }
    printf("Connected to the server.\n");

    while (1) {
        displayMenu();

        int option;
        scanf("%d", &option);
        getchar();

        switch (option) {
            case 1:
                send(sock, &option, sizeof(int), 0);
                send(sock, username, strlen(username), 0);

                char ip_address[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(addr.sin_addr), ip_address, INET_ADDRSTRLEN);
                send(sock, ip_address, strlen(ip_address), 0);

                printf("Ya está registrado: %s\n", username);
                break;

            case 2: {
                send(sock, &option, sizeof(int), 0);

                char recipient_username[MAX_USERNAME_LEN];
                printf("Enter recipient username: ");
                fgets(recipient_username, MAX_USERNAME_LEN, stdin);
                recipient_username[strcspn(recipient_username, "\n")] = '\0';

                send(sock, recipient_username, strlen(recipient_username), 0);

                char message[BUFFER_SIZE];
                printf("Enter message to send: ");
                fgets(message, BUFFER_SIZE, stdin);
                message[strcspn(message, "\n")] = '\0';
                send(sock, message, strlen(message), 0);
                break;
            }

            case 3: {
                send(sock, &option, sizeof(int), 0);

                printf("Elige un nuevo estado:\n");
                printf("1. ACTIVO\n");
                printf("2. OCUPADO\n");
                printf("3. INACTIVO\n");
                printf("Ingresa el número de tu nuevo estado: ");

                int choice;
                scanf("%d", &choice);
                getchar();

                char new_status[20];
                switch (choice) {
                    case 1:
                        strcpy(new_status, "ACTIVO");
                        break;
                    case 2:
                        strcpy(new_status, "OCUPADO");
                        break;
                    case 3:
                        strcpy(new_status, "INACTIVO");
                        break;
                    default:
                        printf("Opción no válida. Selecciona un estado válido.\n");
                        continue;
                }

                send(sock, new_status, sizeof(new_status), 0);

                char response[BUFFER_SIZE];
                recv(sock, response, sizeof(response), 0);
                printf("%s\n", response);
                break;
            }

            case 4: {
                send(sock, &option, sizeof(int), 0);

                int num_users;
                recv(sock, &num_users, sizeof(int), 0);

                printf("Connected users:\n");
                for (int i = 0; i < num_users; i++) {
                    char username [50];
                    char status [20];
                    recv(sock, username, sizeof(username), 0);
                    recv(sock, status, sizeof(status), 0);
                    printf("- Username: %s | Status: %s\n", username, status);
                }
                break;
            }

            case 5: {
                send(sock, &option, sizeof(int), 0);

                char target_username[MAX_USERNAME_LEN];
                printf("Enter username to get information: ");
                fgets(target_username, MAX_USERNAME_LEN, stdin);
                target_username[strcspn(target_username, "\n")] = '\0';
                send(sock, target_username, strlen(target_username), 0);

                int user_found;
                recv(sock, &user_found, sizeof(int), 0);

                if (user_found) {
                    UserInfo user_info;
                    recv(sock, &user_info, sizeof(UserInfo), 0);

                    printf("User info:\n");
                    printf("- Username: %s\n", user_info.username);
                    printf("- IP: %s\n", user_info.ip);
                    printf("- Status: %s\n", user_info.status);

                    // Actualizar la información del usuario local si el nombre de usuario coincide
                    if (strcmp(user_info.username, argv[1]) == 0) {
                        strcpy(argv[1], user_info.username);
                        strcpy(ip, user_info.ip);
                        strcpy(status, user_info.status);
                    }
                } else {
                    printf("User not found.\n");
                }
                break;
            }

            case 6: {
                int submenu_option;
                do {
                    displayBroadcastMenu();
                    scanf("%d", &submenu_option);
                    getchar();

                    switch (submenu_option) {
                        case 1: {
                            // Solicitar al servidor los mensajes de broadcast
                            printf("Mensajes de broadcast:\n");
                            send(sock, &option, sizeof(int), 0);

                            char broadcast_message[BUFFER_SIZE];
                            while (1) {
                                // Recibir los mensajes de broadcast del servidor
                                ssize_t bytes_received = recv(sock, broadcast_message, BUFFER_SIZE, 0);
                                if (bytes_received <= 0) {
                                    // Si no se reciben más datos, se ha terminado la transmisión
                                    printf("Fin de los mensajes de broadcast.\n");
                                    break;
                                } else if (strcmp(broadcast_message, "###END_BROADCAST_MESSAGES###") == 0) {
                                    // Si se recibe la marca de fin de los mensajes, salir del bucle
                                    printf("Fin de los mensajes de broadcast.\n");
                                    break;
                                }
                                printf("- %s\n", broadcast_message);
                            }
                            break;
                        }

                        case 2: {
                            // Enviar mensaje al broadcast
                            printf("Enter message to broadcast ('quit' to exit): ");
                            char buffer[BUFFER_SIZE];
                            fgets(buffer, BUFFER_SIZE, stdin);
                            buffer[strcspn(buffer, "\n")] = '\0';

                            if (strcmp(buffer, "quit") == 0) {
                                break;
                            }

                            send(sock, &option, sizeof(int), 0);
                            send(sock, buffer, strlen(buffer), 0);
                            break;
                        }

                        case 3:
                            break;

                        default:
                            printf("Opción no válida. Por favor, intenta de nuevo.\n");
                    }
                } while (submenu_option != 3);
                break;
            }

            case 7:
                printf("\nComandos disponibles:\n");
                printf("1. Broadcast 2. Mensajes privados 3. Cambiar de etado 4.Listar los usuario conectdos 5. Obtener información de un usuario 6. Registrarse 7. Ayuda 8. Salir\n");
                printf("Consideraciones\n");
                printf("Si se cierra la consola sin ninguna razón volver a entrar a la consola con el comando y volver a realizar la acción deseada.\n");
                printf("Ip para que los demas grupos se conecten\n");
                printf("18.221.157.193\n");
                printf("Puerto para que los demas grupos se conecten\n");
                printf("8888\n");

            case 8:
                printf("Saliendo...\n");
                close(sock);
                return 0;

            default:
                printf("Opción no válida. Por favor, intenta de nuevo.\n");
        }
    }

    return 0;
}

void *handle_client(void *arg) {
    // Esta función no se utiliza en el lado del cliente
    return NULL;
}

void broadcast_message(const char *message, const char *sender) {
    // Esta función no se utiliza en el lado del cliente
}

void process_command(int client_index, char *command) {
    // Esta función no se utiliza en el lado del cliente
}
