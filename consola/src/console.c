#include "console.h"
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#define PATH_CONFIG "consola.config"
#define PATH_LOG_CONSOLA "consola.log"
#define PATH_INSTRUCCIONES "pseudocodigo.txt"

struct consola_config{
	char* IP_KERNEL;
	char* PUERTO_KERNEL;
};

int conexion;

 t_log* logger_consola;


int main(int argc, char *argv[]) {

	consola_config* config;


	logger_consola = log_create(PATH_LOG_CONSOLA,"CONSOLA",true, LOG_LEVEL_INFO);
	config = iniciar_config(logger_consola);

	char *ipKernel = get_ip_kernel_consola(config);
	char *puertoKernel = get_puerto_kernel_consola(config);

	int kernelSocket = conectar_a_servidor(ipKernel, puertoKernel);

	log_info(logger_consola,"Estoy en consola");
	stream_send_empty_buffer(kernelSocket, hs_consola);
	log_info(logger_consola,"Se envio handshadshake de consola a kernel");
	handsakes kernelResponse = stream_recv_header(kernelSocket); //hasta aca llegue. 6/2/2023 11pm
	stream_recv_empty_buffer(kernelSocket);
	if(kernelResponse!= hs_aux){
		return -1; //no se produce el hand shake
	}
	log_info(logger_consola,"EL Kernel y la nueva Consola se han identificado, comenzando envio de instrucciones!");
	send_instruccion_consola_kernel(kernelSocket);

	t_buffer *bufferFinal = buffer_create();
	char* resultadoPcb;
	stream_recv_buffer(kernelSocket, bufferFinal);
	buffer_unpack(bufferFinal, &resultadoPcb, sizeof(resultadoPcb));
	buffer_destroy(bufferFinal);
	log_info(logger_consola, "El resultado de la operacion fue: %s", resultadoPcb); //espera a la respuesta final del kernel

	terminar_programa(conexion, logger_consola, config);

}

 void send_instruccion_consola_kernel(int socket){
	log_info(logger_consola,"Estoy en send_instruccion_consola_kernel"); //llega hasta aca
	t_buffer *buffer_send = buffer_create();
	parsear_pseudocodigo(buffer_send, PATH_INSTRUCCIONES, logger_consola);
	log_info(logger_consola,"se parseo las instrucciones");

	stream_send_buffer(socket, header_lista_instrucciones, buffer_send);
	log_info(logger_consola,"se mando las instrucciones a kernel");

	buffer_destroy(buffer_send);
}

static void consola_config_iniciador(void* modulo, t_config* config){
	consola_config* consola_configs = (consola_config*)modulo;
	consola_configs->IP_KERNEL = strdup(config_get_string_value(config, "IP_KERNEL"));
	consola_configs->PUERTO_KERNEL = strdup(config_get_string_value(config, "PUERTO_KERNEL"));

}

consola_config* iniciar_config(t_log* logger_consola)
{
	consola_config* aux = malloc(sizeof(*aux));
	config_init(aux, PATH_CONFIG, logger_consola, consola_config_iniciador);
	return aux;
}

char* get_ip_kernel_consola(consola_config* aux){
	return aux->IP_KERNEL;
}
char* get_puerto_kernel_consola(consola_config* aux){
	return aux->PUERTO_KERNEL;
}

void terminar_programa(int conexion, t_log* logger_consola, t_config* config)
{
	log_destroy(logger_consola);
	config_destroy(config);
	liberar_conexion(conexion);
	/* Y por ultimo, hay que liberar lo que utilizamos (conexion, log y config)
	  con las funciones de las commons y del TP mencionadas en el enunciado */
}







