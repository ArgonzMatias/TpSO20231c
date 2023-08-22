 #include "planificador.h"
#include "pthread.h"
#include <commons/collections/queue.h>
#include<commons/log.h>

#define CANTIDAD_RECURSOS 5


extern kernel_config* kernel_configs;
t_log* kernelLogger2;
extern segmento* segmento_zero;
extern int max_size_segmento;

static t_queue* arrayRecursos[CANTIDAD_RECURSOS];

static sem_t grado_multiprogramacion;
static sem_t nuevosPcbsReady; // semaforo pcbs para admitir en sistema
static sem_t io;
static sem_t execute_libre;
static sem_t envio_bloqueo_fs;


static pthread_mutex_t mutex_fread_fwrite;
static pthread_mutex_t nextPidMutex;
static pthread_mutex_t lista_recursos;
static pthread_mutex_t mutex_tabla_archivos;
sem_t rwLock;
sem_t resourceAccess;
int readersCount = 0;

static pthread_cond_t readFinished;
static pthread_cond_t writeFinished;
static pthread_cond_t compactReady;

static t_list* lista_global_archivos;
t_list* blocked;



static estado_proceso* e_blocked;
static estado_proceso* e_execute;
static estado_proceso* e_exit;
static estado_proceso* e_ready;
static estado_proceso* e_new;

int nextPid;
int tamanioArrayRecurso = 0;


static int next_pid(void){
	pthread_mutex_lock(&nextPidMutex);
	int newNextPid = nextPid++;
	pthread_mutex_unlock(&nextPidMutex);
	return newNextPid;
}



//HRRN


int64_t get_waiting_time_proceso(procesoNuevo* pcb){
	int64_t waiting;
	waiting = temporal_gettime(pcb->tiempo_de_llegada);        //difftime(pcb->tiempo_de_llegada, time(NULL));
	return waiting;
}

float get_estimado_rafaga_proceso(procesoNuevo* pcb, int alpha){
	float rafaga_ant_estimada = pcb->rafaga_estimada;
	float rafaga_anterior = pcb->rafaga_real;
	float estimado = (alpha * rafaga_anterior) + ((1-alpha) * rafaga_ant_estimada);
    pcb->rafaga_estimada = estimado; //actualizo la nueva estimacion
	return estimado;
}

long int get_hrrn_proceso(procesoNuevo* pcb){
	long int rafaga = (int)get_estimado_rafaga_proceso(pcb, kernel_configs->hrrn_alfa);
	long int waiting = get_waiting_time_proceso(pcb); // get_waiting_time devuelve un dato time_t, me lo toma como long int parece (?
	long int prioridad = 1+(waiting/rafaga);
	return prioridad;
}

static procesoNuevo* mayor_hrrn(procesoNuevo* pcb1, procesoNuevo* pcb2){
	long int rafaga1 = get_hrrn_proceso(pcb1);
	long int rafaga2 = get_hrrn_proceso(pcb2);
	if(rafaga1 > rafaga2){
		return pcb1;
	} else if(rafaga1 < rafaga2){
		return pcb2;
	} else{
		//si la rafaga 1 y rafaga 2 son iguales, seria por fifo.
		if(get_waiting_time_proceso(pcb1) < get_waiting_time_proceso(pcb2)){
		return pcb1;
		}else{
			return pcb2;
		}
	}
}

procesoNuevo* next_hrrn_proceso(estado_proceso* e){
	procesoNuevo* next_pcb = NULL;
	pthread_mutex_lock(get_e_mutex(e));
	int tamanioEstado = list_size(get_e_lista(e));
	if(tamanioEstado > 1){
		next_pcb = list_get_maximum(get_e_lista(e), (void*)mayor_hrrn);
	} else if(tamanioEstado == 1){
		next_pcb = list_remove(get_e_lista(e), 0);
	}
	pthread_mutex_unlock(get_e_mutex(e));
	return next_pcb;
}



// Planificacion


