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

#define BUFFER_SIZE 1024

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
        printf("Digite uma requisição GET filename.ext (ou 'FIN' para encerrar): ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "FIN") == 0) {
            send(sockfd, buffer, strlen(buffer), 0);
            printf("Conexão encerrada pelo cliente.\n");
            break;
        }

        if (strncmp(buffer, "GET ", 4) != 0) {
            printf("Formato inválido. Use: GET filename.ext\n");
            continue;
        }

        char *filename = buffer + 4;
        char output_filename[256];
        snprintf(output_filename, sizeof(output_filename), "%s_recebido", filename);
        FILE *output = fopen(output_filename, "wb");
        if (!output) {
            perror("Erro ao criar arquivo de saída");
            continue;
        }

        send(sockfd, buffer, strlen(buffer), 0);

        ssize_t bytes;
        while ((bytes = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
            if (strncmp(buffer, "EOF", 3) == 0) break;
            fwrite(buffer, 1, bytes, output);
            if (bytes < BUFFER_SIZE) break;
        }

        fclose(output);
        printf("Arquivo salvo como '%s'.\n", output_filename);
    }

    close(sockfd);
    return 0;
}