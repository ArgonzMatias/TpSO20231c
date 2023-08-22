#ifndef KERNEL_H_
#define KERNEL_H_

#include "utils/utils.h"
#include "utils/flags.h"
#include "estado.h"
#include "planificador.h"
#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <readline/readline.h>
#include <semaphore.h>
#include<string.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<string.h>
#include<assert.h>
#include"utils/buffer.h"
#include <commons/temporal.h>



typedef struct {
	char* ip_memoria;
	char* ip_filesystem;
	char* ip_cpu;
	char* algoritmo_planificacion;
	char* puerto_memoria;
	char* puerto_filesystem;
	char* puerto_cpu;
	char* puerto_escucha;
	int estmacion_inicial;
	float hrrn_alfa;
	int grado_max_multiprogramacion;
	int socket_cpu;
	int socket_memoria;
	int socket_filesystem;
	int socket_fs_unblocked;
	char** recurso; //array de recursos
	int* instancias_recursos;
}kernel_config;

struct threadArg {
	int socket;
	t_log* logger;
};



void iterator(char* value);
t_config* iniciar_config(void);
void leer_consola(t_log*);
void paquete(int);
void terminar_programa(int, int,int, t_log*, t_config*);
void* admin_threads_cliente(int server_fd);
procesoNuevo* recibir_instrucciones_consola(int socket_cliente);
void kernel_config_destroy(kernel_config* aux);
void nueva_conex(int socketEscucha);
int iniciar_servidor_kernel(char* ip, char* puerto);
kernel_config* kernel_config_crear();
void kernel_destruir(kernel_config* kernelConf, t_log* kernelLogger);
t_log* get_logger();

#endif
