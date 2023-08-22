#include "estado.h"
#include "pthread.h"
#include "utils/utils.h"


estado_proceso* crear_estado(char* name){
	estado_proceso* e = malloc(sizeof(*e));
	e->nombre = name;
	e->listaDeProcesos = list_create();
	e->mutexEstado = malloc(sizeof(*(e->mutexEstado)));
	pthread_mutex_init(e->mutexEstado, NULL);
	e->semaforoEstado = malloc(sizeof(*(e->semaforoEstado)));
	sem_init(e->semaforoEstado,0,0);
	return e;
}


procesoNuevo* e_pop_primer_proceso(estado_proceso* e){
	pthread_mutex_lock(get_e_mutex(e));
	procesoNuevo* pcb = list_remove(get_e_lista(e), 0);
	pthread_mutex_unlock(get_e_mutex(e));
	return pcb;
}

procesoNuevo* pop_pcb_estado(estado_proceso* estadoAnterior, estado_proceso* estadoNuevo, procesoNuevo* pcb){
	 pthread_mutex_lock(estadoNuevo->mutexEstado);
	 bool _is_the_element(procesoNuevo* data) {
	 		return pcb->pid == data->pid;
	 	}
	 list_remove_by_condition(get_e_lista(estadoNuevo), (void*)_is_the_element);
	 pthread_mutex_unlock(estadoNuevo->mutexEstado);
	 log_info(get_logger(), "PID: %d - Estado Anterior %s - Estado Actual %s", pcb->pid, estadoAnterior->nombre, estadoNuevo->nombre);
	 return pcb;
}


 void push_pcb_estado(estado_proceso* estadoNuevo, estado_proceso* estadoAnterior, procesoNuevo* pcb){
	 pthread_mutex_lock(get_e_mutex(estadoNuevo));
	 list_add(get_e_lista(estadoNuevo), pcb);
	 pthread_mutex_unlock(get_e_mutex(estadoNuevo));
	 log_info(get_logger(), "PID: %d - Estado Anterior %s - Estado Actual %s", pcb->pid, estadoAnterior->nombre, estadoNuevo->nombre);
	 if(strcmp(estadoNuevo->nombre, "READY") == 0){
		 logReady();
	 }
 }

 void push_pcb_estado_new(estado_proceso* estadoNuevo, procesoNuevo* pcb){
	 pthread_mutex_lock(get_e_mutex(estadoNuevo));
	 list_add(get_e_lista(estadoNuevo), pcb);
	 pthread_mutex_unlock(get_e_mutex(estadoNuevo));
	 log_info(get_logger(), "PID: %d - Estado Actual %s", pcb->pid, estadoNuevo->nombre);
 }


void destruir_estado(estado_proceso* e){
	if(list_is_empty(get_e_lista(e))){
		list_destroy(get_e_lista(e));
	} else{
		//list_destroy_and_destroy_elements(e->listaDeProcesos, (void*)pcb_destroy);
	}
	pthread_mutex_destroy(get_e_mutex(e));
	sem_destroy(get_e_semaforos(e));
	free(get_e_semaforos(e));
	free(get_e_mutex(e));
	free(e);
}

t_list* get_e_lista(estado_proceso* e){
	return e->listaDeProcesos;
}

sem_t* get_e_semaforos(estado_proceso* e){
	return e->semaforoEstado;
}

pthread_mutex_t* get_e_mutex(estado_proceso* e){
	return e->mutexEstado;
}

void estadoEnExit(procesoNuevo* pcb, estado_proceso *e_exit,estado_proceso *e_execute){
		pthread_mutex_lock(get_e_mutex(e_exit));
		pcb->estado_actual = EXIT;
		push_pcb_estado(e_exit,e_execute, pcb);
		//stream_send_empty_buffer( kernel_configs->socket_memoria,I_EXIT);
		pthread_mutex_unlock(get_e_mutex(e_exit));
		sem_post(get_e_semaforos(e_exit));
}
