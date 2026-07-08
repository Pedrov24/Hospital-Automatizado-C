#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <ncurses.h>

// ==================== ESTRUTURAS DE DADOS ====================

typedef enum { LIVRE, OCUPADO, AGUARDANDO_MAQUEIRO } EstadoConsultorio;

typedef struct {
    int idPaciente;
    char cor[16];
    int prioridade;
    long tempo_chegada;
    long prioridadeFila;
} Paciente;

#define MAX_FILA 100
Paciente fila_triagem[MAX_FILA];
int qtd_triagem = 0;

int fila_saida[2];
int qtd_saida = 0;

EstadoConsultorio c1 = LIVRE, c2 = LIVRE;
int leitos_ocupados = 0;
#define MAX_LEITOS 2

int paciente_c1 = -1, paciente_c2 = -1;
char cor_c1[16] = "", cor_c2[16] = "";
int leito_paciente[2] = {-1, -1};
char leito_cor[2][16] = {"", ""};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_maqueiro = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_c1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_c2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_recuperacao = PTHREAD_COND_INITIALIZER;

int id_global = 1;
int total_atendidos = 0;
pid_t dashboard_pid = -1;

void encerrar_dashboard() {
    if (dashboard_pid > 0) {
        kill(dashboard_pid, SIGTERM);
        dashboard_pid = -1;
    }
}

void tratar_saida(int sinal) {
    encerrar_dashboard();
    endwin();
    _exit(128 + sinal);
}

void iniciar_dashboard() {
    dashboard_pid = fork();

    if (dashboard_pid == 0) {
        execlp("python3", "python3", "servidor_dashboard.py", NULL);
        execlp("python", "python", "servidor_dashboard.py", NULL);
        _exit(1);
    }
}

// ==================== SISTEMA DE LOGS ====================

#define MAX_LOGS 100
typedef struct { char msg[120]; } LogEntry;
LogEntry log_buffer[MAX_LOGS];
int log_head = 0, log_count = 0;

void log_event(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_buffer[log_head].msg, sizeof(log_buffer[log_head].msg), fmt, args);
    va_end(args);
    log_head = (log_head + 1) % MAX_LOGS;
    if (log_count < MAX_LOGS) log_count++;
}

// ==================== SIMULAÇÃO (THREADS) ====================

long get_time() { return (long)time(NULL) % 100000; }

