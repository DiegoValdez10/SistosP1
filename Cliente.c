#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define MAX_USERNAME_LENGTH 50

typedef struct {
    char username[MAX_USERNAME_LENGTH];
    char ip[INET_ADDRSTRLEN];
    char status[20];
} UserInfo;

void displayMenu() {
    printf("\nOpciones disponibles:\n");
    printf("1. Registrar\n");
    printf("2. Enviar mensaje privado\n");
    printf("3. Cambiar de estado\n");
    printf("4. Listar los usuarios conectados\n");
    printf("5. Obtener información de un usuario\n");
    printf("6. Chatear grupalmente\n");
    printf("7. Ayuda\n");
    printf("8. Salir\n");
    printf("Elige una opción: ");
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <nombredeusuario> <IPdelservidor> <puertodelservidor>\n", argv[0]);
        exit(1);
    }

    char *username = argv[1];
    char *ip = argv[2];
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

                char recipient_username[MAX_USERNAME_LENGTH];
                printf("Enter recipient username: ");
                fgets(recipient_username, MAX_USERNAME_LENGTH, stdin);
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

                char *new_status;
                switch (choice) {
                    case 1:
                        new_status = "ACTIVO";
                        break;
                    case 2:
                        new_status = "OCUPADO";
                        break;
                    case 3:
                        new_status = "INACTIVO";
                        break;
                    default:
                        printf("Opción no válida. Selecciona un estado válido.\n");
                        continue;
                }

                send(sock, new_status, strlen(new_status) + 1, 0);

                option = 5;
                send(sock, &option, sizeof(int), 0);
                break;
            }

            case 4:
                send(sock, &option, sizeof(int), 0);

                int num_users;
                recv(sock, &num_users, sizeof(int), 0);

                printf("Connected users:\n");
                for (int i = 0; i < num_users; i++) {
                    UserInfo user_info;
                    recv(sock, &user_info, sizeof(UserInfo), 0);
                    printf("- Username: %s | Status: %s\n", user_info.username, user_info.status);
                }
                break;

            case 5: {
                send(sock, &option, sizeof(int), 0);

                char target_username[MAX_USERNAME_LENGTH];
                printf("Enter username to get information: ");
                fgets(target_username, MAX_USERNAME_LENGTH, stdin);
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
                } else {
                    printf("User not found.\n");
                }
                break;
            }

            case 6: {
                printf("Enter message to broadcast ('quit' to exit): ");
                char buffer[BUFFER_SIZE];
                fgets(buffer, BUFFER_SIZE, stdin);
                buffer[strcspn(buffer, "\n")] = 0;

                if (strcmp(buffer, "quit") == 0) {
                    break;
                }

                send(sock, &option, sizeof(int), 0);
                send(sock, buffer, strlen(buffer), 0);
                break;
            }

            case 7:
                printf("\nComandos disponibles:\n");
                printf("1. Chatear con todos los usuarios\n");
                printf("2. Enviar y recibir mensajes directos, privados\n");
                printf("3. Cambiar de estado\n");
                printf("4. Listar los usuarios conectados\n");
                printf("5. Obtener información de un usuario\n");
                printf("6. Ayuda\n");
                printf("7. Salir\n");
                break;

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

