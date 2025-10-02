#include "mlfq.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "msg.h"

#define MLFQ_LEVELS      3       // 0 = mais prioritária
#define MLFQ_QUANTUM_MS  500     // mesmo quantum para todas as filas (enunciado)

// Filas internas de MLFQ (estáticas no módulo)
static queue_t levels[MLFQ_LEVELS];
static int initialized = 0;

// Estado do processo atualmente no CPU
static uint32_t slice_used_ms = 0;
static int current_level = 0;

static void mlfq_init_if_needed(void) {
    if (initialized) return;

    initialized = 1;
}


// Move todas as entradas atuais de rq (fila externa) para a fila 0 do MLFQ
static void drain_external_rq_into_level0(queue_t *rq) {
    pcb_t *p;
    while ((p = dequeue_pcb(rq)) != NULL) {
        enqueue_pcb(&levels[0], p);
    }
}

// Vai buscar o próximo processo da fila de maior prioridade disponível
static pcb_t* pick_next_from_levels(int *picked_level) {
    for (int lvl = 0; lvl < MLFQ_LEVELS; ++lvl) {
        pcb_t *p = dequeue_pcb(&levels[lvl]);
        if (p) {
            if (picked_level) *picked_level = lvl;
            return p;
        }
    }
    return NULL;
}

void mlfq_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task) {
    mlfq_init_if_needed();

    // 1) Chegadas novas entram sempre na fila 0
    drain_external_rq_into_level0(rq);

    // 2) Se há alguém no CPU, contabiliza
    if (*cpu_task) {
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;
        slice_used_ms += TICKS_MS;

        // a) Terminou
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {
            msg_t msg = {
                .pid = (*cpu_task)->pid,
                .request = PROCESS_REQUEST_DONE,
                .time_ms = current_time_ms
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                perror("write");
            }
            free(*cpu_task);
            *cpu_task = NULL;
            slice_used_ms = 0;
        }
        // b) Gastou quantum → desce de nível (se possível) e reentra na fila correspondente
        else if (slice_used_ms >= MLFQ_QUANTUM_MS) {
            int next_level = current_level < (MLFQ_LEVELS - 1) ? current_level + 1 : current_level;
            enqueue_pcb(&levels[next_level], *cpu_task);
            *cpu_task = NULL;
            slice_used_ms = 0;
        }
    }

    // 3) Se o CPU ficou livre, escolhe o próximo pela maior prioridade
    if (*cpu_task == NULL) {
        *cpu_task = pick_next_from_levels(&current_level);
        // slice_used_ms já está a 0 quando ativamos um novo processo
    }
}
