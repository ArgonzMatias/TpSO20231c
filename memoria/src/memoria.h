#ifndef MEMORIA_H_
#define MEMORIA_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include "utils/utils.h"
#include "utils/flags.h"
#include "pthread.h"
#include<commons/log.h>
#include<commons/collections/list.h>
#include <commons/bitarray.h>
#include "segmentacion.h"
#include "compact.h"
#include <time.h>
#include "memoria_solicitudes.h"
#include"utils/buffer.h"

typedef struct {
	char* puerto_escucha;
	int tam_memoria;
	int tam_segmento_0;
	int cant_segmentos;
	int retardo_memoria;
	int retardo_compactacion;
	char* algoritmo_asignacion;
	int socket_cpu;
	int socket_filesystem;
	int socket_kernel;
}memoria_config;



char* asignarMemoriaBytes(int bytes);
char* asignarMemoriaBites(int bits);
char* inicializacion_memoria();
 t_list* get_tabla_segmentos();
 void aumentar_prox_id();


#endif /* MEMORIA_H_ */
