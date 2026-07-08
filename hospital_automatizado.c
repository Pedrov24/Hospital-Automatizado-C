#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Estruturas de Dados do Problema
typedef enum { LIVRE, OCUPADO, AGUARDANDO_MAQUEIRO } EstadoConsultorio;

typedef struct {
    int idPaciente;
    char* cor;
    int prioridade; // 1 (Vermelho), 2 (Laranja), 3 (Azul)
    long tempo_chegada;
    long prioridadeFila; // prioridade * 100000 + tempo
} Paciente;

// Filas
#define MAX_FILA 100
Paciente fila_triagem[MAX_FILA];
int qtd_triagem = 0;

int fila_saida[2]; // Para pacientes saindo do consultório para recuperação
int qtd_saida = 0;

// Variáveis de Estado
EstadoConsultorio c1 = LIVRE;
EstadoConsultorio c2 = LIVRE;

int leitos_ocupados = 0;
#define MAX_LEITOS 2

// Sincronização (Windows API)
CRITICAL_SECTION mutex;
CONDITION_VARIABLE cond_maqueiro;
CONDITION_VARIABLE cond_c1;
CONDITION_VARIABLE cond_c2;
CONDITION_VARIABLE cond_recuperacao;

int id_global = 1;
PROCESS_INFORMATION dashboard_process;
int dashboard_iniciado = 0;

void encerrar_dashboard() {
    if (dashboard_iniciado) {
        TerminateProcess(dashboard_process.hProcess, 0);
        CloseHandle(dashboard_process.hThread);
        CloseHandle(dashboard_process.hProcess);
        dashboard_iniciado = 0;
    }
}

BOOL WINAPI tratar_saida(DWORD tipo) {
    if (tipo == CTRL_C_EVENT || tipo == CTRL_CLOSE_EVENT || tipo == CTRL_BREAK_EVENT) {
        encerrar_dashboard();
    }
    return FALSE;
}

int iniciar_dashboard_comando(const char* comando) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char linha_comando[256];

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    snprintf(linha_comando, sizeof(linha_comando), "%s", comando);

    if (!CreateProcessA(
            NULL,
            linha_comando,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi)) {
        return 0;
    }

    dashboard_process = pi;
    dashboard_iniciado = 1;
    return 1;
}

void iniciar_dashboard() {
    if (iniciar_dashboard_comando("python servidor_dashboard.py")) {
        printf("[Dashboard] Servidor iniciado. O navegador sera aberto automaticamente.\n");
        return;
    }

    printf("[Dashboard] Nao foi possivel iniciar automaticamente. Execute: python servidor_dashboard.py\n");
}

// Retorna tempo em segundos (simulado)
long get_time() {
    return (long)time(NULL) % 100000;
}

// Thread 1: Triagem (Geração de Pacientes)
DWORD WINAPI triagem(LPVOID arg) {
    while(1) {
        Sleep((rand() % 3 + 1) * 1000); // Chega paciente a cada 1-3s
        
        EnterCriticalSection(&mutex);
        if (qtd_triagem < MAX_FILA) {
            Paciente p;
            p.idPaciente = id_global++;
            
            // Classificação de risco probabilística (30% Vermelho, 30% Laranja, 40% Azul)
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
            // Lógica do README: menor valor = maior prioridade
            p.prioridadeFila = p.prioridade * 100000 + p.tempo_chegada;
            
            // Inserção ordenada na fila da triagem (baseado na prioridadeFila)
            int i = qtd_triagem - 1;
            while (i >= 0 && fila_triagem[i].prioridadeFila > p.prioridadeFila) {
                fila_triagem[i+1] = fila_triagem[i];
                i--;
            }
            fila_triagem[i+1] = p;
            qtd_triagem++;
            
            printf("[Triagem] Novo Paciente %d | %s | PrioridadeFila: %ld\n", p.idPaciente, p.cor, p.prioridadeFila);
            WakeConditionVariable(&cond_maqueiro);
        }
        LeaveCriticalSection(&mutex);
    }
    return 0;
}

