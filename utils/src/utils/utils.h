#ifndef UTILS_H_
#define UTILS_H_

#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include <semaphore.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include "flags.h"
#include "buffer.h"
#include <commons/temporal.h>


typedef enum
{
	MENSAJE,
	PAQUETE
}op_code;

typedef struct{
	int size;
	int direccionFisica;
	char* nombre_archivo;
	int pid;
	uint32_t puntero;
} rtaKernelFs;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;



typedef struct {

	char AX[4], BX[4], CX[4], DX[4];
	char EAX[8], EBX[8], ECX[8], EDX[8];
	char RAX[16], RBX[16], RCX[16], RDX[16];
}registros_cpu;

typedef struct {
	uint32_t pid;
	uint32_t pc;
	registros_cpu* registros;
	t_list* lista_instrucciones;
}cpu_atributos;


typedef enum{
	NEW,
	READY,
	EXECUTE,
	EXIT,
	BLOCKED,
	PBC_ESPERANDO_IO,
} bandera_estado;

typedef struct
{
	//bandera_estado nombreEstado;
	char* nombre;
	t_list* listaDeProcesos;
	sem_t* semaforoEstado;
	pthread_mutex_t* mutexEstado;
}estado_proceso;

typedef struct{
	int pid;
	instruccion flag;
	registros_cpu* registros;
	uint32_t pc;
	char* archivo;
	int tiempo;
	char* recurso;
	int sizeSegmento; //para create_segment (se puede cambiar esto) aca tambien paso el size del fread
	int id_segmento; //para delete_segment (se puede cambiar esto)
	t_list* lista_instrucciones;
	t_list* tabla_segmentos;
	int posicionPuntero;
	int direccionFisica;
} cpuResponse;



typedef struct
{
	char* ip_memoria;
	char* puerto_memoria;
	char* puerto_escucha;
	char* tam_max_segmento;
	int retardo_instruccion;
	int socket_kernel;
	int socket_memoria;
} cpu_config;

typedef struct{
	int id;
	int base;
	int limite;

}segmento;

typedef struct {
	char* ip_memoria;
	char* puerto_memoria;
	char* puerto_escucha;
	char* path_superbloque;
	char* path_bitmap;
	char* path_bloques;
	char* path_FCB;
	int retardo_acceso_bloque;
	int socketMemoria;
	int socketKernel;
	int socketFS;
	int socketFSBlocked;
}filesystem_config;

typedef struct
{
	tipo_instruccion_consola instruccion_nombre;
	char* parametro1;
	char* parametro2;
	char* parametro3;

} instrucciones;

typedef struct{
	int id;
	int pid;
	int idSegPcb;
	int limite;
	int base;
} tabla_segmento;

typedef struct{
	char* archivo;
	pthread_mutex_t* mutexArchivo;
	t_list* pcb_en_espera;
	int posicion_puntero;
	//completar Filesystem
}tabla_de_archivos;

typedef struct{
	int pid;
	t_list* lista_instrucciones; // tipo de datos instrucciones en consola
	int program_counter;
	registros_cpu* registro_cpu;
	t_list* tabla_de_segmentos;
	float rafaga_estimada;
	t_temporal* tiempo_de_llegada;
	int size;
	t_list* tabla_de_archivos;
	bandera_estado estado_actual;
	float rafaga_real;
	int* socketProceso;//usado para el hrrn, donde guardamos el valor "real" que se ejecuto
	char* resultadoProceso;
}procesoNuevo;



int crear_conexion(char* ip, char* puerto);
void enviar_mensaje(char* mensaje, int socket_cliente);
t_paquete* crear_paquete(void);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void liberar_conexion(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);

#define PUERTO "4444"

extern t_log* logger;

void* recibir_buffer(int*, int);
void empaquetar_lista(t_list* lista, int longitud, void* tipodato, t_buffer* buffer, t_log* cpu_logger);
int iniciar_servidor(char*);
//int esperar_cliente(int);
t_list* recibir_paquete(int);
//void recibir_mensaje(int);
int recibir_operacion(int);
void* recibir_buffer(int* size, int socket_cliente);
int config_init(void* moduleConfig, char* pathToConfig, t_log* moduleLogger, void (*config_initializer)(void* moduleConfig, t_config* tempConfig));
int conectar_a_servidor(char* ip, char* port);
int handshake(int*, handsakes, char*, t_log* );
int aceptar_conexion_server(int socket);
void stream_send_empty_buffer(int toSocket, uint8_t header);
void stream_recv_buffer(int fromSocket, t_buffer* destBuffer);
uint8_t stream_recv_header(int fromSocket);
void stream_recv_empty_buffer(int fromSocket);
cpuResponse stream_recv_cpu_response(int fromSocket);
tabla_segmento* findSegmentoConId(t_list* tablaSegmentos, int id);
t_list* obtener_lista_instrucciones(t_buffer*, t_log*);
instrucciones* armar_instruccion(tipo_instruccion_consola, char[50], char[50], char[50]);
void stream_send_buffer(int, uint8_t, t_buffer*);
tabla_de_archivos* crear_tabla_de_archivos(char* nombre_archivo);
void free_tabla_de_archivos(tabla_de_archivos* archivo);
cpuResponse* desempaquetar_cpu_response(t_buffer* bufferCpu, t_log* cpu_logger);
t_list* desempaquetar_lista(t_buffer* buffer,int longitud,void* tipo_dato, t_log* cpu_logger);
void empaquetar_instruccion(instrucciones* instruc, t_buffer* buffer);
t_list* desempaquetar_lista_instrucciones(t_buffer* buffer,int longitud, t_log* cpu_logger);
int cuantosParametros(tipo_instruccion_consola instruccion_nombre);
void empaquetar_lista_instrucciones(t_buffer* buffer,int longitud,t_list* listaInstrucciones, t_log* cpu_logger);
void empaquetar_lista_segmentos( t_buffer* buffer, int longitud, t_list* tablaDeSegmentos, t_log* logger);
t_list* desempaquetar_lista_segmentos(t_buffer* buffer,int longitud, t_log* logger);
tabla_segmento* desempaquetar_segmento(t_buffer* buffer, t_log* logger);
void empaquetar_segmento(t_buffer* buffer, tabla_segmento* seg);
#endif /* UTILS_H_ */