static void execute_pcb(void){
	while(1){
	sem_wait(get_e_semaforos(e_execute));
	procesoNuevo* pcb = e_pop_primer_proceso(e_execute); //busco el primer proceso en execute
	log_info(kernelLogger2, "ejecutando el proceso con pid: %d",  pcb->pid);
	tabla_segmento* elem = list_get(pcb->tabla_de_segmentos,0);
	log_info(kernelLogger2, "limite segmento: %d",  elem->limite);
	enviar_instruccion_cpu(pcb, header_pcb_execute);

	log_info(kernelLogger2, "esperando respuesta de cpu");
	headers head_cpu = stream_recv_header(kernel_configs->socket_cpu); //se muere aca, bad address
	if(head_cpu != header_respuesta_cpu){
		log_error(kernelLogger2, "recibi mal el header de la respuesta de cpu");
		exit(-1);
	}
	log_info(kernelLogger2, "header respuesta de cpu correcto");
	t_buffer* bufferresponse = buffer_create();
	stream_recv_buffer(kernel_configs->socket_cpu, bufferresponse);
	log_info(kernelLogger2, "rta cpu recibida");
	cpuResponse *respuestaCpu = desempaquetar_cpu_response(bufferresponse, kernelLogger2); //aca se traba ahora
	buffer_destroy(bufferresponse);
	pcb->program_counter = respuestaCpu->pc;
	log_info(kernelLogger2, "desempaquetado completo");
	int pid_en_execute = pcb->pid;
	char* recursoDelPcb = respuestaCpu->recurso;
	rtaKernelFs* envioFs = malloc(sizeof(*envioFs));

	log_info(kernelLogger2, "por entrar al switch");
	log_info(kernelLogger2, "flagResponse = %u", respuestaCpu->flag);
	switch(get_flag_cpuResponse(*respuestaCpu)){

	case I_YIELD:
		//actualizo pcb
		pcb->estado_actual = READY;
		push_pcb_estado(e_ready,e_execute, pcb);
		pcb->tiempo_de_llegada = temporal_create();
		log_info(kernelLogger2, "Cola Ready %s : %d",kernel_configs->algoritmo_planificacion, pcb->pid);
		sem_post(get_e_semaforos(e_ready));
		sem_post(&execute_libre);
		//sem_post(get_e_semaforos(e_execute));
		break;


	case I_EXIT:
		pcb->resultadoProceso = "SUCCESS";
		estadoEnExit(pcb, e_exit,e_execute); //funcion que esta en estado.c
		sem_post(&execute_libre);
		//sem_post(get_e_semaforos(e_execute));
		break;


	case I_IO:
		instruccion_en_IO(pcb, get_tiempo_cpuResponse(*respuestaCpu));
		sem_post(&execute_libre);
		//sem_post(get_e_semaforos(e_execute));
		break;


	case I_WAIT:
		pthread_mutex_lock(&lista_recursos);  //bloqueo el uso de recursos por otro proceso
		log_info(kernelLogger2, "entre en wait");
		int resultadoWait = buscar_posicion_recurso(recursoDelPcb);
		log_info(kernelLogger2, "encontre posicion recurso %d", resultadoWait);
		//chequeo si el elemento existe
			if(resultadoWait == -1){
				log_error(kernelLogger2, "ERROR: recurso solicitado no se encuentra en el sistema :(");
				recurso_inexistente(recursoDelPcb, pcb);
				pcb->resultadoProceso = "INVALID_RESOURCE";
				estadoEnExit(pcb, e_exit,e_execute); //manda el proceso completo a exit
				sem_post(&execute_libre);
				pthread_mutex_unlock(&lista_recursos); //desbloqueo recursos
				sem_post(get_e_semaforos(e_execute));
				break;
			}
			else{
				log_info(kernelLogger2, "se encontro el recurso solicitado por la instruccion Wait, chequeando si hay suficientes recursos para la operacion ...");
			}

			log_info(kernelLogger2, "cantidad de instancias del recurso %d", kernel_configs->instancias_recursos[resultadoWait]);
		int resultadoOperacion = hay_elementos_wait(resultadoWait);
		log_info(kernelLogger2, "pase hay elementos wait = %d", resultadoOperacion);
		if(resultadoOperacion == -1){
			queue_push(arrayRecursos[resultadoOperacion], pcb);
			push_pcb_estado(e_execute, e_blocked, pcb);
			logBlocked(pcb->pid,recursoDelPcb);
			pcb->estado_actual = BLOCKED;
			log_info(kernelLogger2, "se pasa pcb de pid %d a estado Blocked por faltante del recurso %s. Cuando haya elementos de dicho recurso, se sacara de blockeado a ready. ", pid_en_execute, recursoDelPcb);
			pthread_mutex_unlock(&lista_recursos); //desbloqueo recursos
			sem_post(&execute_libre);
			//sem_post(get_e_semaforos(e_execute));
			break;
		}
		else{
			log_info(kernelLogger2, "se le asigno 1 recurso del tipo %s al pcb de pid %d", recursoDelPcb, pid_en_execute);
			log_info(kernelLogger2, "PID: %d - Wait: %s - Instancias: %d", pid_en_execute, recursoDelPcb, kernel_configs->instancias_recursos[resultadoWait]);
			push_pcb_estado( e_execute, e_execute, pcb);
			pthread_mutex_unlock(&lista_recursos); //desbloqueo recursos
			sem_post(get_e_semaforos(e_execute));
			break;
		}


	case I_SIGNAL:
		pthread_mutex_lock(&lista_recursos); //bloqueo el uso de recursos por otro proceso
		int resultadoSignal = buscar_posicion_recurso(recursoDelPcb);
			if(resultadoWait == -1){
				log_error(kernelLogger2, "ERROR: recurso solicitado no se encuentra en el sistema :( ");
				recurso_inexistente(recursoDelPcb, pcb);
				pcb->resultadoProceso = "INVALID_RESOURCE";
				estadoEnExit(pcb, e_exit,e_execute); //manda el proceso a exit
				pthread_mutex_unlock(&lista_recursos); //desbloqueo recursos
				sem_post(&execute_libre);
				sem_post(get_e_semaforos(e_execute));
				break;
			}
			else{
				log_info(kernelLogger2, "se encontro el recurso solicitado por la instruccion Signal! :) ");
			}
		kernel_configs->instancias_recursos[resultadoSignal] =+ 1;
		log_info(kernelLogger2, "PID: %d - Signal: %s - Instancias: %d", pid_en_execute, recursoDelPcb, kernel_configs->instancias_recursos[resultadoWait]);
		push_pcb_estado(e_execute, e_execute, pcb);

			if(kernel_configs->instancias_recursos[resultadoSignal] == 0 && arrayRecursos[resultadoSignal] != NULL){ //esto es pq dice que tiene que haber menor a 0 para que haya error y me fijo que hayan elementos en la cola del elemento
				log_info(kernelLogger2, "se detecto que este recurso estaba siendo solicitado por otra instruccion, sacando de blockeado al primer elemento de dicha cola para que pueda operar! ");
				procesoNuevo* procesoDesbloqueado = queue_pop(arrayRecursos[resultadoSignal]);
				pop_pcb_estado(e_execute, e_blocked,procesoDesbloqueado);
				int resultado = hay_elementos_wait(procesoDesbloqueado);
				if(resultado != -1){
					log_info(kernelLogger2,"se logro asignar recurso con exito al proceso de id %d que estaba en la cola del recurso! ", procesoDesbloqueado->pid);
				}
				log_info(kernelLogger2, "PID: %d - Wait: %s - Instancias: %d", procesoDesbloqueado->pid, recursoDelPcb, kernel_configs->instancias_recursos[resultadoWait]);
				procesoDesbloqueado->estado_actual = READY;
				procesoDesbloqueado->tiempo_de_llegada = temporal_create();
				push_pcb_estado(e_ready,e_blocked,procesoDesbloqueado);

				sem_post(get_e_semaforos(e_ready));
			}
			pthread_mutex_unlock(&lista_recursos); //desbloqueo recursos
			push_pcb_estado(e_execute, e_execute, pcb);
			sem_post(get_e_semaforos(e_execute));
		break;

	// memoria

	case I_DELETE_SEGMENT:
		log_info(kernelLogger2, "PID: %d - Eliminar Segmento - Id Segmento %d ", pcb->pid, respuestaCpu->id_segmento);
		stream_send_empty_buffer( kernel_configs->socket_memoria,I_DELETE_SEGMENT);
		t_buffer* inDeleteSegment = buffer_create();
		buffer_pack(inDeleteSegment, respuestaCpu->id_segmento, sizeof(respuestaCpu->id_segmento));
		stream_send_buffer(kernel_configs->socket_memoria, header_resultado, inDeleteSegment);
		//tabla_segmento* segmentoAEliminar = recive_segmento_nuevo();
		// esta es la nueva tabla de segmentos

		// elimino del pcb el segmento que elimino
		int id_a_eliminar(tabla_segmento segmentoAux){
						return(segmentoAux.id == respuestaCpu->id_segmento );
		}
		list_remove_by_condition(pcb->tabla_de_segmentos, (void*) id_a_eliminar);
		log_info(kernelLogger2, "Segmento eliminado con exito para el PID: %d  con id de segmento %d ", pcb->pid, respuestaCpu->id_segmento);
		buffer_destroy(inDeleteSegment);
		push_pcb_estado(e_execute, e_execute, pcb);
		sem_post(get_e_semaforos(e_execute));
		break;

	case I_CREATE_SEGMENT:
		log_info(kernelLogger2, "PID: %d - Crear Segmento - Id %d - Tamanio: %d", pcb->pid, respuestaCpu->id_segmento, respuestaCpu->sizeSegmento);
		if(max_size_segmento == list_size(pcb->tabla_de_segmentos)){
			log_error(kernelLogger2, "maxima cantidad de segmentos por pcb alcanzada. Error");
			pcb->resultadoProceso = "SEG_FAULT";
			estadoEnExit(pcb, e_exit,e_execute);
			sem_post(&execute_libre);
			sem_post(get_e_semaforos(e_execute));
		} else{

		stream_send_empty_buffer(kernel_configs->socket_memoria,I_CREATE_SEGMENT);
		t_buffer* pidSegmento = buffer_create();
		buffer_pack(pidSegmento, &pcb->pid, sizeof(pcb->pid));
		stream_send_buffer(kernel_configs->socket_memoria, header_resultado, pidSegmento);
		buffer_destroy(pidSegmento);

		t_buffer* inCreateSegment = buffer_create();
		buffer_pack(inCreateSegment, &respuestaCpu->sizeSegmento, sizeof(respuestaCpu->sizeSegmento));
		stream_send_buffer(kernel_configs->socket_memoria, header_resultado, inCreateSegment);
		buffer_destroy(inCreateSegment);

		t_buffer* idCreateSegment = buffer_create();
		buffer_pack(idCreateSegment, &respuestaCpu->id_segmento, sizeof(respuestaCpu->id_segmento));
		stream_send_buffer(kernel_configs->socket_memoria, header_resultado, idCreateSegment);
		buffer_destroy(idCreateSegment);


		headers header = stream_recv_header(kernel_configs->socket_memoria);
		stream_recv_empty_buffer(kernel_configs->socket_memoria);
		consultar_memoria_create(header);
		log_info(kernelLogger2, "esperando recibir la rta de memoria create segment");

		headers head= stream_recv_header(kernel_configs->socket_memoria);
		if(head != header_general){
			log_error(kernelLogger2, "header equivocado en create segmento");
		}
		if(head == header_general){
					log_info(kernelLogger2, "header recibido bien create segmento");
				}
		t_buffer* nuevoSegmento = buffer_create();
		stream_recv_buffer(kernel_configs->socket_memoria ,nuevoSegmento);

		tabla_segmento* newEntradaATabla = desempaquetar_segmento(nuevoSegmento,kernelLogger2);
		buffer_destroy(nuevoSegmento);
		log_info(kernelLogger2, "segmento creado y recibidio por kernel con limite %d", newEntradaATabla->limite);

		list_add(pcb->tabla_de_segmentos, newEntradaATabla);
		log_info(kernelLogger2, "Segmento creado con exito para el PID: %d  con id de segmento %d y tamanio %d", pcb->pid, respuestaCpu->id_segmento, respuestaCpu->sizeSegmento);
		push_pcb_estado(e_execute, e_execute, pcb);
		sem_post(get_e_semaforos(e_execute));
		}
		break;

	//filesystem

	case I_OPEN:
		log_info(kernelLogger2, "entre open");
		pthread_mutex_lock(&mutex_tabla_archivos);
		/*if(list_is_empty(lista_global_archivos)){
			creacionArchivoFileSystem(pcb, respuestaCpu.archivo);
			tabla_de_archivos archivoNuevo;
			archivoNuevo.archivo = respuestaCpu.archivo;
			list_add(lista_global_archivos, archivoNuevo);
		}
		else{*/
		int mismo_id(tabla_de_archivos tablaAux){
				return(strcmp(respuestaCpu->archivo, tablaAux.archivo) == 0);
			}
		log_info(kernelLogger2, "pasa funcion rara");
		if (list_find(lista_global_archivos, (void*) mismo_id)){ //se fija si existe el archivo en la tabla global de archivos abiertos
			log_info(kernelLogger2, "entra al if");
			tabla_de_archivos* archivoBuscado = list_find(lista_global_archivos, (void*) mismo_id);
			log_info(kernelLogger2, "encuentra funcion rara");
			queue_push(archivoBuscado->pcb_en_espera,pcb);
			push_pcb_estado(e_execute, e_blocked, pcb);
			pcb->estado_actual = BLOCKED;
			logBlocked(pcb->pid,archivoBuscado->archivo); // le paso el pid y nombre del archivo
			sem_post(&execute_libre);
			//sem_post(get_e_semaforos(e_execute));
		} else{
			log_info(kernelLogger2, "va al else");
			envioFs->nombre_archivo = respuestaCpu->archivo;
			enviar_archivo_fs(envioFs, I_OPEN);
			int header = stream_recv_header(kernel_configs->socket_filesystem);
			stream_recv_empty_buffer(kernel_configs->socket_filesystem);
				if(header == header_nuevo_archivo_filesystem){ //si el archivo no existe
					log_info(kernelLogger2, "archivo %s no existe en sistema. Creando el archivo en filesystem... ", respuestaCpu->archivo);
					creacionArchivoFileSystem(pcb,envioFs);

				}
				else if (header == header_general){
					log_info(kernelLogger2, "archivo %s solicitado existente y disponible para usar ", respuestaCpu->archivo);
				}
				tabla_de_archivos* archivoNuevo = crear_tabla_de_archivos(respuestaCpu->archivo);
				list_add(lista_global_archivos, archivoNuevo); //nose pq me tira error list_add
				list_add(pcb->tabla_de_archivos, archivoNuevo);
				log_info(kernelLogger2, "PID: %d - Abrir Archivo: %s ", pcb->pid, respuestaCpu->archivo);
				push_pcb_estado(e_execute, e_execute, pcb);
				sem_post(get_e_semaforos(e_execute));
		}
		pthread_mutex_unlock(&mutex_tabla_archivos);
		log_info(kernelLogger2, "termine open");
		//}
		// va a ser parecido a lo de wait, si el archivo esta abierto, lo paso a blocked.
		break;
	case I_CLOSE:
		int mismo_archivo(tabla_de_archivos* tablaAux){
						return(strcmp(respuestaCpu->archivo, tablaAux->archivo) == 0);
					}
		bool mismo_bool_archivo(tabla_de_archivos tablaAux){
								return(strcmp(respuestaCpu->archivo, tablaAux.archivo) == 0);
							}
		list_remove_by_condition(pcb->tabla_de_archivos, mismo_archivo); //lo saco de la lista de archivos abiertos

		pthread_mutex_lock(&mutex_tabla_archivos);
		tabla_de_archivos* archivo_objetivo = list_find(lista_global_archivos, mismo_archivo);
		if(list_is_empty(archivo_objetivo->pcb_en_espera)){ //se fija si tiene pcbs esperando a usar el archivo
			list_remove_by_condition(lista_global_archivos, (void*) mismo_bool_archivo); //si no lo tiene, lo saca de la lista global de archivos abiertos
			log_info(kernelLogger2, "Se termino de usar el Archivo: %s ",respuestaCpu->archivo);

		}
		else{ //si el archivo tiene pcbs en espera,
			procesoNuevo* pcbDesbloqueado = queue_pop(archivo_objetivo->pcb_en_espera); //hace un pop agarrando el primero que estaba busacando,
			pop_pcb_estado(e_blocked, e_blocked,pcbDesbloqueado); //saca el estado de blocked
			list_add(pcb->tabla_de_archivos, archivo_objetivo); //le agrega a su lista de archivos abiertos el archivo
			pcbDesbloqueado->estado_actual = READY; // cambia el estado a  ready
			pcbDesbloqueado->tiempo_de_llegada = temporal_create(); //crea un nuevo temporal para el calculo de HRRN
			log_info(kernelLogger2, "PID: %d - Abrir Archivo: %s ", pcb->pid, respuestaCpu->archivo);
			push_pcb_estado(e_ready,e_blocked,pcbDesbloqueado); // le hace un puch a ready

			sem_post(get_e_semaforos(e_ready)); //y le avisa a ready q tiene un nuevo proceso q atender
		}
		log_info(kernelLogger2, "PID: %d - Cerrar Archivo: %s ", pcb->pid, respuestaCpu->archivo);
		push_pcb_estado(e_execute, e_execute, pcb); //ahora pusheamos el pcb que estuvimos atendiendo pq todavia le toca a el la atencion
		sem_post(get_e_semaforos(e_execute)); // y le avisa a execute q puede continuar

		pthread_mutex_unlock(&mutex_tabla_archivos);
			break;
	case I_SEEK:
		int mismo_archivo2(tabla_de_archivos* tablaAux){
						return(strcmp(respuestaCpu->archivo, tablaAux->archivo) == 0);
					}
		tabla_de_archivos* archivoBuscado = list_find(pcb->tabla_de_archivos, (void*)mismo_archivo2); //busco el archivo en la tabla de pcbs de archivos
		archivoBuscado->posicion_puntero = respuestaCpu->posicionPuntero;
		list_remove_by_condition(pcb->tabla_de_archivos,(void*) mismo_archivo); //lo saco de la lista de archivos abiertos
		list_add(pcb->tabla_de_archivos, archivoBuscado);
		log_info(kernelLogger2, "PID: %d - Actualizar puntero Archivo: %s - Puntero %d",pcb->pid, respuestaCpu->archivo, respuestaCpu->posicionPuntero);
		push_pcb_estado(e_execute, e_execute, pcb);
		sem_post(get_e_semaforos(e_execute));
		break;
	case I_TRUNCATE:
		envioFs->nombre_archivo = respuestaCpu->archivo;
		envioFs->size = respuestaCpu->sizeSegmento;
		enviar_archivo_fs(envioFs, I_TRUNCATE);
		streamPcbFS(pcb->pid);
		log_info(kernelLogger2, "haciendo truncate");
		log_info(kernelLogger2, "PID: %d - Archivo: %s - Tamaño: %d",pcb->pid, respuestaCpu->archivo, respuestaCpu->sizeSegmento);
		pcb->estado_actual = BLOCKED;
		push_pcb_estado(e_blocked, e_execute, pcb);
		//log_info(get_logger(), "PID: %d - Estado Anterior %s - Estado Actual %s", pcb->pid, "EXECUTE","BLOCKED");
		//list_add(blocked, pcb);
		//log_info()
		logBlocked(pcb->pid,"IO");
		log_info(kernelLogger2, "prcoeso bloqueado por truncate, debe esperar a que finalize el proceso");
		sem_post(&envio_bloqueo_fs);

		sem_post(&execute_libre); // quien mierda lo saca de blocked? Un nuevo hilo?
		//sem_post(get_e_semaforos(e_execute));
		break;
	case I_READ:
		tabla_de_archivos* archivoBuscadoRead = list_find(pcb->tabla_de_archivos,(void*) mismo_archivo2); //busco el archivo en la tabla de pcbs de archivos
		envioFs->puntero = archivoBuscadoRead->posicion_puntero;
		envioFs->direccionFisica = respuestaCpu->direccionFisica;
		envioFs->size = respuestaCpu->sizeSegmento;
		envioFs->pid = pcb->pid;
		envioFs->nombre_archivo = respuestaCpu->archivo;
		streamPcbFS(pcb->pid);
		sem_wait(&rwLock);
		    readersCount++;
		    if (readersCount == 1) {
		        sem_wait(&resourceAccess);
		    }

		    sem_post(&rwLock);
		f_read_write(pcb, envioFs, I_READ);
		sem_wait(&rwLock);
		    readersCount--;
		    if (readersCount == 0) {
		        sem_post(&resourceAccess);
		    }

		    sem_post(&rwLock);
		log_info(kernelLogger2, "PID: %d - Leer Archivo: %s - Puntero %d - Dirección Memoria %d - Tamaño %d",pcb->pid, respuestaCpu->archivo,respuestaCpu->posicionPuntero,respuestaCpu->direccionFisica , respuestaCpu->sizeSegmento);

		sem_post(&envio_bloqueo_fs);
		sem_post(&execute_libre);
		//sem_post(get_e_semaforos(e_execute));
		break;
	case I_WRITE:
		tabla_de_archivos* archivoBuscadoWrite = list_find(pcb->tabla_de_archivos,(void*) mismo_archivo2); //busco el archivo en la tabla de pcbs de archivos
		envioFs->puntero = archivoBuscadoWrite->posicion_puntero;
		envioFs->direccionFisica = respuestaCpu->direccionFisica;
		envioFs->size = respuestaCpu->sizeSegmento;
		envioFs->pid = pcb->pid;
		envioFs->nombre_archivo = respuestaCpu->archivo;
		streamPcbFS(pcb->pid);
		sem_wait(&resourceAccess);
		f_read_write(pcb, envioFs, I_WRITE);
		 sem_post(&resourceAccess);
		log_info(kernelLogger2, "PID: %d - Escribir Archivo: %s - Puntero %d - Dirección Memoria %d - Tamaño %d",pcb->pid, respuestaCpu->archivo,respuestaCpu->posicionPuntero,respuestaCpu->direccionFisica , respuestaCpu->sizeSegmento);
		sem_post(&envio_bloqueo_fs);
		sem_post(&execute_libre);
		//sem_post(get_e_semaforos(e_execute));
		break;
	case SEG_FAULT:
		pcb->resultadoProceso = "SEG_FAULT";
		estadoEnExit(pcb, e_exit,e_execute);
		sem_post(&execute_libre);
		//sem_post(get_e_semaforos(e_execute));
	}
	log_info(kernelLogger2, "instruccion %u finalizada", respuestaCpu->flag);
	free(envioFs);
}
}

