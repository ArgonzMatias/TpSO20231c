#ifndef ESTADO_H_
#define ESTADO_H_
#include "kernel.h"
#include "planificador.h"
#include "utils/utils.h"



t_list* get_e_lista(estado_proceso* e);
sem_t* get_e_semaforos(estado_proceso* e);
pthread_mutex_t* get_e_mutex(estado_proceso* e);
void push_pcb_estado(estado_proceso* estadoNuevo, estado_proceso* estadoAnterior, procesoNuevo* pcb);
void push_pcb_estado_new(estado_proceso* estadoNuevo, procesoNuevo* pcb);
procesoNuevo* e_remove_pcb_cola(estado_proceso* procesoObjetivo, procesoNuevo* pcbObjetivo);
procesoNuevo* e_pop_primer_proceso(estado_proceso* e);
void destruir_estado(estado_proceso* e);
void funcion_pcb_a_declarar(void*);
procesoNuevo* pop_pcb_estado(estado_proceso* estadoAnterior, estado_proceso* estadoNuevo, procesoNuevo* pcb);
estado_proceso* crear_estado(char* name);



#endif
