#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

typedef enum { LIVRE, OCUPADO, AGUARDANDO_MAQUEIRO } EstadoConsultorio;

typedef struct {
    int idPaciente;
    char* cor;
    int prioridade;
    long tempo_chegada;
    long prioridadeFila;
} Paciente;

#define MAX_FILA 100
Paciente fila_triagem[MAX_FILA];
int qtd_triagem = 0;

int fila_saida[2];
int qtd_saida = 0;

EstadoConsultorio c1 = LIVRE;
EstadoConsultorio c2 = LIVRE;

int leitos_ocupados = 0;
#define MAX_LEITOS 2

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_maqueiro = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_c1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_c2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_recuperacao = PTHREAD_COND_INITIALIZER;

int id_global = 1;

long get_time() {
    return (long)time(NULL) % 100000;
}

void* triagem(void* arg) {
    while(1) {
        usleep((rand() % 3 + 1) * 1000000);

        pthread_mutex_lock(&mutex);
        if (qtd_triagem < MAX_FILA) {
            Paciente p;
            p.idPaciente = id_global++;

            double sorteio = (double)rand() / RAND_MAX;
            if (sorteio < 0.3) {
                p.cor = "Vermelho";
                p.prioridade = 1;
            } else if (sorteio < 0.6) {
                p.cor = "Laranja";
                p.prioridade = 2;
            } else {
                p.cor = "Azul";
                p.prioridade = 3;
            }

            p.tempo_chegada = get_time();
            p.prioridadeFila = p.prioridade * 100000 + p.tempo_chegada;

            int i = qtd_triagem - 1;
            while (i >= 0 && fila_triagem[i].prioridadeFila > p.prioridadeFila) {
                fila_triagem[i+1] = fila_triagem[i];
                i--;
            }
            fila_triagem[i+1] = p;
            qtd_triagem++;

            printf("[Triagem] Novo Paciente %d | %s | PrioridadeFila: %ld\n", p.idPaciente, p.cor, p.prioridadeFila);
            pthread_cond_signal(&cond_maqueiro);
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void* maqueiro(void* arg) {
    while(1) {
        pthread_mutex_lock(&mutex);

        while (qtd_saida == 0 && (qtd_triagem == 0 || (c1 != LIVRE && c2 != LIVRE))) {
            pthread_cond_wait(&cond_maqueiro, &mutex);
        }

        if (qtd_saida > 0) {
            while (leitos_ocupados == MAX_LEITOS) {
                printf("[Maqueiro] Recuperacao CHEIA. Aguardando liberar leito...\n");
                pthread_cond_wait(&cond_recuperacao, &mutex);
            }

            int id_c = fila_saida[0];
            fila_saida[0] = fila_saida[1];
            qtd_saida--;

            printf("[Maqueiro] Transportando paciente do Consultorio %d para Recuperacao...\n", id_c);
            pthread_mutex_unlock(&mutex);
            usleep(1000000);
            pthread_mutex_lock(&mutex);

            leitos_ocupados++;
            printf("[Maqueiro] Paciente acomodado na Recuperacao. Leitos: %d/%d\n", leitos_ocupados, MAX_LEITOS);

            if (id_c == 1) {
                c1 = LIVRE;
                pthread_cond_signal(&cond_c1);
            } else {
                c2 = LIVRE;
                pthread_cond_signal(&cond_c2);
            }

            pthread_cond_signal(&cond_maqueiro);
        }
        else if (qtd_triagem > 0 && (c1 == LIVRE || c2 == LIVRE)) {
            Paciente p = fila_triagem[0];
            for (int i = 0; i < qtd_triagem - 1; i++) {
                fila_triagem[i] = fila_triagem[i+1];
            }
            qtd_triagem--;

            int id_c;
            if (c1 == LIVRE && c2 == LIVRE) {
                id_c = (rand() % 2 == 0) ? 1 : 2;
            } else if (c1 == LIVRE) {
                id_c = 1;
            } else {
                id_c = 2;
            }

            if (id_c == 1) c1 = OCUPADO;
            else c2 = OCUPADO;

            printf("[Maqueiro] Levando Paciente %d (%s) da Triagem para Consultorio %d...\n", p.idPaciente, p.cor, id_c);
            pthread_mutex_unlock(&mutex);
            usleep(1000000);
            pthread_mutex_lock(&mutex);

            if (id_c == 1) pthread_cond_signal(&cond_c1);
            else pthread_cond_signal(&cond_c2);
        }

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void* consultorio(void* arg) {
    int id = *((int*)arg);
    while(1) {
        pthread_mutex_lock(&mutex);

        while ((id == 1 && c1 != OCUPADO) || (id == 2 && c2 != OCUPADO)) {
            pthread_cond_wait((id == 1) ? &cond_c1 : &cond_c2, &mutex);
        }

        printf("[Consultorio %d] Atendimento iniciado.\n", id);
        pthread_mutex_unlock(&mutex);

        usleep((rand() % 3 + 2) * 1000000);

        pthread_mutex_lock(&mutex);
        if (id == 1) c1 = AGUARDANDO_MAQUEIRO;
        else c2 = AGUARDANDO_MAQUEIRO;

        fila_saida[qtd_saida++] = id;
        printf("[Consultorio %d] Atendimento concluido. Aguardando maqueiro.\n", id);
        pthread_cond_signal(&cond_maqueiro);

        while ((id == 1 && c1 == AGUARDANDO_MAQUEIRO) || (id == 2 && c2 == AGUARDANDO_MAQUEIRO)) {
            pthread_cond_wait((id == 1) ? &cond_c1 : &cond_c2, &mutex);
        }

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void* alta_pacientes(void* arg) {
    while(1) {
        usleep((rand() % 4 + 3) * 1000000);

        pthread_mutex_lock(&mutex);
        if (leitos_ocupados > 0) {
            leitos_ocupados--;
            printf("[Recuperacao] Paciente recebeu ALTA. Leitos: %d/%d\n", leitos_ocupados, MAX_LEITOS);
            pthread_cond_signal(&cond_recuperacao);
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main() {
    srand((unsigned int)time(NULL));
    pthread_t t_triagem, t_m, t_c1, t_c2, t_alta;
    int id1 = 1, id2 = 2;

    printf("=== Sistema de Hospital Automatizado (AnyLogic -> C) ===\n");
    printf("Ctrl+C para encerrar.\n\n");

    pthread_create(&t_triagem, NULL, triagem, NULL);
    pthread_create(&t_m, NULL, maqueiro, NULL);
    pthread_create(&t_c1, NULL, consultorio, &id1);
    pthread_create(&t_c2, NULL, consultorio, &id2);
    pthread_create(&t_alta, NULL, alta_pacientes, NULL);

    pthread_join(t_triagem, NULL);

    return 0;
}