void streamPcbFS(int pid){
	t_buffer* bufferpcb = buffer_create();
	log_info(kernelLogger2, "enviando pid %d por bloqueo de proceso a FS", pid);
	buffer_pack(bufferpcb,&pid,sizeof(int));
	stream_send_buffer(kernel_configs->socket_fs_unblocked, hs_aux, bufferpcb);
	log_info(kernelLogger2, "pid %d enviado", pid);
	buffer_destroy(bufferpcb);
}

void* nuevo_pcb_new(void* socket){
	log_info(kernelLogger2, "entre al nuevo_pcb_new");
	int hs_rsultado = handshake(socket, hs_consola, "consola", kernelLogger2);
	log_info(kernelLogger2, "hice el handshake");
	if(hs_rsultado == 1){
	int* socket_pcb = (int*) socket;
	int consola_rta = stream_recv_header(*socket_pcb);

	if(consola_rta != header_lista_instrucciones){
		log_error(kernelLogger2, "error header de instrucciones");
		return NULL;
	}
	log_info(kernelLogger2, "por recibir instrucciones");
	t_buffer* instruccionesBuffer = buffer_create();
	stream_recv_buffer(*socket_pcb, instruccionesBuffer);
	log_info(kernelLogger2, "recibi instrucciones");

	t_buffer* pidReferenciaConsola = buffer_create();
	procesoNuevo* pcbNuevo = nuevoProceso(instruccionesBuffer);
	log_info(kernelLogger2, "genero nuevo proceso");
	pcbNuevo->socketProceso = socket_pcb;
	int pidProceso = pcbNuevo->pid ;
	/*buffer_pack(pidReferenciaConsola, &pidProceso, sizeof(pidProceso));
	stream_send_buffer(*socket_pcb, header_pcb_pid, pidReferenciaConsola); //pid que le avisa a consola cual es el suyo.
	buffer_destroy(pidReferenciaConsola);*/

	stream_send_empty_buffer(kernel_configs->socket_memoria, CREAR_PROCESO);
	pidReferenciaConsola = buffer_create();
	buffer_pack(pidReferenciaConsola, &pidProceso, sizeof(pidProceso));
	stream_send_buffer(kernel_configs->socket_memoria, CREAR_PROCESO, pidReferenciaConsola); //pid que le avisa a memoria cual es el proceso.
	buffer_destroy(pidReferenciaConsola);

	push_pcb_estado_new(e_new, pcbNuevo);
	buffer_destroy(instruccionesBuffer);
	sem_post(&nuevosPcbsReady);
	log_info(kernelLogger2, "termine nuevo_pcb_new");
	}
	return NULL;
}

