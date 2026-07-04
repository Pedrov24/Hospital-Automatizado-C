# Projeto Hospital Automatizado com Maqueiro Compartilhado

Este repositório contém a implementação do sistema de simulação **Hospital Automatizado**, adaptado a partir do modelo AnyLogic para duas soluções distintas:
1. **Simulação em C (Terminal):** Modelagem rigorosa de concorrência com Threads.
2. **Dashboard Web (Visual):** Uma interface interativa em tempo real (Vanilla JS).

## 1. Simulação em C (`hospital_automatizado.c`)
A implementação em C utiliza a **Windows API** (`<windows.h>`) para gerenciar as *Threads* e a sincronização do ambiente de concorrência, prevenindo problemas clássicos de sistemas operacionais como *Deadlock* e *Starvation*.

### Arquitetura de Threads:
- **Thread Triagem:** Gera pacientes com classificações probabilísticas (Vermelho, Laranja, Azul) e insere na fila de prioridades obedecendo a fórmula `prioridade * 100000 + tempo()`.
- **Thread Maqueiro:** O recurso compartilhado do sistema. Move os pacientes da triagem para os consultórios, e dos consultórios para a recuperação. Sua rotina prioriza liberar consultórios ocupados para evitar bloqueio do sistema.
- **Threads Consultórios (C1 e C2):** Representam os médicos atendendo (captura o paciente, aguarda o tempo estocástico de atendimento, libera).
- **Thread Recuperação:** Libera os pacientes de tempos em tempos (Alta).

### Como Compilar e Rodar:
No seu terminal (ex: MSYS2, MinGW ou Git Bash no Windows):
```bash
gcc hospital_automatizado.c -o hospital.exe
.\hospital.exe
```

---

## 2. Dashboard Web (`hospital-dashboard/`)
Para permitir uma observação visual amigável do sistema sem depender dos logs em console, o projeto conta com uma versão visual rica feita puramente em HTML, CSS (com estética *Glassmorphism* e Dark Mode) e JavaScript.

### Estrutura
- `index.html`: A estrutura com o painel de Triagem, Maqueiro, Consultórios e Recuperação.
- `style.css`: Toda a estilização moderna com micro-animações (ex: o ícone do maqueiro).
- `main.js`: Implementação das regras determinísticas da simulação através do `setInterval` e `setTimeout`, conectadas em tempo real ao DOM.

### Como Rodar:
Essa solução não exige instalação de pacotes complexos (Node.js/NPM).
1. Navegue até a pasta `hospital-dashboard`.
2. Dê um duplo clique no arquivo `index.html` para abrir diretamente no seu navegador.
3. Aproveite a interface e acompanhe o comportamento das filas de prioridade e do maqueiro dinamicamente!

---

**Observações:** 
As lógicas desenvolvidas neste projeto focam em respeitar e comprovar teorias de Sistemas de Tempo Real (STR) baseadas em modelagem do AnyLogic.
