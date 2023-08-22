#include "memoria.h"



#define PATH_LOG_MEMORIA "memoria.log"
#define PATH_CONFIG_MEMORIA "memoria.config"



 t_log* logger_memoria;
 memoria_config* memoria_configs;
 char* memoria_Principal; //toda la memoria general
 char* datos;
 t_bitarray* bitMapSegment;
 int idSegmentos;

t_list* tablaSegmentos;

static void memoria_config_iniciar(void* moduleConfig, t_config* config){
	memoria_config* memoriaConfig = (memoria_config*)moduleConfig; //Logre arreglarlo parece
	memoriaConfig->puerto_escucha = strdup(config_get_string_value(config, "PUERTO_ESCUCHA"));;
	memoriaConfig->tam_memoria = config_get_int_value(config, "TAM_MEMORIA");
	memoriaConfig->tam_segmento_0 = config_get_int_value(config, "TAM_SEGMENTO_0");
	memoriaConfig->cant_segmentos = config_get_int_value(config, "CANT_SEGMENTOS");
	memoriaConfig->retardo_memoria = config_get_int_value(config, "RETARDO_MEMORIA");
	memoriaConfig->retardo_compactacion = config_get_int_value(config, "RETARDO_COMPACTACION");
	memoriaConfig->algoritmo_asignacion = config_get_string_value(config, "ALGORITMO_ASIGNACION");
	memoriaConfig->socket_cpu = -1;
	memoriaConfig->socket_kernel = -1;
	memoriaConfig->socket_filesystem = -1;
}

int main(int argc, char *argv[]) {
	logger_memoria = log_create(PATH_LOG_MEMORIA, "MEMORIA", 1, LOG_LEVEL_DEBUG);
	memoria_configs = malloc(sizeof(*memoria_configs));

	config_init(memoria_configs, PATH_CONFIG_MEMORIA, logger_memoria, memoria_config_iniciar);

	int socketMemoria = iniciar_servidor(memoria_configs->puerto_escucha);
	if(socketMemoria == -1){
				log_error(logger_memoria, "no se pudo iniciar el servidor kernel");
				memoria_destruir(memoria_configs, logger_memoria);
				exit(-1);
	}

	memoria_Principal = inicializacion_memoria();


		  //hace una conexion con kernel, cpu y filesystem
		int clienteCpu = aceptar_conexion_server(socketMemoria);
		handshake(&clienteCpu, hs_cpu , "cpu", logger_memoria);
		memoria_configs->socket_cpu = clienteCpu;

		pthread_t hilo1;
		pthread_create(&hilo1, NULL, (void*) peticiones_cpu, (void*)clienteCpu);
		pthread_detach(&hilo1);

		int clienteFileSystem = aceptar_conexion_server(socketMemoria);
		handshake(&clienteFileSystem, hs_filesystem , "file system", logger_memoria);
		memoria_configs->socket_filesystem = clienteFileSystem;

		pthread_t hilo2;
		pthread_create(&hilo2, NULL, (void*) peticiones_fileSystem, (void*)clienteFileSystem);
		pthread_detach(&hilo2);

		int clienteKernel = aceptar_conexion_server(socketMemoria);
		handshake(&clienteKernel, hs_kernel, "kernel", logger_memoria);
		memoria_configs->socket_kernel = clienteKernel;

		segmento_zero();
		log_info(logger_memoria,"mande segmento zero");
		cant_max_por_pcb();
		log_info(logger_memoria,"mande cant max por pcb");

		t_buffer* buffer_kernel_inicial = buffer_create();
		int tam_lista_segmentos = list_size(tablaSegmentos);
		buffer_pack(buffer_kernel_inicial, &tam_lista_segmentos, sizeof(int));

		empaquetar_lista_segmentos(buffer_kernel_inicial, tam_lista_segmentos,tablaSegmentos, logger_memoria);
		stream_send_buffer(memoria_configs->socket_kernel,hs_aux,buffer_kernel_inicial);
		log_info(logger_memoria,"mande tabla segmentos");
		buffer_destroy(buffer_kernel_inicial);

		pthread_t hilo3;
		pthread_create(&hilo3, NULL, (void*) peticiones_kernel, (void*)clienteKernel);
		pthread_join(hilo3, NULL);


return 0;


}

char* inicializacion_memoria(void) {
	//separo el tamanio de la memoria total
			char* inicio_de_memoria = malloc(sizeof(memoria_configs->tam_memoria));
			if(inicio_de_memoria == NULL){
				log_error(logger_memoria, "error en la creacion inicial de memoria");
			}

			//creo la tabla de segmentos

			tablaSegmentos = list_create();

			//bit array

			datos = asignarMemoriaBytes(memoria_configs->tam_memoria);
			if(datos == NULL){
				log_error(logger_memoria, "error en la creacion inicial deL bit array");
			}

			int size = bitsToBytes(memoria_configs->tam_memoria);

			bitMapSegment = bitarray_create_with_mode(datos, size, MSB_FIRST);

			idSegmentos = 0;

		//inicio memoria
			return inicio_de_memoria;

}

void simulador_de_espera(int espera){
	const int milisecs = 1000;
	const milisec_nanosecs = 1000000;
	struct timespec timeSpec;
	timeSpec.tv_sec = espera / milisecs;
	timeSpec.tv_nsec = (espera / milisecs) * milisec_nanosecs;
	nanosleep(&timeSpec, &timeSpec);

}


void memoria_destruir(memoria_config* memoriaConf, t_log* logger){
	memoria_config_destroy(memoriaConf);
	log_destroy(logger);
}

void memoria_config_destroy(memoria_config* aux){
	free(aux->algoritmo_asignacion);
	free(aux->cant_segmentos);
	free(aux->puerto_escucha);
	free(aux->retardo_compactacion);
	free(aux->retardo_memoria);
	free(aux->socket_cpu);
	free(aux->socket_filesystem);
	free(aux->socket_kernel);
	free(aux->tam_memoria);
	free(aux->tam_segmento_0);
	free(aux);
}

void segmento_zero(){
	tabla_segmento*  segmento_0 = crear_nuevo_segmento(0,memoria_configs->tam_segmento_0, 0); //creo el segmento 0 yo le asigno al segmento 0 por default un valor de pid.
	t_buffer* buffer_zero = buffer_create();
	empaquetar_segmento(buffer_zero,segmento_0);
	//buffer_pack(buffer_zero,&segmento_0, sizeof(tabla_segmento));
	stream_send_buffer(memoria_configs->socket_kernel,hs_kernel,buffer_zero);
	buffer_destroy(buffer_zero);
}

void cant_max_por_pcb(){
	t_buffer* buffer_max_size = buffer_create();
	int size = memoria_configs->cant_segmentos;
	buffer_pack(buffer_max_size,&size, sizeof(size));
	stream_send_buffer(memoria_configs->socket_kernel,hs_aux,buffer_max_size);
	buffer_destroy(buffer_max_size);
}

t_list* get_tabla_segmentos(){
	return tablaSegmentos;
}

int get_proximo_id_segmento(){
	return idSegmentos;
}

void aumentar_prox_id(){
	idSegmentos += 1;
}
