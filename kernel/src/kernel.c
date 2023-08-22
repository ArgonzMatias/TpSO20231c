#include "kernel.h"
#include "pthread.h"

#define IP_KERNEL "127.0.0.1"
#define PATH_CONFIGS "kernel.cfg"


kernel_config* kernel_configs;
t_log* kernelLogger;
tabla_segmento* segmento_zero;
int max_size_segmento;



char* kernel_get_puerto_escucha(kernel_config* aux){
	return aux->puerto_escucha;
}

static void crear_hilo_conex_entrante(int* socket){
	pthread_t thread1;
	pthread_create(&thread1, NULL, nuevo_pcb_new, (void*) socket);
	pthread_detach(thread1);
}



void kernel_destruir(kernel_config* kernelConf, t_log* kernelLogger){
	kernel_config_destroy(kernel_configs);
	log_destroy(kernelLogger);
}

void kernel_config_destroy(kernel_config* aux){
	free(aux->algoritmo_planificacion);
	free(aux->instancias_recursos);
	free(aux->ip_cpu);
	free(aux->ip_filesystem);
	free(aux->ip_memoria);
	free(aux->puerto_cpu);
	free(aux->puerto_escucha);
	free(aux->puerto_filesystem);
	free(aux->puerto_memoria);
	free(aux->recurso);
	free(aux);
}





int main(int argc, char* argv[]) {

	kernelLogger = log_create("bin/kernel.log", "Kernel", true, LOG_LEVEL_DEBUG);
	kernel_configs = kernel_config_crear(PATH_CONFIGS);


	//Conexion con cpu

	char* ip_cpu = kernel_configs->ip_cpu;
	char* puerto_cpu = kernel_configs->puerto_cpu;

	const int socketCPU = conectar_a_servidor(ip_cpu, puerto_cpu);

	if (socketCPU == -1) {
			log_error(kernelLogger, "error socket en coneccion de cpu con kernel");
			kernel_destruir(kernel_configs, kernelLogger);
			exit(-1);
	}

	kernel_configs->socket_cpu = socketCPU;
	stream_send_empty_buffer(socketCPU, hs_cpu);
	handsakes cpuRta = stream_recv_header(socketCPU);
	stream_recv_empty_buffer(socketCPU);
	if(cpuRta != hs_aux){
		log_error(kernelLogger, "error handshake conectando cpu con kernel ");
		kernel_destruir(kernel_configs, kernelLogger);
		exit(-1);
	}
	//log_info(kernelLogger, "conexion con kernel con cpu exitosa");

	//Conexion con memoria
	//para que anden las conexxiones con memoria y cpu, tienen que estar esos servidores levantados.

		int socketMemoria = conectar_a_servidor(kernel_configs->ip_memoria, kernel_configs->puerto_memoria);
		if (socketMemoria == -1) {
				log_error(kernelLogger, "error socket en coneccion de memoria con kernel");
				kernel_destruir(kernel_configs, kernelLogger);
				exit(-1);
		}

		kernel_configs->socket_memoria = socketMemoria;
		stream_send_empty_buffer(socketMemoria, hs_kernel);
		handsakes memoriaRta = stream_recv_header(socketMemoria);
		stream_recv_empty_buffer(socketMemoria);
		if(memoriaRta != hs_aux){
			log_error(kernelLogger, "error handshake conectando memoria con kernel ");
			kernel_destruir(kernel_configs, kernelLogger);
			exit(-1);
		}
		//log_info(kernelLogger, "conexcion con kernel con memoria exitosa");


		//Conexion con filesystem
			//para que anden las conexxiones con memoria y cpu, tienen que estar esos servidores levantados.

			const int socketFileSystem = conectar_a_servidor(kernel_configs->ip_filesystem, kernel_configs->puerto_filesystem);
				if (socketFileSystem == -1) {
						log_error(kernelLogger, "error socket en coneccion de filesystem con kernel");
						kernel_destruir(kernel_configs, kernelLogger);
						exit(-1);
				}

				kernel_configs->socket_filesystem = socketFileSystem;
				stream_send_empty_buffer(socketFileSystem, hs_filesystem);
				handsakes fsRta = stream_recv_header(socketFileSystem);
				stream_recv_empty_buffer(socketFileSystem);
						if(fsRta != hs_aux){
							log_error(kernelLogger, "error handshake conectando fs con kernel ");
							kernel_destruir(kernel_configs, kernelLogger);
							exit(-1);
				}
				const int socketFileSystemBlocked = conectar_a_servidor(kernel_configs->ip_filesystem, kernel_configs->puerto_filesystem);
				kernel_configs->socket_fs_unblocked = socketFileSystemBlocked;
				stream_send_empty_buffer(socketFileSystemBlocked, hs_fs_blocked);
								handsakes fsRtaBlocked = stream_recv_header(socketFileSystemBlocked);
								stream_recv_empty_buffer(socketFileSystemBlocked);
										if(fsRtaBlocked != hs_aux){
											log_error(kernelLogger, "error handshake conectando fs con kernel ");
											kernel_destruir(kernel_configs, kernelLogger);
											exit(-1);
								}
				//handshake(&kernel_configs->socket_filesystem, hs_aux, "filesystem", kernelLogger);

				log_info(kernelLogger, "conexcion con kernel con memoria exitosa");

				headers headMem = stream_recv_header(socketMemoria);
				if(headMem != header_general){
					log_error(kernelLogger, "agarro mal el header");
				}
				stream_recv_empty_buffer(kernel_configs->socket_memoria);
//// recibo el segmento zero
				t_buffer* buffer_segmento_zero = buffer_create();
				segmento_zero = malloc(sizeof(tabla_segmento));
				memoriaRta = stream_recv_header(socketMemoria);
				if(memoriaRta != hs_kernel){
					log_error(kernelLogger, "agarro mal el header");
				}
				stream_recv_buffer(kernel_configs->socket_memoria, buffer_segmento_zero);
				segmento_zero = desempaquetar_segmento(buffer_segmento_zero, kernelLogger);
				log_info(kernelLogger, "limite segmento: %d", segmento_zero->limite);
				buffer_destroy(buffer_segmento_zero);
// recibo cantidad maxima de segmentos
				t_buffer* buffer_max_seg = buffer_create();
				memoriaRta = stream_recv_header(socketMemoria);
				if(memoriaRta != hs_aux){
					log_error(kernelLogger, "agarro mal el header");
				}
				stream_recv_buffer(kernel_configs->socket_memoria, buffer_max_seg);
				buffer_unpack(buffer_max_seg, &max_size_segmento, sizeof(max_size_segmento));
				buffer_destroy(buffer_max_seg);
	//recibo la tabla de segmentos iniciales para mandarselo a filesystem y memoria.

			t_buffer* buffer_tabla_segmentos = buffer_create();
			memoriaRta = stream_recv_header(socketMemoria);
			if(memoriaRta != hs_aux){
				log_error(kernelLogger, "agarro mal el header");
			}
			stream_recv_buffer(kernel_configs->socket_memoria, buffer_tabla_segmentos);

			buffer_destroy(buffer_tabla_segmentos);
	// Servidor con consola

		int socketKernel = iniciar_servidor(kernel_configs->puerto_escucha);



		//log_info(kernelLogger,"servidor_iniciado");
		if(socketKernel == -1){
			log_error(kernelLogger, "no se pudo iniciar el servidor kernel");
			kernel_destruir(kernel_configs, kernelLogger);
			exit(-1);
		}
		inicializar_planificador();
		log_info(kernelLogger, "planificacion iniciado");


		nueva_conex(socketKernel);

		return 0;
		}

