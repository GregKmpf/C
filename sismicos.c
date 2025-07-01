#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define TAM_LEITURA 17  // 16 bits + '\0'
#define VERDE "\033[32m"
#define AZUL  "\033[34m"
#define RESET "\033[0m"

int formato_saida, num_leituras, tempo_limite;
pid_t pid_esr = 0, pid_cpg = 0;

void gerar_leitura_binaria(char *leitura) {
    for (int i = 0; i < 16; i++) {
        leitura[i] = (rand() % 2) ? '1' : '0';
    }
    leitura[16] = '\0';
}

int converter_binario_para_decimal(const char *bin) {
    return (int)strtol(bin, NULL, 2);
}

void imprimir_formatado(int valor, int formato) {
    switch (formato) {
        case 8:
            printf("%o", valor);
            break;
        case 10:
            printf("%d", valor);
            break;
        case 16:
            printf("%X", valor);
            break;
        default:
            fprintf(stderr, "Formato inválido!\n");
            exit(EXIT_FAILURE);
    }
}

void tratador_sigint(int signum) {
    printf("\n\033[31mAnálise interrompida pelo pesquisador!\033[0m\n");
    exit(EXIT_FAILURE);
}

void tratador_timeout(int signum) {
    printf("\n\033[31mProcessamento excedeu o tempo limite!\033[0m\n");
    if (pid_esr > 0) kill(pid_esr, SIGKILL);
    if (pid_cpg > 0) kill(pid_cpg, SIGKILL);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <FORMATO_SAIDA> <NUM_LEITURAS> <TEMPO_LIMITE>\n", argv[0]);
        return EXIT_FAILURE;
    }

    formato_saida = atoi(argv[1]);
    num_leituras = atoi(argv[2]);
    tempo_limite = atoi(argv[3]);

    if ((formato_saida != 8 && formato_saida != 10 && formato_saida != 16) || num_leituras <= 0 || tempo_limite <= 0) {
        fprintf(stderr, "Parâmetros inválidos.\n");
        return EXIT_FAILURE;
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("Erro ao criar pipe");
        return EXIT_FAILURE;
    }

    signal(SIGALRM, tratador_timeout);
    alarm(tempo_limite);

    pid_esr = fork();

    if (pid_esr < 0) {
        perror("Erro ao criar processo ESR");
        return EXIT_FAILURE;
    }

    if (pid_esr == 0) {
        // Processo ESR
        close(pipe_fd[0]);
        srand(time(NULL) ^ getpid());

        for (int i = 0; i < num_leituras; i++) {
            char leitura[TAM_LEITURA];
            gerar_leitura_binaria(leitura);

            dprintf(pipe_fd[1], "%s\n", leitura);
            printf(VERDE"[ESR PID:%d] Leitura #%d: %s transmitida.\n"RESET, getpid(), i + 1, leitura);
            fflush(stdout);
            usleep(100000); // 100ms só pra simular envio
        }

        close(pipe_fd[1]);
        exit(EXIT_SUCCESS);
    }

    // Processo pai continua
    pid_cpg = fork();

    if (pid_cpg < 0) {
        perror("Erro ao criar processo CPG");
        return EXIT_FAILURE;
    }

    if (pid_cpg == 0) {
        // Processo CPG
        signal(SIGINT, tratador_sigint);
        close(pipe_fd[1]);
        FILE *fp = fdopen(pipe_fd[0], "r");
        if (!fp) {
            perror("fdopen");
            exit(EXIT_FAILURE);
        }

        char buffer[TAM_LEITURA];
        int count = 0;
        while (fgets(buffer, sizeof(buffer), fp)) {
            buffer[strcspn(buffer, "\n")] = '\0'; // remove \n
            int decimal = converter_binario_para_decimal(buffer);

            printf(AZUL"[CPG PID:%d] Recebido: %s -> Formato %d: ", getpid(), buffer, formato_saida);
            imprimir_formatado(decimal, formato_saida);
            printf("\n"RESET);
            fflush(stdout);
            count++;
        }

        fclose(fp);
        exit(EXIT_SUCCESS);
    }

    // Processo Controlador
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    waitpid(pid_esr, NULL, 0);
    waitpid(pid_cpg, NULL, 0);

    printf("\n\033[32mConclusão Normal.\033[0m\n");
    return EXIT_SUCCESS;
}
