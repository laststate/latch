#ifndef LASTSTATE_TRACE_H
#define LASTSTATE_TRACE_H

#include <stdbool.h>
#include <stdint.h>

void ls_trace_task_switch(uint16_t previous_task, uint16_t next_task);
void ls_trace_irq_enter(uint16_t irq);
void ls_trace_irq_exit(uint16_t irq);
void ls_trace_mutex_timeout(uint16_t mutex_id, uint32_t waited_ms);
void ls_trace_dma(uint16_t channel, bool started, uint32_t status);
void ls_trace_state(uint16_t machine_id, int32_t previous_state, int32_t next_state);
void ls_trace_link(uint16_t link_id, bool up, uint32_t detail);

#endif