static void estado_end_liberar(void){
	while(1){
		sem_wait(get_e_semaforos(e_exit));
		procesoNuevo* aux = e_pop_primer_proceso(e_exit);
		liberar_segmentos_pcb(aux);
		log_info(kernelLogger2, "Finaliza el proceso %d -Motivo: %s", aux->pid, aux->resultadoProceso);
		t_buffer *bufferConsola = buffer_create();
		buffer_pack(bufferConsola, aux->resultadoProceso, sizeof(aux->resultadoProceso));
		stream_send_buffer(aux->socketProceso , header_resultado, bufferConsola); //le manda a la consola el resultado del proceso.
		int* socketAux = aux->socketProceso;
		stream_send_empty_buffer(*socketAux, hs_aux);
		if(aux->lista_instrucciones != NULL){
			buffer_destroy(aux->lista_instrucciones);
		}
		destroy_pcb(aux);
		buffer_destroy(bufferConsola);


		sem_post(&grado_multiprogramacion);
	}
}

static void planificador_largo_plazo(void){

	pthread_t  freeProcesoExit;
	pthread_create(&freeProcesoExit, NULL, (void*)estado_end_liberar, NULL);
	pthread_detach(freeProcesoExit);



	while(1){

	sem_wait(&nuevosPcbsReady); // semaforo pcbs para admitir en sistema
	sem_wait(&grado_multiprogramacion); //semaforo de multiprogramacion
	pthread_mutex_lock(e_new->mutexEstado);
	procesoNuevo* pcbReady = list_remove(e_new->listaDeProcesos, 0);
	pthread_mutex_unlock(e_new->mutexEstado);
	//aca tenemos que asignar lugar en la memoria
	push_pcb_estado(e_ready,e_new, pcbReady);
	pcbReady->tiempo_de_llegada = temporal_create();

	log_info(kernelLogger2, "Cola Ready %s : %d",kernel_configs->algoritmo_planificacion, pcbReady->pid);
	sem_post(e_ready->semaforoEstado);
	}
}

