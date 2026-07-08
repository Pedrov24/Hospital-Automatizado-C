// --- DOM Elements ---
const DOM = {
    filaTriagem: document.getElementById('filaTriagem'),
    triagemCount: document.getElementById('triagemCount'),
    
    maqueiroStatus: document.getElementById('maqueiroStatus'),
    maqueiroStateLabel: document.getElementById('maqueiroStateLabel'),
    maqueiroActionLabel: document.getElementById('maqueiroActionLabel'),
    
    c1Indicator: document.getElementById('c1-indicator'),
    c1Slot: document.getElementById('c1-slot'),
    c2Indicator: document.getElementById('c2-indicator'),
    c2Slot: document.getElementById('c2-slot'),
    
    leitosCount: document.getElementById('leitosCount'),
    recupSlot1: document.getElementById('recup-slot-1'),
    recupSlot2: document.getElementById('recup-slot-2'),
    
    actionLogs: document.getElementById('actionLogs'),
    statAtendidos: document.getElementById('statAtendidos'),
    btnToggleSim: document.getElementById('btnToggleSim')
};

// --- Configurações da Simulação ---
const SIM_SPEED = 1000; // 1 segundo na simulação = 1000ms reais
const MAX_LEITOS = 2;

// --- Estado da Simulação ---
let isRunning = true;
let totalAtendidos = 0;
let patientIdCounter = 1;

let triagemQueue = [];
let saidaQueue = []; // Aguardando ir para recuperação

let consultorios = {
    1: { id: 1, estado: 'LIVRE', paciente: null },
    2: { id: 2, estado: 'LIVRE', paciente: null }
};

let recuperacao = [null, null]; // 2 leitos

let maqueiro = {
    estado: 'M_LIVRE', // M_LIVRE, OCUPADO
    tarefaAtual: null
};

// --- Funções Auxiliares ---
function logAction(msg) {
    const li = document.createElement('li');
    li.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
    DOM.actionLogs.prepend(li);
    if(DOM.actionLogs.children.length > 20) {
        DOM.actionLogs.removeChild(DOM.actionLogs.lastChild);
    }
}

function randRange(min, max) {
    return Math.floor(Math.random() * (max - min + 1)) + min;
}

function getPatientColorStr(prio) {
    if(prio === 1) return 'Vermelho';
    if(prio === 2) return 'Laranja';
    return 'Azul';
}

function renderPatientCard(p) {
    return `
        <div class="paciente-card prio-${p.prioridade}">
            <div class="paciente-info">
                <h4>Paciente #${p.id}</h4>
                <p>Chegou em: ${p.tempoChegada}</p>
            </div>
            <div class="paciente-prioridade">P${p.prioridade}</div>
        </div>
    `;
}

// --- Atualização da UI ---
function updateUI() {
    // Triagem
    DOM.triagemCount.textContent = triagemQueue.length;
    DOM.filaTriagem.innerHTML = triagemQueue.map(p => renderPatientCard(p)).join('');
    
    // Consultórios
    [1, 2].forEach(id => {
        const c = consultorios[id];
        const indicator = DOM[`c${id}Indicator`];
        const slot = DOM[`c${id}Slot`];
        
        indicator.textContent = c.estado;
        indicator.className = `status-indicator ${c.estado === 'LIVRE' ? '' : (c.estado === 'AGUARDANDO' ? 'aguardando' : 'ocupado')}`;
        
        if (c.paciente) {
            slot.classList.add('filled');
            slot.innerHTML = renderPatientCard(c.paciente);
        } else {
            slot.classList.remove('filled');
            slot.innerHTML = `<p class="empty-text">Vazio</p>`;
        }
    });

    // Recuperação
    let leitosOcupados = 0;
    recuperacao.forEach((paciente, idx) => {
        const slot = DOM[`recupSlot${idx + 1}`];
        if (paciente) {
            leitosOcupados++;
            slot.classList.add('filled');
            slot.innerHTML = renderPatientCard(paciente);
        } else {
            slot.classList.remove('filled');
            slot.innerHTML = `<p class="empty-text">Livre</p>`;
        }
    });
    DOM.leitosCount.textContent = `${leitosOcupados} / ${MAX_LEITOS}`;
    
    // Stats
    DOM.statAtendidos.textContent = totalAtendidos;
}

// --- Lógica das Threads Simuladas ---

// 1. Gerador de Pacientes (Triagem)
function spawnPatient() {
    if(!isRunning) return;
    
    // Random priority
    const rand = Math.random();
    let prioridade = 3; // Azul
    if (rand < 0.3) prioridade = 1; // Vermelho
    else if (rand < 0.6) prioridade = 2; // Laranja
    
    let tempoChegada = Date.now() % 100000;
    let prioridadeFila = (prioridade * 100000) + tempoChegada;
    
    let paciente = {
        id: patientIdCounter++,
        prioridade,
        tempoChegada,
        prioridadeFila
    };
    
    // Insere ordenado na fila (menor prioridadeFila primeiro)
    triagemQueue.push(paciente);
    triagemQueue.sort((a, b) => a.prioridadeFila - b.prioridadeFila);
    
    logAction(`Paciente #${paciente.id} (${getPatientColorStr(prioridade)}) chegou na Triagem.`);
    updateUI();
    maqueiroCheck();
}

// 2. Loop do Consultório
function startAtendimento(id_c) {
    logAction(`Consultório ${id_c} iniciou atendimento do Paciente #${consultorios[id_c].paciente.id}.`);
    
    setTimeout(() => {
        if(!isRunning) return; // Note: simplificado para a demo, o correto seria pausar o timeout
        consultorios[id_c].estado = 'AGUARDANDO';
        saidaQueue.push(id_c);
        logAction(`Consultório ${id_c} concluiu atendimento. Aguardando maqueiro.`);
        updateUI();
        maqueiroCheck();
    }, randRange(3, 6) * SIM_SPEED);
}

