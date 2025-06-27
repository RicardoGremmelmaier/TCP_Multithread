// ===================== SERVIDOR TCP =====================
/*
 * Autor: Ricardo Marthus Gremmelmaier
 * Data: 2025-06-27
 * Versão: 1.0
*/

/* 
 * Criação de um servidor TCP multithread que escuta em uma porta específica e retorna os arquivos solicitados pelo cliente
 * O servidor imprime o IP local e a porta em que está escutando, e recebe mensagens de clientes, processando-as e respondendo de acordo.
 * O protocolo de aplicação será semelhante ao protocolo HTTP, onde o cliente solicita um arquivo e o servidor responde com o conteúdo do arquivo (Get filename.ext).
 * Caso o arquivo não exista, o servidor deve retornar uma mensagem de erro apropriada.
 * O servidor deve ser capaz de lidar com múltiplos clientes simultaneamente.
 * O servidor deve ser capaz de lidar com mensagens de erro e retornar mensagens apropriadas ao cliente.
 * O servidor deve ser capaz de lidar com mensagens de controle, como "sair" ou "desconectar", e encerrar a conexão com o cliente.
 * O servidor deve ser capaz de abrir um chat simples com o cliente, onde o cliente pode enviar mensagens e o servidor responde.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <pthread.h>

#define PORT 5555
#define BUFFER_SIZE 1024

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    ssize_t bytes;

    while ((bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes] = '\0';

        if (strcmp(buffer, "FIN") == 0) {
            printf("Cliente desconectou.\n");
            break;
        }

        if (strncmp(buffer, "GET ", 4) == 0) {
            char *filename = buffer + 4;
            FILE *file = fopen(filename, "rb");
            if (!file) {
                perror("Erro ao abrir arquivo");
                send(client_sock, "ERROR: Arquivo não encontrado", 30, 0);
                continue;
            }

            while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                send(client_sock, buffer, bytes, 0);
            }
            fclose(file);

            send(client_sock, "EOF", 3, 0);
            printf("Arquivo '%s' enviado.\n", filename);
        } else {
            printf("Comando desconhecido: %s\n", buffer);
        }
    }

    close(client_sock);
    return NULL;
}

void print_local_ip() {
    struct ifaddrs *ifaddr, *ifa;
    char ip[INET_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET &&
            strcmp(ifa->ifa_name, "eth0") == 0) {

            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), ip, INET_ADDRSTRLEN);
            printf("Servidor rodando no IP: %s:%d\n", ip, PORT);
        }
    }

    freeifaddrs(ifaddr);
}

int main() {
    int sockfd, *client_sock;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Erro no bind");
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 5);
    print_local_ip();

    while (1) {
        client_sock = malloc(sizeof(int));
        *client_sock = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
        if (*client_sock < 0) {
            perror("Erro no accept");
            free(client_sock);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_sock);
        pthread_detach(tid);
    }

    close(sockfd);
    return 0;
}