static void planificador_corto_plazo(void){

	pthread_t  th_execute_pcb;
	pthread_create(&th_execute_pcb, NULL, (void*) execute_pcb, NULL);
	pthread_detach(th_execute_pcb);

	while(1){
		sem_wait(get_e_semaforos(e_ready));
		sem_wait(&execute_libre);
		procesoNuevo* pcbExecute;
			//eligo que algoritmo ejecutar
			if(strcmp(kernel_configs->algoritmo_planificacion, "FIFO") == 0){
				pcbExecute = e_pop_primer_proceso(e_ready);
			} else if(strcmp(kernel_configs->algoritmo_planificacion, "HRRN") == 0){
				pcbExecute = next_hrrn_proceso(e_ready);
			}
		push_pcb_estado(e_execute,e_ready, pcbExecute);
		log_info(kernelLogger2, "algoritmo elegido = %s", kernel_configs->algoritmo_planificacion);
		log_info(kernelLogger2, "se paso proceso de ready a execute el proceso %d", pcbExecute->pid);
		sem_post(e_execute->semaforoEstado);
}
}


procesoNuevo* pop_blocked(int pid){
	bool pcb_ublocked(procesoNuevo* pcbList){
									return(pcbList->pid == pid );
	}
			procesoNuevo* pcbUnblocked = list_find(e_blocked->listaDeProcesos,(void*) pcb_ublocked);
			list_remove_by_condition(e_blocked->listaDeProcesos,(void*) pcb_ublocked);
			log_info(get_logger(), "se libero el pcb con pid %d ", pcbUnblocked->pid);
			if(pcbUnblocked == NULL){
				log_error(get_logger(), "no existe pcbs cargados en blocked! Error al querer desbloquear.");
			}
			return pcbUnblocked;
}


static void unblocked_pcb_after_fs(void){

	while(1){
		//el filesystem solo le manda esto al kernel? Si es asi no necesito nada
		//sino tengo que poner un semaforo

		t_buffer* bufferPcb = buffer_create();
		stream_recv_header(kernel_configs->socket_fs_unblocked);
		stream_recv_buffer(kernel_configs->socket_fs_unblocked, bufferPcb);
		int pid;
		buffer_unpack(bufferPcb, &pid, sizeof(pid));
		buffer_destroy(bufferPcb);
		sem_wait(&envio_bloqueo_fs);
		procesoNuevo* pcbUnblocked = pop_blocked(pid);


		pcbUnblocked->estado_actual = READY;
		 log_info(get_logger(), "PID: %d - Estado Anterior %s - Estado Actual %s", pid, "BLOCKED", "EXECUTE");
		pcbUnblocked->tiempo_de_llegada = temporal_create();
		push_pcb_estado(e_ready,e_execute, pcbUnblocked);


		sem_post(get_e_semaforos(e_ready));
}
}



procesoNuevo* nuevoProceso(t_buffer* instrucciones){
	log_info(kernelLogger2, "entre nuevo proceso");
	procesoNuevo* aux = malloc(sizeof(*aux));
	aux->pid = next_pid();
	log_info(kernelLogger2, "obtuve next pid");
	aux->lista_instrucciones = obtener_lista_instrucciones(instrucciones, kernelLogger2);
	log_info(kernelLogger2, "obtuve lista nuevas instrucciones");
	aux->program_counter = 0;
	aux->tabla_de_segmentos = list_create();
	log_info(kernelLogger2, "segmento_zero base = %d", segmento_zero->base);
	log_info(kernelLogger2, "segmento_zero id = %d", segmento_zero->id);
	log_info(kernelLogger2, "segmento_zero limite = %d", segmento_zero->limite);
	list_add(aux->tabla_de_segmentos, segmento_zero);
	aux->rafaga_estimada = kernel_configs->estmacion_inicial; // esto hay que difinirlo por archivo de config
	aux->tiempo_de_llegada = temporal_create(); //calculado con hrrn despues cada vez que entra a ready se re calcula.
	aux->tabla_de_archivos = list_create(); //usado en Filesystem
 	aux->estado_actual =  NEW;
	aux->rafaga_real = kernel_configs->estmacion_inicial; //nose pq no me lo toma pero lo declare en utiles.h
	aux->socketProceso = 0;
	log_info(kernelLogger2, "Se crea el proceso ID %d en NEW", aux->pid);
	return aux;
}



