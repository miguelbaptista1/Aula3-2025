#include "rr.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "msg.h"

// Quantum fixo do enunciado
#define RR_QUANTUM_MS 500

void rr_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task) {
    // Tempo já gasto do quantum pelo processo atualmente no CPU
    static uint32_t slice_used_ms = 0;

    if (*cpu_task) {
        // Conta tempo de CPU e de quantum
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;
        slice_used_ms += TICKS_MS;

        // 1) Terminou?
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
        // 2) Não terminou mas gastou o quantum ⇒ preempção e volta ao fim da fila
        else if (slice_used_ms >= RR_QUANTUM_MS) {
            enqueue_pcb(rq, *cpu_task);
            *cpu_task = NULL;
            slice_used_ms = 0;
        }
    }

    // Se o CPU está livre, escolhe o próximo (por ordem de chegada)
    if (*cpu_task == NULL) {
        *cpu_task = dequeue_pcb(rq);
        // novo processo começa quantum do zero
        // (slice_used_ms já está a 0 quando chega aqui após preempção/termino)
    }
}
