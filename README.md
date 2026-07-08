# Projeto Hospital Automatizado com Maqueiro Compartilhado
Este projeto simula o fluxo de pacientes em uma unidade hospitalar com dois consultórios, uma sala de recuperação limitada e um único maqueiro compartilhado. A proposta vem de um problema de Sistemas de Tempo Real: coordenar recursos concorrentes sem gerar superlotação, bloqueio operacional ou espera injusta.

A solução foi construída em duas versões complementares:

- uma simulação em C, focada na lógica de concorrência, threads e sincronização;
- versões em C para Linux, usando `pthread` e, opcionalmente, `ncurses`;
- um dashboard web, feito em HTML, CSS e JavaScript, para visualizar o comportamento do sistema em tempo real.

## Problema modelado

O cenário do hospital possui três partes principais:

- **Consultórios 1 e 2:** atendem um paciente por vez. Depois do atendimento, o paciente permanece no consultório até ser removido pelo maqueiro.
- **Maqueiro automatizado:** é o recurso compartilhado do sistema. Ele só transporta um paciente por vez e precisa decidir qual tarefa executar primeiro.
- **Sala de recuperação:** possui apenas dois leitos. Se estiver cheia, nenhum novo paciente pode ser acomodado até que alguém receba alta.

O desafio é manter o hospital funcionando sem deixar consultórios travados, sem colocar pacientes em uma recuperação cheia e sem permitir que um consultório ou paciente espere indefinidamente.

## Solução construída

A lógica implementada usa uma política de semáforos : o maqueiro dá prioridade para remover pacientes que já concluíram o atendimento. Isso libera consultórios rapidamente e reduz o risco de bloqueio do sistema.

Quando não há pacientes aguardando saída dos consultórios, o maqueiro busca o próximo paciente da triagem e o leva para um consultório livre. A fila da triagem é ordenada por prioridade clínica:

1. **Vermelho:** maior prioridade;
2. **Laranja:** prioridade intermediária;
3. **Azul:** menor prioridade.

Pacientes com a mesma prioridade são organizados pelo tempo de chegada, preservando uma lógica parecida com FIFO dentro de cada grupo.

## Estrutura do projeto

```text
.
+-- README.md
+-- servidor_dashboard.py
+-- hospital_automatizado.c
+-- hospital_automatizado_Linux.c
+-- hospital_automatizado_interface_Linux.c
+-- hospital_automatizado.exe
+-- hospital.exe
+-- hospital-dashboard/
    +-- index.html
    +-- main.js
    +-- style.css
```

## Simulação em C

O arquivo `hospital_automatizado.c` implementa a versão mais próxima do conteúdo de Sistemas de Tempo Real. Ele usa a API do Windows para criar threads e coordenar o acesso aos recursos compartilhados.

Principais elementos:

- **Thread de triagem:** cria pacientes continuamente, define a classificação de risco e insere cada paciente na fila de prioridade.
- **Thread do maqueiro:** decide entre liberar consultórios ou levar pacientes da triagem para atendimento.
- **Threads dos consultórios:** simulam o atendimento médico nos consultórios 1 e 2.
- **Thread de recuperação:** libera leitos periodicamente, representando a alta de pacientes.
- **Sincronização:** usa `CRITICAL_SECTION` e `CONDITION_VARIABLE` para evitar acessos simultâneos indevidos ao estado do hospital.

### Como compilar

No Windows, usando GCC/MinGW:

```bash
gcc hospital_automatizado.c -o hospital_automatizado.exe
```

### Como executar

```bash
.\hospital_automatizado.exe
```

## Dashboard web

A pasta `hospital-dashboard` contém uma versão visual da simulação. Ela não substitui a implementação concorrente em C, mas ajuda a entender o fluxo do sistema de forma mais intuitiva.

Na tela é possível acompanhar:

- a fila de triagem ordenada por prioridade;
- o estado atual do maqueiro;
- a ocupação dos dois consultórios;
- os dois leitos da recuperação;
- o total de pacientes atendidos;
- um histórico das últimas ações.


## Versões para Linux
Para Linux, os arquivos recomendados são:

- `hospital_automatizado_Linux.c`: versão de terminal com `pthread`;
- `hospital_automatizado_interface_Linux.c`: versão com interface no terminal usando `ncurses`.

### Compilar a versão simples

```bash
gcc hospital_automatizado_Linux.c -o hospital_linux -pthread
```

### Executar a versão simples

```bash
./hospital_linux
```

### Compilar a versão com interface

Em distribuições Debian/Ubuntu, pode ser necessário instalar a biblioteca de desenvolvimento do ncurses:

```bash
sudo apt install libncurses-dev
```

Depois, compile com:

```bash
gcc hospital_automatizado_interface_Linux.c -o hospital_interface_linux -pthread -lncurses
```

### Executar a versão com interface

```bash
./hospital_interface_linux
```

Na interface, pressione `q` para encerrar. Ao iniciar, essa versão também tenta abrir o dashboard automaticamente no navegador.

## Observações

O arquivo `hospital_automatizado.c` foi pensado para ambiente Windows, pois depende de `<windows.h>`. Para Linux, use as versões com sufixo `_Linux.c`. Já o dashboard web pode ser aberto em qualquer navegador moderno. Os arquivos `.exe` incluídos são executáveis já compilados para Windows.

### Membros do Grupo
- Pedro Vinicius; 
- Pedro Henrique Gomes;
- Vittor Gomes.