void inicializar_planificador(){

	//inicio los mutex
	pthread_mutex_init(&nextPidMutex,NULL);
	pthread_mutex_init(&lista_recursos, NULL);
	pthread_mutex_init(&mutex_tabla_archivos, NULL);
	pthread_mutex_init(&mutex_fread_fwrite, NULL);

	sem_init(&rwLock, 0, 1);
	sem_init(&resourceAccess, 0, 1);

	kernelLogger2 = get_logger();
	log_info(kernelLogger2,"iniciando planificador");
	int valorMultiprogramacion = kernel_configs->grado_max_multiprogramacion; //esto viene por el archivo de config de kernel


	sem_init(&nuevosPcbsReady, 0, 0);
	sem_init(&grado_multiprogramacion, 0, valorMultiprogramacion);
	sem_init(&io,0,0);
	sem_init(&execute_libre, 0 , 1);
	sem_init(&envio_bloqueo_fs, 0 , 0);

	nextPid = 1;

	blocked = list_create();
	tamanioArrayRecurso = sizeof(kernel_configs->recurso);
	for (int i = 0; i < tamanioArrayRecurso; i++){
		t_queue* cola = queue_create();
		arrayRecursos[i] = cola;
	}

	e_new = crear_estado("NEW");
	e_ready = crear_estado("READY");
	e_execute = crear_estado("EXECUTE");
	e_exit = crear_estado("EXIT");
	e_blocked = crear_estado("BLOCKED");

	lista_global_archivos = list_create();

	log_info(kernelLogger2, "Los estados fueron creados");

	log_info(kernelLogger2, "El grado de multiprogramacion es de %d", valorMultiprogramacion);

	pthread_t  th_largo_plazo;
	pthread_create(&th_largo_plazo, NULL, (void*) planificador_largo_plazo, NULL);
	pthread_detach(th_largo_plazo);

	pthread_t  th_corto_plazo;
	pthread_create(&th_corto_plazo, NULL, (void*) planificador_corto_plazo , NULL);
	pthread_detach(th_corto_plazo);

	pthread_t  th_fs_unblocked;
	pthread_create(&th_fs_unblocked, NULL, (void*) unblocked_pcb_after_fs , NULL);
	pthread_detach(th_fs_unblocked);

	log_info(kernelLogger2, "hilos de planificacion creados");

}


void enviar_instruccion_cpu (procesoNuevo* pcb, uint8_t header){
	t_buffer* bufferEjecutar = buffer_create();
	cpuResponse* cpu_enviar = malloc(sizeof(cpuResponse));
	cpu_enviar->pid = pcb->pid;//pack
	buffer_pack(bufferEjecutar, &cpu_enviar->pid, sizeof(cpu_enviar->pid));
	log_info(kernelLogger2, "mando pid: %d", pcb->pid);

	cpu_enviar->pc = (uint32_t)pcb->program_counter;//pack
	buffer_pack(bufferEjecutar, &cpu_enviar->pc, sizeof(cpu_enviar->pc));
	log_info(kernelLogger2, "mando pc: %d", pcb->program_counter);

	cpu_enviar->registros = pcb->registro_cpu;//pack
	buffer_pack(bufferEjecutar, &cpu_enviar->registros, sizeof(cpu_enviar->registros));
	//pack_registros(bufferEjecutar, cpu_enviar);

	log_info(kernelLogger2, "por agarrar tamanio");
	int tam_lista_instruc = list_size(pcb->lista_instrucciones);
	instrucciones* instruc_ejemplo = list_get(pcb->lista_instrucciones, 0);
	buffer_pack(bufferEjecutar, &tam_lista_instruc, sizeof(int));
	cpu_enviar->lista_instrucciones = pcb->lista_instrucciones;//funcion
	for(int i = 0; i< list_size(cpu_enviar->lista_instrucciones); i++){
		instrucciones* aux =  list_get(cpu_enviar->lista_instrucciones,i);
					log_info(kernelLogger2, "numero instruccion  %u " , aux->instruccion_nombre);
	}
	log_info(kernelLogger2, "por empaquetar listas");
	empaquetar_lista_instrucciones(bufferEjecutar,tam_lista_instruc,cpu_enviar->lista_instrucciones, kernelLogger2);
	log_info(kernelLogger2, "tamanio lista: %d", list_size(cpu_enviar->lista_instrucciones));

	int tam_lista_segmentos = list_size(pcb->tabla_de_segmentos);
	buffer_pack(bufferEjecutar, &tam_lista_segmentos, sizeof(int));
	cpu_enviar->tabla_segmentos = pcb->tabla_de_segmentos;//funcion
	log_info(kernelLogger2, "por empaquetar lista de segmentos");
	tabla_segmento* elem = list_get(pcb->tabla_de_segmentos,0);
	log_info(kernelLogger2, "limite segmento : %d", elem->limite);
	empaquetar_lista_segmentos(bufferEjecutar, tam_lista_segmentos, cpu_enviar->tabla_segmentos, kernelLogger2);
	log_info(kernelLogger2, "empaquetado con exito");
	cpu_enviar->archivo = "";

	buffer_pack_string(bufferEjecutar, cpu_enviar->archivo);
	buffer_pack(bufferEjecutar,&cpu_enviar->flag,sizeof(cpu_enviar->flag));
	log_info(kernelLogger2, "empaquetado flag");
	buffer_pack(bufferEjecutar,&cpu_enviar->tiempo,sizeof(cpu_enviar->tiempo));
	buffer_pack_string(bufferEjecutar,&cpu_enviar->recurso);//revisar si falla
	buffer_pack(bufferEjecutar,&cpu_enviar->sizeSegmento,sizeof(cpu_enviar->sizeSegmento));
	buffer_pack(bufferEjecutar,&cpu_enviar->id_segmento,sizeof(cpu_enviar->id_segmento));
	buffer_pack(bufferEjecutar,&cpu_enviar->posicionPuntero,sizeof(cpu_enviar->posicionPuntero));
	buffer_pack(bufferEjecutar,&cpu_enviar->direccionFisica,sizeof(cpu_enviar->direccionFisica));

	stream_send_buffer(kernel_configs->socket_cpu, header, bufferEjecutar);
	buffer_destroy(bufferEjecutar);
}