void nueva_conex(int socketEscucha){

	log_info(kernelLogger, "escuchando clientes");
	for(;;){
	int clienteElegido = aceptar_conexion_server(socketEscucha);
	if(clienteElegido > -1){
		int* socketCliente = malloc(sizeof(*socketCliente));
		*socketCliente = clienteElegido;
		crear_hilo_conex_entrante(socketCliente);
	} else{
		log_error(kernelLogger, "error en establecer conexion");
	}
}}


int iniciar_servidor_kernel(char* ip, char* puerto){
			int optVal = 1;
			int socketKernel;
			struct addrinfo hints;
			struct addrinfo*servinfo;

			memset(&hints, 0, sizeof(hints));
				hints.ai_family = AF_INET;
				hints.ai_socktype = SOCK_STREAM;
				getaddrinfo(ip,puerto, &hints, &servinfo);

				// Creamos el socket de escucha del servidor
				socketKernel = socket(servinfo->ai_family,
											 servinfo->ai_socktype,
											 servinfo->ai_protocol);
				setsockopt(socketKernel, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal));

				// Asociamos el socket a un puerto
				bind(socketKernel, servinfo->ai_addr, servinfo->ai_addrlen);

				// Escuchamos las conexiones entrantes
				listen(socketKernel, SOMAXCONN);

				freeaddrinfo(servinfo);
				return socketKernel;
}

static void kernel_config_iniciar(void* moduleConfig, t_config* config){
	kernel_config* kernelConfig = (kernel_config*)moduleConfig; //Logre arreglarlo parece
	kernelConfig->ip_memoria = strdup(config_get_string_value(config, "IP_MEMORIA"));
	kernelConfig->ip_cpu = strdup(config_get_string_value(config, "IP_CPU"));
	kernelConfig->ip_filesystem = strdup(config_get_string_value(config, "IP_FILESYSTEM"));
	kernelConfig->algoritmo_planificacion = strdup(config_get_string_value(config, "ALGORITMO_PLANIFICACION"));
	kernelConfig->estmacion_inicial = config_get_int_value(config, "ESTIMACION_INICIAL");
	kernelConfig->grado_max_multiprogramacion = config_get_int_value(config, "GRADO_MAX_MULTIPROGRAMACION");
	kernelConfig->hrrn_alfa = config_get_double_value(config, "HRRN_ALFA");
	kernelConfig->puerto_cpu = strdup(config_get_string_value(config, "PUERTO_CPU"));
	kernelConfig->puerto_escucha = strdup(config_get_string_value(config, "PUERTO_ESCUCHA"));
	kernelConfig->puerto_filesystem = strdup(config_get_string_value(config, "PUERTO_FILESYSTEM"));
	kernelConfig->puerto_memoria = strdup(config_get_string_value(config, "PUERTO_MEMORIA"));
	kernelConfig->recurso = config_get_array_value(config, "RECURSOS"); // esto nose si estara bien
	char** aux = config_get_array_value(config, "INSTANCIAS_RECURSOS");
	int size = 0;
	while(aux[size]!= NULL){
		size++;
	}
	kernelConfig->instancias_recursos = (int*)malloc(size * sizeof(int));
	for(int i= 0; i<size; i++){
		kernelConfig->instancias_recursos[i] = atoi(aux[i]);
	}
	kernelConfig->socket_cpu = -1;
	kernelConfig->socket_memoria = -1;
	kernelConfig->socket_fs_unblocked = -1;
	kernelConfig->socket_filesystem = -1;
}
kernel_config* kernel_config_crear(char* path){
	kernel_config* config_aux = malloc(sizeof(*config_aux));
	config_init(config_aux, path, kernelLogger, kernel_config_iniciar); //hay que averiguar como hacer para que levante esto por argumento
	return config_aux;
}

t_log* get_logger(){
	return kernelLogger;
}