// 3. Alta de Pacientes (Recuperação)
function liberarLeito(index) {
    setTimeout(() => {
        if(!isRunning) return;
        const p = recuperacao[index];
        if(p) {
            recuperacao[index] = null;
            totalAtendidos++;
            logAction(`Paciente #${p.id} recebeu ALTA da recuperação.`);
            updateUI();
            maqueiroCheck(); // Maqueiro pode estar preso esperando leito
        }
    }, randRange(4, 8) * SIM_SPEED);
}

// 4. Lógica Inteligente do Maqueiro
function maqueiroCheck() {
    if (!isRunning || maqueiro.estado !== 'M_LIVRE') return;
    
    const leitosOcupados = recuperacao.filter(p => p !== null).length;

    // Prioridade 1: Levar paciente da saída para a recuperação
    if (saidaQueue.length > 0) {
        if (leitosOcupados === MAX_LEITOS) {
            DOM.maqueiroStateLabel.textContent = 'AGUARDANDO LEITO';
            DOM.maqueiroActionLabel.textContent = 'Recuperação cheia...';
            DOM.maqueiroStatus.classList.remove('active');
            return; // Espera liberar
        }
        
        // Pega o primeiro da fila de saída
        const c_id = saidaQueue.shift();
        const p = consultorios[c_id].paciente;
        
        maqueiro.estado = 'OCUPADO';
        DOM.maqueiroStateLabel.textContent = 'TRANSPORTANDO';
        DOM.maqueiroActionLabel.textContent = `Levando Paciente #${p.id} do C${c_id} p/ Recuperação`;
        DOM.maqueiroStatus.classList.add('active');
        
        setTimeout(() => {
            if(!isRunning) return;
            // Libera consultório
            consultorios[c_id].paciente = null;
            consultorios[c_id].estado = 'LIVRE';
            
            // Acomoda no primeiro leito vazio
            const emptyIndex = recuperacao.indexOf(null);
            recuperacao[emptyIndex] = p;
            
            logAction(`Maqueiro acomodou Paciente #${p.id} no Leito ${emptyIndex+1}. Consultório ${c_id} liberado.`);
            
            maqueiro.estado = 'M_LIVRE';
            DOM.maqueiroStateLabel.textContent = 'LIVRE';
            DOM.maqueiroActionLabel.textContent = 'Aguardando tarefas...';
            DOM.maqueiroStatus.classList.remove('active');
            
            updateUI();
            liberarLeito(emptyIndex); // Programa a alta dele
            
            maqueiroCheck(); // Verifica próxima tarefa
        }, 1.5 * SIM_SPEED);
        
        return;
    }
    
    // Prioridade 2: Levar da Triagem para um consultório livre
    if (triagemQueue.length > 0) {
        let c_id = null;
        if (consultorios[1].estado === 'LIVRE' && consultorios[2].estado === 'LIVRE') {
            c_id = Math.random() > 0.5 ? 1 : 2; // 50/50
        } else if (consultorios[1].estado === 'LIVRE') {
            c_id = 1;
        } else if (consultorios[2].estado === 'LIVRE') {
            c_id = 2;
        }
        
        if (c_id !== null) {
            const p = triagemQueue.shift();
            
            maqueiro.estado = 'OCUPADO';
            DOM.maqueiroStateLabel.textContent = 'TRANSPORTANDO';
            DOM.maqueiroActionLabel.textContent = `Levando Paciente #${p.id} p/ Consultório ${c_id}`;
            DOM.maqueiroStatus.classList.add('active');
            
            consultorios[c_id].estado = 'OCUPADO';
            updateUI();
            
            setTimeout(() => {
                if(!isRunning) return;
                consultorios[c_id].paciente = p;
                
                logAction(`Maqueiro deixou Paciente #${p.id} no Consultório ${c_id}.`);
                
                maqueiro.estado = 'M_LIVRE';
                DOM.maqueiroStateLabel.textContent = 'LIVRE';
                DOM.maqueiroActionLabel.textContent = 'Aguardando tarefas...';
                DOM.maqueiroStatus.classList.remove('active');
                
                updateUI();
                startAtendimento(c_id); // Inicia processo do médico
                
                maqueiroCheck();
            }, 1.5 * SIM_SPEED);
        }
    } else {
        DOM.maqueiroStateLabel.textContent = 'LIVRE';
        DOM.maqueiroActionLabel.textContent = 'Aguardando tarefas...';
        DOM.maqueiroStatus.classList.remove('active');
    }
}

// --- Controle de Simulação ---
let spawnInterval;

function initSimulation() {
    logAction("Sistema do Hospital Automatizado Iniciado.");
    
    // Gera 5 pacientes iniciais na triagem
    for (let i = 0; i < 5; i++) {
        spawnPatient();
    }
    
    updateUI();
    
    // Inicia geração de pacientes a cada 2-4 segundos
    spawnInterval = setInterval(() => {
        if(isRunning) spawnPatient();
    }, randRange(2, 4) * SIM_SPEED);
}

// Botão Play/Pause
DOM.btnToggleSim.addEventListener('click', () => {
    isRunning = !isRunning;
    DOM.btnToggleSim.textContent = isRunning ? "Pausar Simulação" : "Retomar Simulação";
    DOM.btnToggleSim.classList.toggle('paused', !isRunning);
    
    if(isRunning) {
        logAction("Simulação Retomada.");
        maqueiroCheck(); // Destrava o maqueiro
    } else {
        logAction("Simulação Pausada.");
    }
});

// Start
initSimulation();