void instruccion_en_IO(procesoNuevo* pcb, int tiempo){
	pcb->estado_actual = BLOCKED;
	push_pcb_estado(e_blocked, e_execute, pcb);
	logBlocked(pcb->pid,"IO");
	log_info(kernelLogger2, "PID %d - Ejecuta IO: %d", pcb->pid, tiempo);
	pthread_t ejecutarIO;
	pthread_create(&ejecutarIO, NULL, sleepHilo, (void*) &tiempo);

	pthread_detach(ejecutarIO);
	sem_wait(&io);
	pop_blocked(pcb->pid);

	push_pcb_estado(e_ready,e_blocked, pcb);
	pcb->estado_actual = READY;
	pcb->tiempo_de_llegada = temporal_create();
	sem_post(get_e_semaforos(e_ready));
	log_info(kernelLogger2, "Cola Ready %s : %d",kernel_configs->algoritmo_planificacion, pcb->pid);

	return;
}


void sleepHilo(void* tiempo_ptr) {
    int tiempo = *(int*)tiempo_ptr;
    sleep(tiempo);
    sem_post(&io);
}

instruccion get_flag_cpuResponse(cpuResponse cpu){
	 return cpu.flag;
}

int get_tiempo_cpuResponse(cpuResponse cpu){
	return cpu.tiempo;
}

void destroy_pcb(procesoNuevo* pcb){
	if(pcb->lista_instrucciones != NULL){
		buffer_destroy(pcb->lista_instrucciones);
	}
	if(pcb->socketProceso != NULL){
		close(*pcb->socketProceso);
		free(pcb->socketProceso);
	}
	free(pcb);
}

//logica para instrucciones de WAIT y SIGNAL

int count_elements(char** array) {
    int count = 0;
    if (array == NULL) {
        return count;
    }
    while (array[count] != NULL) {
        count++;
    }
    return count;
}

int buscar_posicion_recurso(char* recurso){
	log_info(kernelLogger2, "entre buscar posicion");
	char** arrayRecursos = kernel_configs->recurso; //huardo el array en una variable para su uso
	log_info(kernelLogger2, "agarre array");
	int sizeArray = count_elements(arrayRecursos); //busco tamanio del array recursos
	log_info(kernelLogger2, "tamanio char %d", string_length(recurso));
	log_info(kernelLogger2, "tengo size = %d", sizeArray);
	for(int i=0;i<sizeArray; i++){
		log_info(kernelLogger2, "entre for");
		if( strcmp(arrayRecursos[i],recurso ) == 0 ){
			log_info(kernelLogger2, "entro if");
			return i;
		}
		log_info(kernelLogger2, "pase if");
	}
	return -1;
}

int hay_elementos_wait(int posicion){
	if(kernel_configs->instancias_recursos[posicion] >= 0 ){
		kernel_configs->instancias_recursos[posicion] -= 1;
		return 1;
	}
	else{
		return -1;
	}
}


void recurso_inexistente(char*recursoPcb,procesoNuevo* pcb){
	log_error(kernelLogger2, "recurso %s no existe", recursoPcb);
}

void consultar_memoria_create(headers rtaMemoria, procesoNuevo* pcb){
				t_list* tablaSegmentos;
	if(rtaMemoria ==  header_out_of_memory){
				 pcb->resultadoProceso = "Out of memory";
				 estadoEnExit(pcb, e_exit,e_execute);
				 sem_post(&execute_libre);
	} else if(rtaMemoria ==  header_compactacion){
				log_info(kernelLogger2, "Compactación: Esperando fin de operaciones de FS");
				sem_wait(&resourceAccess);// verifico q no haya procesos haciedno fread_fwrite
				stream_send_empty_buffer(kernel_configs->socket_memoria, hs_aux); //le avisa a la memoria q puede proceder.
				int sizeTabla;
				t_buffer* sizeTablaBuffer = buffer_create();
				log_info(kernelLogger2, "Compactación: Se solicita compactacion");
				stream_recv_buffer(kernel_configs->socket_memoria, sizeTablaBuffer); //aca para esperando la rta de memoria
				buffer_unpack(sizeTablaBuffer, &sizeTabla, sizeof(sizeTabla));
				buffer_destroy(sizeTablaBuffer);
				actualizarPorCompactacion(sizeTabla);
				log_info(kernelLogger2, "Se finalizo la compactacion");
				sem_post(&resourceAccess);
				consultar_memoria_create(rtaMemoria, pcb);
	} else if(rtaMemoria == header_general){

		        //tablaSegmentos = recive_segmento_nuevo();
				// esta es la nueva tabla de segmentos
		        log_info(kernelLogger2, "se encontro lugar para un segmento");
				pcb->tabla_de_segmentos = tablaSegmentos;
				sem_post(get_e_semaforos(e_execute));
	}
}

/*
tabla_segmento* recive_segmento_nuevo(){
	tabla_segmento* tablaSegmentos;
	t_buffer* rtaMemoria = buffer_create();
	stream_recv_buffer(kernel_configs->socket_memoria, rtaMemoria);
	buffer_unpack(rtaMemoria, &tablaSegmentos, sizeof(tablaSegmentos));
	buffer_destroy(rtaMemoria);
	return tablaSegmentos;
}
*/

void* liberar_segmentos_pcb(procesoNuevo* aux){
	stream_send_empty_buffer( kernel_configs->socket_memoria,I_EXIT);

	t_buffer* pcbBuffer = buffer_create();
	buffer_pack(pcbBuffer, aux , sizeof(aux));
	stream_send_buffer(kernel_configs->socket_memoria, header_resultado, pcbBuffer);
	buffer_destroy(pcbBuffer);


	int memoriaRespuesta = stream_recv_header(kernel_configs->socket_memoria);
	stream_recv_empty_buffer(kernel_configs->socket_memoria);
	if(memoriaRespuesta != hs_aux){
		log_error(kernelLogger2, "error en la eliminacion de los segmentos del proceso %d ", aux->pid );
		kernel_destruir(kernel_configs, kernelLogger2);
		exit(-1);
		}
	//espero su resultado
}

void* actualizarPorCompactacion(int sizeTabla){
						t_list* tablaSegmentosCompactada;
						t_buffer* compactacion = buffer_create();
						stream_recv_buffer(kernel_configs->socket_memoria, compactacion);
						buffer_unpack(compactacion, &tablaSegmentosCompactada, sizeof(tablaSegmentosCompactada));
						buffer_destroy(compactacion);
						for(int i = 0; i< sizeTabla; i++){
						tabla_segmento* segmentoAux = list_get(tablaSegmentosCompactada, i);
						 //actualizarSegmentosPorEstado(e_new, segmentoAux); creo que este no hace falta pq todavia no tiene segmentos asignados
						 actualizarSegmentosPorEstado(e_blocked,segmentoAux);
						 actualizarSegmentosPorEstado(e_execute, segmentoAux);
						 actualizarSegmentosPorEstado(e_ready,segmentoAux);
						}
					// esta es la nueva tabla de segmentos
					//pcb->tabla_de_segmentos = tablaSegmentos; esto tiene que ser solo los segmentos de este pcb
}


