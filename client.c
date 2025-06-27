// ===================== CLIENTE TCP =====================
/*
 * Autor: Ricardo Marthus Gremmelmaier
 * Data: 2025-06-27
 * Versão: 1.0
*/

/*
 * Criação de um cliente TCP que se conecta a um servidor em uma porta específica e solicita arquivos
 * O cliente deve ser capaz de enviar mensagens ao servidor, processar as respostas e salvar os arquivos recebidos.
 * O protocolo de aplicação será semelhante ao protocolo HTTP, onde o cliente solicita um arquivo e o servidor responde com o conteúdo do arquivo (Get filename.ext).
 * Caso o arquivo não exista, o cliente deve exibir uma mensagem de erro apropriada.
 * O cliente deve ser capaz de lidar com mensagens de controle, como "sair" ou "desconectar", e encerrar a conexão com o servidor.
 * O cliente deve ser capaz de abrir um chat simples com o servidor, onde o cliente pode enviar mensagens e o servidor responde.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <openssl/sha.h>

#define BUFFER_SIZE 1024
#define HASH_SIZE 65

void calcula_sha256(const char *filename, char *output) {
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

ssize_t recv_line(int sock, char *buffer, size_t maxlen) {
    size_t i = 0;
    char c;
    while (i < maxlen - 1) {
        ssize_t n = recv(sock, &c, 1, 0);
        if (n <= 0) return n;
        if (c == '\n') break;
        buffer[i++] = c;
    }
    buffer[i] = '\0';
    return i;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <IP> <Porta>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Erro ao conectar");
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("Digite uma requisição GET [filename.ext] ou CHAT [mensagem] ou 'FIN' para encerrar: ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "FIN") == 0) {
            send(sockfd, buffer, strlen(buffer), 0);
            printf("Conexão encerrada pelo cliente.\n");
            break;
        }
        
        if (strncmp(buffer, "CHAT ", 5) == 0) {
            send(sockfd, buffer, strlen(buffer), 0);
            printf("Cliente: %s\n", buffer + 5);
            recv_line(sockfd, buffer, BUFFER_SIZE);
            printf("Servidor: %s\n", buffer);
            continue;
        }

        if (strncmp(buffer, "GET ", 4) != 0) {
            printf("Formato inválido. Use: GET filename.ext\n");
            continue;
        }

        char *filename = buffer + 4;
        char output_filename[256];
        snprintf(output_filename, sizeof(output_filename), "recebido_%s", filename);
        FILE *output = fopen(output_filename, "wb");
        if (!output) {
            perror("Erro ao criar arquivo de saída");
            continue;
        }

        send(sockfd, buffer, strlen(buffer), 0);

        recv_line(sockfd, buffer, BUFFER_SIZE);
        if (strncmp(buffer, "ERROR", 5) == 0) {
            printf("Erro do servidor: %s\n", buffer);
            fclose(output);
            remove(output_filename);
            continue;
        }

        recv_line(sockfd, buffer, BUFFER_SIZE);
        if (strncmp(buffer, "SIZE ", 5) != 0) {
            printf("Resposta inválida do servidor: %s\n", buffer);
            fclose(output);
            remove(output_filename);
            continue;
        }

        long filesize = atol(buffer + 5);
        long received = 0;
        char file_buffer[BUFFER_SIZE];
        while (received < filesize) {
            ssize_t bytes = recv(sockfd, file_buffer, BUFFER_SIZE, 0);
            if (bytes <= 0) break;

            size_t to_write = (received + bytes <= filesize) ? bytes : (filesize - received);
            fwrite(file_buffer, 1, to_write, output);
            received += to_write;

            if (received == filesize && bytes > to_write) { 
                size_t sobra = bytes - to_write;
                memmove(buffer, file_buffer + to_write, sobra);
                buffer[sobra] = '\0';
            } else {
                buffer[0] = '\0';
            }
        }

        fclose(output);
        printf("Arquivo salvo como '%s'.\n", output_filename);
        
        if (strncmp(buffer, "HASH ", 5) != 0) {
        recv_line(sockfd, buffer, BUFFER_SIZE);
        }
        if (strncmp(buffer, "HASH ", 5) != 0) {
            printf("Hash inválido recebido: %s\n", buffer);
            continue;
        }

        char *hash_servidor = buffer + 5;
        hash_servidor[strcspn(hash_servidor, "\r\n")] = '\0';
        char hash_local[HASH_SIZE];
        calcula_sha256(output_filename, hash_local);

        printf("Hash recebido: %s\n", hash_servidor);
        printf("Hash local   : %s\n", hash_local);

        if (strcmp(hash_servidor, hash_local) == 0) {
            printf("Integridade verificada com sucesso.\n");
        } else {
            printf("Arquivo corrompido!\n");
            remove(output_filename);
            printf("Arquivo '%s' removido.\n", output_filename);
        }
    }
    close(sockfd);
    return 0;
}