// Thread 2: Maqueiro
DWORD WINAPI maqueiro(LPVOID arg) {
    while(1) {
        EnterCriticalSection(&mutex);
        
        // Aguarda ter algo para fazer
        // Tarefas: 1) Levar da saída pro leito OU 2) Levar da triagem pro consultorio (se houver consultorio livre)
        while (qtd_saida == 0 && (qtd_triagem == 0 || (c1 != LIVRE && c2 != LIVRE))) {
            SleepConditionVariableCS(&cond_maqueiro, &mutex, INFINITE);
        }
        
        // Prioridade do Maqueiro: Esvaziar os consultórios primeiro para evitar Deadlock
        if (qtd_saida > 0) {
            // Verifica leitos na recuperação
            while (leitos_ocupados == MAX_LEITOS) {
                printf("[Maqueiro] Recuperacao CHEIA. Aguardando liberar leito...\n");
                SleepConditionVariableCS(&cond_recuperacao, &mutex, INFINITE);
            }
            
            int id_c = fila_saida[0];
            fila_saida[0] = fila_saida[1];
            qtd_saida--;
            
            printf("[Maqueiro] Transportando paciente do Consultorio %d para Recuperacao...\n", id_c);
            LeaveCriticalSection(&mutex);
            Sleep(1000); // Simula transporte
            EnterCriticalSection(&mutex);
            
            leitos_ocupados++;
            printf("[Maqueiro] Paciente acomodado na Recuperacao. Leitos: %d/%d\n", leitos_ocupados, MAX_LEITOS);
            
            // Libera consultorio
            if (id_c == 1) {
                c1 = LIVRE;
                WakeConditionVariable(&cond_c1);
            } else {
                c2 = LIVRE;
                WakeConditionVariable(&cond_c2);
            }
            
            // Acorda a si mesmo caso haja mais tarefas
            WakeConditionVariable(&cond_maqueiro);
        }
        // Segunda tarefa: Levar da triagem para o consultorio
        else if (qtd_triagem > 0 && (c1 == LIVRE || c2 == LIVRE)) {
            Paciente p = fila_triagem[0];
            for (int i = 0; i < qtd_triagem - 1; i++) {
                fila_triagem[i] = fila_triagem[i+1];
            }
            qtd_triagem--;
            
            // Lógica do README para escolha de consultório
            int id_c;
            if (c1 == LIVRE && c2 == LIVRE) {
                id_c = (rand() % 2 == 0) ? 1 : 2; // randomTrue(0.5)
            } else if (c1 == LIVRE) {
                id_c = 1;
            } else {
                id_c = 2;
            }
            
            if (id_c == 1) c1 = OCUPADO;
            else c2 = OCUPADO;
            
            printf("[Maqueiro] Levando Paciente %d (%s) da Triagem para Consultorio %d...\n", p.idPaciente, p.cor, id_c);
            LeaveCriticalSection(&mutex);
            Sleep(1000); // Simula transporte
            EnterCriticalSection(&mutex);
            
            // Inicia atendimento
            if (id_c == 1) WakeConditionVariable(&cond_c1);
            else WakeConditionVariable(&cond_c2);
        }
        
        LeaveCriticalSection(&mutex);
    }
    return 0;
}

// Thread 3: Consultório
DWORD WINAPI consultorio(LPVOID arg) {
    int id = *((int*)arg);
    while(1) {
        EnterCriticalSection(&mutex);
        
        // Aguarda receber paciente do maqueiro
        while ((id == 1 && c1 != OCUPADO) || (id == 2 && c2 != OCUPADO)) {
            SleepConditionVariableCS((id == 1) ? &cond_c1 : &cond_c2, &mutex, INFINITE);
        }
        
        printf("[Consultorio %d] Atendimento iniciado.\n", id);
        LeaveCriticalSection(&mutex);
        
        Sleep((rand() % 3 + 2) * 1000); // Tempo de atendimento
        
        EnterCriticalSection(&mutex);
        if (id == 1) c1 = AGUARDANDO_MAQUEIRO;
        else c2 = AGUARDANDO_MAQUEIRO;
        
        fila_saida[qtd_saida++] = id;
        printf("[Consultorio %d] Atendimento concluido. Aguardando maqueiro.\n", id);
        WakeConditionVariable(&cond_maqueiro); // Chama o maqueiro
        
        // Aguarda o maqueiro remover o paciente (Estado volta para LIVRE)
        while ((id == 1 && c1 == AGUARDANDO_MAQUEIRO) || (id == 2 && c2 == AGUARDANDO_MAQUEIRO)) {
            SleepConditionVariableCS((id == 1) ? &cond_c1 : &cond_c2, &mutex, INFINITE);
        }
        
        LeaveCriticalSection(&mutex);
    }
    return 0;
}

// Thread 4: Alta (Liberando leitos)
DWORD WINAPI alta_pacientes(LPVOID arg) {
    while(1) {
        Sleep((rand() % 4 + 3) * 1000);
        
        EnterCriticalSection(&mutex);
        if (leitos_ocupados > 0) {
            leitos_ocupados--;
            printf("[Recuperacao] Paciente recebeu ALTA. Leitos: %d/%d\n", leitos_ocupados, MAX_LEITOS);
            WakeConditionVariable(&cond_recuperacao); // Avisa o maqueiro que tem vaga
        }
        LeaveCriticalSection(&mutex);
    }
    return 0;
}

int main() {
    srand((unsigned int)time(NULL));
    HANDLE t_triagem, t_m, t_c1, t_c2, t_alta;
    int id1 = 1, id2 = 2;
    SetConsoleCtrlHandler(tratar_saida, TRUE);
    
    InitializeCriticalSection(&mutex);
    InitializeConditionVariable(&cond_maqueiro);
    InitializeConditionVariable(&cond_c1);
    InitializeConditionVariable(&cond_c2);
    InitializeConditionVariable(&cond_recuperacao);
    
    printf("=== Sistema de Hospital Automatizado (AnyLogic -> C) ===\n");
    printf("Ctrl+C para encerrar.\n\n");
    iniciar_dashboard();
    
    t_triagem = CreateThread(NULL, 0, triagem, NULL, 0, NULL);
    t_m = CreateThread(NULL, 0, maqueiro, NULL, 0, NULL);
    t_c1 = CreateThread(NULL, 0, consultorio, &id1, 0, NULL);
    t_c2 = CreateThread(NULL, 0, consultorio, &id2, 0, NULL);
    t_alta = CreateThread(NULL, 0, alta_pacientes, NULL, 0, NULL);
    
    WaitForSingleObject(t_triagem, INFINITE);
    
    DeleteCriticalSection(&mutex);
    return 0;
}