void* actualizarSegmentosPorEstado(estado_proceso* e, tabla_segmento* segmentoAux){
	t_list* lista_de_pcbs = get_e_lista(e);
	for(int i = 0; i< list_size(lista_de_pcbs); i++){
			procesoNuevo* pcb_analizado = list_get(lista_de_pcbs, i);
			t_list* segmentos_del_pcb = pcb_analizado->tabla_de_segmentos;

			segmento* segmentoEncontrado = findSegmentoConId(segmentos_del_pcb, segmentoAux->id);
			if(segmentoAux->pid == pcb_analizado->pid){
				segmentoEncontrado->base = segmentoAux->base;
				segmentoEncontrado->limite = segmentoAux->limite;
				list_replace_by_condition(segmentos_del_pcb, (void*)mismoSegmento , segmentoEncontrado);
				list_clean(pcb_analizado->tabla_de_segmentos);
				pcb_analizado->tabla_de_segmentos = segmentos_del_pcb;
				list_replace_by_condition(lista_de_pcbs,(void*)mismoProceso, pcb_analizado);
				e->listaDeProcesos = lista_de_pcbs;
			}
	}
}

bool mismoProceso(procesoNuevo* pcb1, procesoNuevo* pcb2){
	return pcb1->pid == pcb2->pid;
}
bool mismoSegmento(segmento* seg1, segmento* seg2){
	return seg1->id == seg2->id;
}

void creacionArchivoFileSystem(procesoNuevo* pcb, rtaKernelFs* archivo){
	/*stream_send_empty_buffer(kernel_configs->socket_filesystem, header_nuevo_archivo_filesystem);
	headers rtaFS = stream_recv_header(kernel_configs->socket_filesystem);
		if(rtaFS != hs_aux){
			log_error(kernelLogger, "error en la creacion del archivo en file system en el pcb %d", pcb->pid);
		}*/
	enviar_archivo_fs(archivo, I_CREATE_ARCHIVO);
	headers rtaFS = stream_recv_header(kernel_configs->socket_filesystem);
	stream_recv_empty_buffer(kernel_configs->socket_filesystem);
	if(rtaFS != hs_aux){
		log_error(kernelLogger2, "error en la creacion del archivo %s en file system en el pcb %d",archivo->nombre_archivo, pcb->pid);
	}
	else{
		log_info(kernelLogger2, "archivo %s solicitado por %d creado exitosamente",archivo, pcb->pid);
	}
}




void* stream_archivo_fs_consulta(char* archivo){
	t_buffer* archivoBuffer = buffer_create();
	buffer_pack(archivoBuffer, archivo, sizeof(archivo));
	stream_send_buffer(kernel_configs->socket_filesystem,header_general,archivoBuffer);
	buffer_destroy(archivoBuffer);
}


/*
t_list* recibir_tabla_actualizada(){
	t_list* tabla_actualizada;
	t_buffer* tablaSegmentos = buffer_create();
	stream_send_buffer(kernel_configs->socket_memoria,header_general,tablaSegmentos);
	buffer_unpack(tablaSegmentos, &tabla_actualizada, sizeof(tabla_actualizada));
	buffer_destroy(tablaSegmentos);
}

void* stream_tabla_actualizada(int* socket, t_list* tabla){
	t_buffer* tablaSegmentosBuffer = buffer_create();
	buffer_pack(tablaSegmentosBuffer, tabla, sizeof(tabla));
	stream_send_buffer(socket,header_general,tablaSegmentosBuffer);
	buffer_destroy(tablaSegmentosBuffer);
}*/

void* enviar_archivo_fs(rtaKernelFs* dato, instruccion instruc){
	t_buffer* bufferArchivo = buffer_create();
	buffer_pack(bufferArchivo, &dato->direccionFisica, sizeof(int));
	buffer_pack(bufferArchivo, &dato->pid, sizeof(int));
	buffer_pack(bufferArchivo, &dato->puntero, sizeof(uint32_t));
	buffer_pack(bufferArchivo, &dato->size, sizeof(int));
	buffer_pack_string(bufferArchivo, dato->nombre_archivo);
	stream_send_buffer(kernel_configs->socket_filesystem, instruc, bufferArchivo);
	log_info(kernelLogger2, "mande buffer a fs");
	buffer_destroy(bufferArchivo);
}

void * f_read_write(procesoNuevo* pcb, rtaKernelFs* envioFs, instruccion instruc){

	enviar_archivo_fs(envioFs, instruc);
	pcb->estado_actual = BLOCKED;
	logBlocked(pcb->pid,"IO");
	push_pcb_estado(e_blocked, e_execute, pcb);
	 //semaforo para la compactacion
	//sem_post(&execute_libre);
	//sem_post(get_e_semaforos(e_execute));
}

void logBlocked(int pid, char* motivoBloqueo){
	log_info(kernelLogger2, "PID: %d - Bloqueado por: %s",pid ,motivoBloqueo);
}

void logReady(){
	 int numProcesos = list_size(e_ready->listaDeProcesos);

	    // Allocate memory for the pids string
	    char* pids = string_new();

	    for (int i = 0; i < numProcesos; i++) {
	        procesoNuevo* aux = list_get(e_ready->listaDeProcesos, i);
	        char* pid_str = string_itoa(aux->pid);
	        string_append(&pids, pid_str);
	    }
	log_info(kernelLogger2, "Cola Ready %s : [ %s ] ", kernel_configs->algoritmo_planificacion, pids);
}
/*
void pack_registros(t_buffer* bufferEjecutar,cpuResponse* cpu_enviar){
	buffer_pack(bufferEjecutar, cpu_enviar->registros->AX, sizeof(cpu_enviar->registros->AX));
	buffer_pack(bufferEjecutar, cpu_enviar->registros->BX, sizeof(cpu_enviar->registros->BX));
	buffer_pack(bufferEjecutar, cpu_enviar->registros->CX, sizeof(cpu_enviar->registros->CX));
	buffer_pack(bufferEjecutar, cpu_enviar->registros->DX, sizeof(cpu_enviar->registros->DX));

	buffer_pack(bufferEjecutar, cpu_enviar->registros->EAX, sizeof(cpu_enviar->registros->EAX));
	buffer_pack(bufferEjecutar, cpu_enviar->registros->EBX, sizeof(cpu_enviar->registros->EBX));
	buffer_pack(bufferEjecutar, cpu_enviar->registros->ECX, sizeof(cpu_enviar->registros->ECX));
	buffer_pack(bufferEjecutar, cpu_enviar->registros->EDX, sizeof(cpu_enviar->registros->EDX));

	buffer_pack(bufferEjecutar, cpu_enviar->registros->RAX, sizeof(cpu_enviar->registros->RAX));
	buffer_pack(bufferEjecutar, cpu_enviar->registros->RBX, sizeof(cpu_enviar->registros->RBX));
	buffer_pack(bufferEjecutar, cpu_enviar->registros->RCX, sizeof(cpu_enviar->registros->RCX));
	buffer_pack(bufferEjecutar, cpu_enviar->registros->RDX, sizeof(cpu_enviar->registros->RDX));
}*/


