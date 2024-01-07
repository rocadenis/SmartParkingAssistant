#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 10024

int main() {
    int client_fd;
    struct sockaddr_in server_addr;

    // Crearea socket-ului client
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la crearea socket-ului");
        exit(EXIT_FAILURE);
    }

    // Setarea adresei serverului
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    // Conectarea la server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Eroare la conectarea la server");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // Ciclu infinit care se termină la comanda "exit"
    while (1) {
        printf("Introduceti comanda:\n");
        char command[BUFFER_SIZE];
        fgets(command, BUFFER_SIZE, stdin);

        // Trimiterea comenzii către server
        if (send(client_fd, command, strlen(command), 0) < 0) {
            perror("Eroare la trimiterea comenzii");
            close(client_fd);
            exit(EXIT_FAILURE);
        }
        // Primirea răspunsului de la server
        char response[BUFFER_SIZE];
        int bytes_received = recv(client_fd, response, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Eroare la primirea răspunsului");
            close(client_fd);
            exit(EXIT_FAILURE);
        }

        // Afișarea răspunsului de la server
        printf("Răspuns: %s\n", response);
        bzero(response, BUFFER_SIZE);
        // Verificarea comenzii de ieșire
        if (strcmp(command, "exit") == 0) {
            break;
        }
    }

    // Închiderea socket-ului client
    close(client_fd);

    return 0;
}