void* triagem(void* arg) {
    while (1) {
        usleep((rand() % 3 + 1) * 1000000);
        pthread_mutex_lock(&mutex);
        if (qtd_triagem < MAX_FILA) {
            Paciente p;
            p.idPaciente = id_global++;
            double sorteio = (double)rand() / RAND_MAX;
            if (sorteio < 0.3) {
                strcpy(p.cor, "Vermelho"); p.prioridade = 1;
            } else if (sorteio < 0.6) {
                strcpy(p.cor, "Laranja"); p.prioridade = 2;
            } else {
                strcpy(p.cor, "Azul"); p.prioridade = 3;
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
            log_event("Triagem: Paciente #%d (%s) chegou", p.idPaciente, p.cor);
            pthread_cond_signal(&cond_maqueiro);
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void* maqueiro(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        while (qtd_saida == 0 && (qtd_triagem == 0 || (c1 != LIVRE && c2 != LIVRE)))
            pthread_cond_wait(&cond_maqueiro, &mutex);

        if (qtd_saida > 0) {
            while (leitos_ocupados == MAX_LEITOS) {
                log_event("Maqueiro: Recuperacao CHEIA, aguardando...");
                pthread_cond_wait(&cond_recuperacao, &mutex);
            }
            int id_c = fila_saida[0];
            fila_saida[0] = fila_saida[1];
            qtd_saida--;
            log_event("Maqueiro: Transportando C%d -> Recuperacao", id_c);
            pthread_mutex_unlock(&mutex);
            usleep(1000000);
            pthread_mutex_lock(&mutex);
            leitos_ocupados++;
            int vaga = (leito_paciente[0] == -1) ? 0 : 1;
            leito_paciente[vaga] = (id_c == 1) ? paciente_c1 : paciente_c2;
            strcpy(leito_cor[vaga], (id_c == 1) ? cor_c1 : cor_c2);
            log_event("Maqueiro: Paciente acomodado no Leito %d", vaga + 1);

            if (id_c == 1) {
                paciente_c1 = -1; cor_c1[0] = '\0';
                c1 = LIVRE;
                pthread_cond_signal(&cond_c1);
            } else {
                paciente_c2 = -1; cor_c2[0] = '\0';
                c2 = LIVRE;
                pthread_cond_signal(&cond_c2);
            }
            pthread_cond_signal(&cond_maqueiro);
        } else if (qtd_triagem > 0 && (c1 == LIVRE || c2 == LIVRE)) {
            Paciente p = fila_triagem[0];
            for (int i = 0; i < qtd_triagem - 1; i++)
                fila_triagem[i] = fila_triagem[i+1];
            qtd_triagem--;

            int id_c;
            if (c1 == LIVRE && c2 == LIVRE) id_c = (rand() % 2 == 0) ? 1 : 2;
            else if (c1 == LIVRE) id_c = 1;
            else id_c = 2;

            if (id_c == 1) {
                c1 = OCUPADO;
                paciente_c1 = p.idPaciente;
                strcpy(cor_c1, p.cor);
            } else {
                c2 = OCUPADO;
                paciente_c2 = p.idPaciente;
                strcpy(cor_c2, p.cor);
            }
            log_event("Maqueiro: Paciente #%d (%s) -> Consultorio %d", p.idPaciente, p.cor, id_c);
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
    while (1) {
        pthread_mutex_lock(&mutex);
        while ((id == 1 && c1 != OCUPADO) || (id == 2 && c2 != OCUPADO))
            pthread_cond_wait((id == 1) ? &cond_c1 : &cond_c2, &mutex);
        log_event("Consultorio %d: Atendimento iniciado", id);
        pthread_mutex_unlock(&mutex);
        usleep((rand() % 3 + 2) * 1000000);
        pthread_mutex_lock(&mutex);
        if (id == 1) c1 = AGUARDANDO_MAQUEIRO;
        else c2 = AGUARDANDO_MAQUEIRO;
        fila_saida[qtd_saida++] = id;
        log_event("Consultorio %d: Atendimento concluido, aguardando maqueiro", id);
        pthread_cond_signal(&cond_maqueiro);
        while ((id == 1 && c1 == AGUARDANDO_MAQUEIRO) || (id == 2 && c2 == AGUARDANDO_MAQUEIRO))
            pthread_cond_wait((id == 1) ? &cond_c1 : &cond_c2, &mutex);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void* alta_pacientes(void* arg) {
    while (1) {
        usleep((rand() % 4 + 3) * 1000000);
        pthread_mutex_lock(&mutex);
        if (leitos_ocupados > 0) {
            int idx = -1;
            for (int i = 0; i < MAX_LEITOS; i++) {
                if (leito_paciente[i] != -1) { idx = i; break; }
            }
            if (idx != -1) {
                log_event("Recuperacao: Paciente #%d recebeu ALTA", leito_paciente[idx]);
                leito_paciente[idx] = -1;
                leito_cor[idx][0] = '\0';
                leitos_ocupados--;
                total_atendidos++;
                pthread_cond_signal(&cond_recuperacao);
            }
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// ==================== INTERFACE NCURSES ====================

int cor_prioridade(int prio) {
    if (prio == 1) return 1;
    if (prio == 2) return 2;
    return 3;
}

void desenhar_triagem(WINDOW* win) {
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " TRIAGEM ");
    int h, w;
    getmaxyx(win, h, w);
    pthread_mutex_lock(&mutex);
    mvwprintw(win, 1, 2, "Fila: %d", qtd_triagem);
    int y = 2;
    int max_show = h - 3;
    int start = (qtd_triagem > max_show) ? qtd_triagem - max_show : 0;
    for (int i = start; i < qtd_triagem && y < h - 1; i++, y++) {
        int cp = cor_prioridade(fila_triagem[i].prioridade);
        wattron(win, COLOR_PAIR(cp));
        mvwprintw(win, y, 2, "#%d %s", fila_triagem[i].idPaciente, fila_triagem[i].cor);
        wattroff(win, COLOR_PAIR(cp));
    }
    pthread_mutex_unlock(&mutex);
}

void desenhar_maqueiro(WINDOW* win) {
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " MAQUEIRO ");
    int h, w;
    getmaxyx(win, h, w);
    pthread_mutex_lock(&mutex);
    int ocupado = (qtd_saida > 0 || (qtd_triagem > 0 && (c1 == LIVRE || c2 == LIVRE)));
    if (ocupado) {
        wattron(win, COLOR_PAIR(1));
        mvwprintw(win, 1, 2, "OCUPADO");
        wattroff(win, COLOR_PAIR(1));
        if (qtd_saida > 0) {
            mvwprintw(win, 2, 2, "Transportando C%d p/ Recup", fila_saida[0]);
        } else {
            mvwprintw(win, 2, 2, "Levando p/ Consultorio");
        }
    } else {
        wattron(win, COLOR_PAIR(4));
        mvwprintw(win, 1, 2, "LIVRE");
        wattroff(win, COLOR_PAIR(4));
        mvwprintw(win, 2, 2, "Aguardando tarefas...");
    }
    pthread_mutex_unlock(&mutex);
}

void desenhar_consultorio(WINDOW* win, int id) {
    box(win, 0, 0);
    int h, w;
    getmaxyx(win, h, w);
    pthread_mutex_lock(&mutex);
    EstadoConsultorio estado = (id == 1) ? c1 : c2;
    int p_id = (id == 1) ? paciente_c1 : paciente_c2;
    char* p_cor = (id == 1) ? cor_c1 : cor_c2;
    char label[32];
    snprintf(label, sizeof(label), " CONSULTORIO %d ", id);
    mvwprintw(win, 0, 2, "%s", label);

    if (estado == LIVRE) {
        wattron(win, COLOR_PAIR(4));
        mvwprintw(win, 1, 2, "LIVRE");
        wattroff(win, COLOR_PAIR(4));
        mvwprintw(win, 2, 2, "Vazio");
    } else if (estado == OCUPADO) {
        wattron(win, COLOR_PAIR(1));
        mvwprintw(win, 1, 2, "OCUPADO");
        wattroff(win, COLOR_PAIR(1));
        mvwprintw(win, 2, 2, "#%d", p_id);
        int cp = cor_prioridade(
            strcmp(p_cor, "Vermelho") == 0 ? 1 :
            strcmp(p_cor, "Laranja") == 0 ? 2 : 3
        );
        wattron(win, COLOR_PAIR(cp));
        mvwprintw(win, 3, 2, "%s", p_cor);
        wattroff(win, COLOR_PAIR(cp));
    } else {
        wattron(win, COLOR_PAIR(3));
        mvwprintw(win, 1, 2, "AGUARDANDO");
        wattroff(win, COLOR_PAIR(3));
        mvwprintw(win, 2, 2, "Maqueeiro...");
    }
    pthread_mutex_unlock(&mutex);
}

void desenhar_recuperacao(WINDOW* win) {
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " RECUPERACAO ");
    int h, w;
    getmaxyx(win, h, w);
    pthread_mutex_lock(&mutex);
    mvwprintw(win, 1, 2, "Leitos: %d/%d", leitos_ocupados, MAX_LEITOS);
    for (int i = 0; i < MAX_LEITOS; i++) {
        mvwprintw(win, 2 + i * 2, 2, "Leito %d:", i + 1);
        if (leito_paciente[i] != -1) {
            wattron(win, A_BOLD);
            int cp = cor_prioridade(
                strcmp(leito_cor[i], "Vermelho") == 0 ? 1 :
                strcmp(leito_cor[i], "Laranja") == 0 ? 2 : 3
            );
            wattron(win, COLOR_PAIR(cp));
            mvwprintw(win, 2 + i * 2, 14, "#%d %s", leito_paciente[i], leito_cor[i]);
            wattroff(win, COLOR_PAIR(cp));
            wattroff(win, A_BOLD);
        } else {
            mvwprintw(win, 2 + i * 2, 14, "Livre");
        }
    }
    pthread_mutex_unlock(&mutex);
}

void desenhar_logs(WINDOW* win) {
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " LOGS ");
    int h, w;
    getmaxyx(win, h, w);
    int start = log_count < h - 2 ? 0 : log_count - (h - 2);
    for (int i = 0; i < h - 2 && i < log_count; i++) {
        int idx = (log_head - log_count + start + i) % MAX_LOGS;
        mvwprintw(win, i + 1, 2, "%.*s", w - 4, log_buffer[idx].msg);
    }
}

int main() {
    srand((unsigned int)time(NULL));
    pthread_t t_triagem, t_m, t_c1, t_c2, t_alta;
    int id1 = 1, id2 = 2;
    signal(SIGINT, tratar_saida);
    signal(SIGTERM, tratar_saida);
    atexit(encerrar_dashboard);

    // Inicia ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);
    start_color();

    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_CYAN, COLOR_BLACK);
    init_pair(4, COLOR_GREEN, COLOR_BLACK);
    iniciar_dashboard();
    log_event("Dashboard iniciado em http://127.0.0.1:8000");

    // Cria threads
    pthread_create(&t_triagem, NULL, triagem, NULL);
    pthread_create(&t_m, NULL, maqueiro, NULL);
    pthread_create(&t_c1, NULL, consultorio, &id1);
    pthread_create(&t_c2, NULL, consultorio, &id2);
    pthread_create(&t_alta, NULL, alta_pacientes, NULL);

    // Loop principal da UI
    while (1) {
        int term_h, term_w;
        getmaxyx(stdscr, term_h, term_w);

        int p1_w = term_w * 0.22;
        int p2_w = term_w * 0.38;
        int p3_w = term_w * 0.38;

        WINDOW* w_triagem = newwin(term_h - 8, p1_w, 0, 0);
        WINDOW* w_maqueiro = newwin(5, p2_w, 0, p1_w);
        int c_w = (p2_w - 1) / 2;
        WINDOW* w_c1 = newwin(term_h - 15, c_w, 5, p1_w);
        WINDOW* w_c2 = newwin(term_h - 15, p2_w - c_w - 1, 5, p1_w + c_w + 1);
        WINDOW* w_recup = newwin(term_h - 8, p3_w - 1, 0, p1_w + p2_w);
        WINDOW* w_logs = newwin(6, term_w, term_h - 6, 0);

        // Header
        wattron(stdscr, A_BOLD);
        mvprintw(0, 2, "HOSPITAL AUTOMATIZADO - Simulacao em Tempo Real");
        wattroff(stdscr, A_BOLD);
        mvprintw(1, 2, "Atendimentos: %d", total_atendidos);
        mvprintw(1, term_w - 30, "Pressione 'q' para sair");

        desenhar_triagem(w_triagem);
        desenhar_maqueiro(w_maqueiro);
        desenhar_consultorio(w_c1, 1);
        desenhar_consultorio(w_c2, 2);
        desenhar_recuperacao(w_recup);
        desenhar_logs(w_logs);

        wrefresh(w_triagem);
        wrefresh(w_maqueiro);
        wrefresh(w_c1);
        wrefresh(w_c2);
        wrefresh(w_recup);
        wrefresh(w_logs);

        // Verifica tecla 'q'
        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        usleep(150000);

        delwin(w_triagem);
        delwin(w_maqueiro);
        delwin(w_c1);
        delwin(w_c2);
        delwin(w_recup);
        delwin(w_logs);
    }

    endwin();
    encerrar_dashboard();
    printf("Simulacao encerrada. Total de atendimentos: %d\n", total_atendidos);
    return 0;
}
