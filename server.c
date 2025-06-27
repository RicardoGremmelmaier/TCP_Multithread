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
 * O servidor deve ser capaz de responder um chat simples com o cliente, onde o cliente pode enviar mensagens e o servidor responde.
 * O servidor deve ser capaz de calcular o hash SHA256 do arquivo solicitado e enviar o hash ao cliente.
 * O servidor deve ser capaz de enviar uma mensagem para todos os clientes conectados, se necessário.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <openssl/sha.h>

#define PORT 5555
#define BUFFER_SIZE 1024
#define HASH_SIZE 65

typedef struct {
    int sock;
    struct sockaddr_in addr;
} client_info_t;

void calcula_sha256(const char *filename, char *output) {
    printf("Calculando SHA256 para o arquivo: %s\n", filename);
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Erro ao abrir arquivo para SHA");
        strcpy(output, "ERRO");
        return;
    }

    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    unsigned char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        SHA256_Update(&sha256, buffer, bytes);
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);
    fclose(file);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = '\0';
}

void *handle_client(void *arg) {
    client_info_t *client = (client_info_t *)arg;
    int client_sock = client->sock;
    struct sockaddr_in addr = client->addr;
    free(client);

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip_str, sizeof(ip_str));
    int port = ntohs(addr.sin_port);

    char buffer[BUFFER_SIZE];
    ssize_t bytes;
    while ((bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes] = '\0';

        if (strcmp(buffer, "FIN") == 0) {
            printf("[%s:%d] Cliente desconectou.\n", ip_str, port);
            close(client_sock);
            return NULL;
        }

        if (strncmp(buffer, "CHAT ", 5) == 0) {
            char *msg = buffer + 5;
            printf("[%s:%d] Cliente: %s\n", ip_str, port, msg);
            printf("Envie uma mensagem de resposta:\n");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = 0;
            char resposta[BUFFER_SIZE];
            snprintf(resposta, sizeof(resposta), "%.*s\n", BUFFER_SIZE - 2, buffer);
            send(client_sock, resposta, strlen(resposta), 0);
            printf("Servidor: %s\n", buffer);
            continue;
        }

        if (strncmp(buffer, "GET ", 4) == 0) {
            char *filename = buffer + 4;
            printf("[%s:%d] Requisição recebida: %s\n", ip_str, port, filename);
            char hash[HASH_SIZE];
            calcula_sha256(filename, hash);
            FILE *file = fopen(filename, "rb");
            if (!file) {
                send(client_sock, "ERROR: Arquivo não encontrado\n", 31, 0);
                continue;
            }

            send(client_sock, "OK\n", 3, 0);

            fseek(file, 0, SEEK_END);
            long filesize = ftell(file);
            fseek(file, 0, SEEK_SET);

            sprintf(buffer, "SIZE %ld\n", filesize);
            send(client_sock, buffer, strlen(buffer), 0);

            while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                send(client_sock, buffer, bytes, 0);
            }
            fclose(file);

            sprintf(buffer, "HASH %s\n", hash);
            send(client_sock, buffer, strlen(buffer), 0);
            printf("[%s:%d] Enviando arquivo e hash\n", ip_str, port);
        } else {
            printf("[%s:%d] Comando desconhecido: %s\n", ip_str, port, buffer);
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
    int sockfd;
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
        int client_sock = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
        if (client_sock < 0) {
            perror("Erro no accept");
            continue;
        }

        client_info_t *info = malloc(sizeof(client_info_t));
        info->sock = client_sock;
        info->addr = cli_addr;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, info);
        pthread_detach(tid);
    }

    close(sockfd);
    return 0;
}