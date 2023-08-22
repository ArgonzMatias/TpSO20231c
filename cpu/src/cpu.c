#include "cpu.h"

#define PATH_CONFIGS "cpu.cfg"


cpu_config* cpu_configs;
t_log* cpu_logger;

bool noMandaContexto(instrucciones* proxima_instruccion){
	tipo_instruccion_consola nombre_inst = proxima_instruccion->instruccion_nombre;
	return (nombre_inst == INSTRUC_MOV_OUT || nombre_inst == INSTRUC_MOV_IN || nombre_inst == INSTRUC_SET);
}

void realizar_instrucciones(){
	while(1){
		log_info(cpu_logger, "por recibir buffer cpu");
	    t_buffer* bufferCpu= buffer_create();
	    headers head = stream_recv_header(cpu_configs->socket_kernel);
	    if(head != header_pcb_execute){
	    	log_error(cpu_logger, "se recibio mal el header");
	    }
	    stream_recv_buffer(cpu_configs->socket_kernel, bufferCpu);
	    instrucciones* proxima_instruccion = malloc(sizeof(*proxima_instruccion));

	    cpuResponse* informacion_instrucciones = desempaquetar_cpu_response(bufferCpu, cpu_logger);
		log_info(cpu_logger, "desempaquete instrucciones");
		buffer_destroy(bufferCpu);
		log_info(cpu_logger, "destrui buffer");
		log_info(cpu_logger, "recibi pid: %d", informacion_instrucciones->pid);
		log_info(cpu_logger, "tamanio lista: %d", list_size(informacion_instrucciones->lista_instrucciones));
		log_info(cpu_logger, "recibi pc: %u", informacion_instrucciones->pc);

		int instruccion = informacion_instrucciones->pc;

		for(int i = 0; i< list_size(informacion_instrucciones->lista_instrucciones); i++){
				instrucciones* aux =  list_get(informacion_instrucciones->lista_instrucciones,i);
							log_info(cpu_logger, "numero instruccion  %u " , aux->instruccion_nombre);
						}

		proxima_instruccion = list_get(informacion_instrucciones->lista_instrucciones,instruccion);
		log_info(cpu_logger, "agarre de la lista");
		log_info(cpu_logger, "recibi instruccion %u", proxima_instruccion->instruccion_nombre);
		while (instruccionNoDesaloja(proxima_instruccion)) {
			log_info(cpu_logger, "entre loop");
			//buffer_unpack(bufferCpu, &informacion_instrucciones,sizeof(informacion_instrucciones));

			if(noMandaContexto(proxima_instruccion)){
				proxima_instruccion = list_get(informacion_instrucciones->lista_instrucciones,informacion_instrucciones->pc);
				log_info(cpu_logger, "esta instruccion no manda contexto");
				decode(informacion_instrucciones, proxima_instruccion);
				proxima_instruccion = list_get(informacion_instrucciones->lista_instrucciones,informacion_instrucciones->pc);
			}
			else{
				log_info(cpu_logger, "esta instruccion manda contexto");
				proxima_instruccion = list_get(informacion_instrucciones->lista_instrucciones,informacion_instrucciones->pc);
				decode(informacion_instrucciones, proxima_instruccion);
				t_buffer* bufferInstruccion= buffer_create();
				headers heads = stream_recv_header(cpu_configs->socket_kernel); ///por algun lugar de aca esta fallando, mose si es el loop o que
				if(heads != header_pcb_execute){
					log_error(cpu_logger, "se recibio mal el header");
				}
				stream_recv_buffer(cpu_configs->socket_kernel, bufferInstruccion);
				informacion_instrucciones = desempaquetar_cpu_response(bufferInstruccion, cpu_logger);
				buffer_destroy(bufferInstruccion);
				proxima_instruccion = list_get(informacion_instrucciones->lista_instrucciones,informacion_instrucciones->pc);
			}

		}
		decode(informacion_instrucciones, proxima_instruccion); //aca ejecutaria exit
	}
}


int main(int argc, char *argv[]) {
	cpu_logger = log_create("cpu.log", "CPU", 1, LOG_LEVEL_DEBUG);

	cpu_configs = cpu_config_crear(PATH_CONFIGS);

	int memoriaSocket = conectar_a_servidor(cpu_configs->ip_memoria,cpu_configs->puerto_memoria);
	if (memoriaSocket == -1) {
		log_error(cpu_logger,"Error al intentar establecer conexión inicial con módulo Memoria");
		log_destroy(cpu_logger);
		return -1;
	}

	cpu_configs->socket_memoria = memoriaSocket;

    stream_send_empty_buffer(memoriaSocket, hs_cpu);
    handsakes memoriaResponse = stream_recv_header(memoriaSocket);
    //t_buffer* bufferMemoria = buffer_create();
    stream_recv_empty_buffer(cpu_configs->socket_memoria);
    if (memoriaResponse != hs_aux) {
        log_error(cpu_logger, "Error al intentar establecer Handshake inicial con módulo Memoria");
        log_destroy(cpu_logger);
        return -1;
    }

    int socketCpu = iniciar_servidor(cpu_configs->puerto_escucha);

    int socketKernel = aceptar_conexion_server(socketCpu);
    cpu_configs->socket_kernel = socketKernel;

    handsakes kernelHeader = stream_recv_header(socketKernel);
    if(kernelHeader != hs_cpu){
		log_error(cpu_logger,"Error al intentar establecer Handshake inicial con módulo Kernel");
		log_destroy(cpu_logger);
		return -1;
    }
    stream_recv_empty_buffer(socketKernel);
    //mandar hs a kernel
    stream_send_empty_buffer(socketKernel, hs_aux);

    log_info(cpu_logger, "Por entrar a realizar_instrucciones");
	pthread_t  th_execute_inst;
	pthread_create(&th_execute_inst, NULL, (void*) realizar_instrucciones, NULL);
	pthread_join(th_execute_inst, NULL);

//	while(1){ //condicion para que termine de ejecutar la cpu
//		terminar_programa(conexion, logger, config);
//	}
	return 0;
}



void iterator(char* value) {
	//log_info(logger,"%s", value);
}

bool instruccionNoDesaloja(instrucciones* proxima_instruccion){
	log_info(cpu_logger, "entre instruccion no desaloja");
	bool ret = true;
	if(proxima_instruccion->instruccion_nombre == INSTRUC_IO || proxima_instruccion->instruccion_nombre == INSTRUC_YIELD || proxima_instruccion->instruccion_nombre == INSTRUC_EXIT){
		ret = false;
	}
	return ret;
}

void decode(cpuResponse* info_instrucciones, instrucciones* instruccion){
	log_info(cpu_logger, "entre decode");
	t_buffer* buffer_respuesta_kernel = buffer_create();
	t_buffer* buffer_mem = buffer_create();
	//cpuResponse* contextoEjecucion = malloc(sizeof(cpuResponse));
	int num_segmento;
	int desplazamiento_segmento;
	tabla_segmento* segmento_encontrado = malloc(sizeof(segmento_encontrado));

	switch(instruccion->instruccion_nombre){
			case INSTRUC_SET:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1, instruccion->parametro2);
				usleep(cpu_configs->retardo_instruccion * 100);
				escribir_valor_en_registro(instruccion->parametro2,instruccion->parametro1,info_instrucciones->registros);
				break;
			case INSTRUC_MOV_IN:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1, instruccion->parametro2);
					num_segmento = floor(atoi(instruccion->parametro2) / atoi(cpu_configs->tam_max_segmento));
					desplazamiento_segmento = atoi(instruccion->parametro2) % atoi(cpu_configs->tam_max_segmento);
					segmento_encontrado = encontrar_segmento(info_instrucciones->tabla_segmentos, num_segmento);
					if(segmento_encontrado->limite >= desplazamiento_segmento){
						int size = encontrar_tamanio_registro(instruccion->parametro1);
						int direccion_fisica = segmento_encontrado->base + desplazamiento_segmento;
						buffer_pack(buffer_mem,&direccion_fisica, sizeof(&direccion_fisica));
						buffer_pack(buffer_mem,&size, sizeof(&size));
						buffer_pack(buffer_mem,&info_instrucciones->pid, sizeof(int));
						stream_send_buffer(cpu_configs->socket_memoria, I_READ, buffer_mem);
						t_buffer* respuesta_mem = buffer_create();
						if(stream_recv_header(cpu_configs->socket_memoria) != I_READ){
							log_error(cpu_logger, "recibi mal el header de memoria en MOV_IN");
						}
						stream_recv_buffer(cpu_configs->socket_memoria, respuesta_mem);
						char* data;
						data = buffer_unpack_string(respuesta_mem);
						buffer_destroy(respuesta_mem);
						log_info(cpu_logger, "PID: %d - Accion: LEER - Segmento: %d - Direccion Fisica: %d - Valor: %s", info_instrucciones->pid, num_segmento, direccion_fisica, data);
						escribir_valor_en_registro(data, instruccion->parametro1, info_instrucciones->registros);
					}
					else{
						log_info(cpu_logger, "PID: %d - Error SEG_FAULT- Segmento: %d - Offset: %d - Tamaño: %d", info_instrucciones->pid, num_segmento, desplazamiento_segmento, segmento_encontrado->limite);
						info_instrucciones->flag = SEG_FAULT;
						buffer_pack(buffer_respuesta_kernel, info_instrucciones, sizeof(info_instrucciones));
						stream_send_buffer(cpu_configs->socket_kernel, header_respuesta_cpu, buffer_respuesta_kernel);
						buffer_destroy(buffer_respuesta_kernel);
						}
				break;
			case INSTRUC_MOV_OUT:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1, instruccion->parametro2);
				num_segmento = floor(atoi(instruccion->parametro1) / atoi(cpu_configs->tam_max_segmento));
								desplazamiento_segmento = atoi(instruccion->parametro1) % atoi(cpu_configs->tam_max_segmento);
								segmento_encontrado = encontrar_segmento(info_instrucciones->tabla_segmentos, num_segmento);
								if (segmento_encontrado->limite >= desplazamiento_segmento) {
									int direccion_fisica = segmento_encontrado->base + desplazamiento_segmento;
									buffer_pack(buffer_mem, &direccion_fisica, sizeof(direccion_fisica));
									char* valor_registro = encontrar_valor_registro(instruccion->parametro2, info_instrucciones->registros);
									int size = encontrar_tamanio_registro(instruccion->parametro2);
									buffer_pack(buffer_mem, &size, sizeof(size));
									buffer_pack(buffer_mem, &info_instrucciones->pid, sizeof(int));
									buffer_pack_string(buffer_mem, valor_registro);

									stream_send_buffer(cpu_configs->socket_memoria,I_WRITE,buffer_mem);
									stream_recv_header(cpu_configs->socket_memoria);//hs_aux = OK
									stream_recv_empty_buffer(cpu_configs->socket_memoria);
									log_info(cpu_logger, "PID: %d - Accion: ESCRIBIR - Segmento: %d - Direccion Fisica: %d - Valor: %s", info_instrucciones->pid, num_segmento, direccion_fisica, valor_registro);
								} else {
									log_info(cpu_logger, "PID: %d - Error SEG_FAULT- Segmento: %d - Offset: %d - Tamaño: %d", info_instrucciones->pid, num_segmento, desplazamiento_segmento, segmento_encontrado->limite);
									info_instrucciones->flag = SEG_FAULT;
									buffer_pack(buffer_respuesta_kernel, info_instrucciones, sizeof(info_instrucciones));
									stream_send_buffer(cpu_configs->socket_kernel, header_respuesta_cpu, buffer_respuesta_kernel);
									buffer_destroy(buffer_respuesta_kernel);
								}
				break;
			case INSTRUC_F_READ:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s %s %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1, instruccion->parametro2, instruccion->parametro3);
				info_instrucciones->flag = I_READ;
				info_instrucciones->archivo = instruccion->parametro1;
				info_instrucciones->sizeSegmento = atoi(instruccion->parametro3);
				num_segmento = floor(atoi(instruccion->parametro2) / atoi(cpu_configs->tam_max_segmento));
								desplazamiento_segmento = atoi(instruccion->parametro2) % atoi(cpu_configs->tam_max_segmento);
								segmento_encontrado = encontrar_segmento(info_instrucciones->tabla_segmentos, num_segmento);
								if(segmento_encontrado->limite >= (desplazamiento_segmento + info_instrucciones->sizeSegmento)){
									info_instrucciones->direccionFisica = segmento_encontrado->base + desplazamiento_segmento;
								}else{
									log_info(cpu_logger, "PID: %d - Error SEG_FAULT- Segmento: %d - Offset: %d - Tamaño: %d", info_instrucciones->pid, num_segmento, desplazamiento_segmento, segmento_encontrado->limite);
									info_instrucciones->flag = SEG_FAULT;
								}
				break;
			case INSTRUC_F_WRITE:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s %s %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1, instruccion->parametro2, instruccion->parametro3);
				info_instrucciones->flag = I_WRITE;
				info_instrucciones->archivo = instruccion->parametro1;
				info_instrucciones->sizeSegmento = atoi(instruccion->parametro3);
				num_segmento = floor(atoi(instruccion->parametro2) / atoi(cpu_configs->tam_max_segmento));
								desplazamiento_segmento = atoi(instruccion->parametro2) % atoi(cpu_configs->tam_max_segmento);
								segmento_encontrado = encontrar_segmento(info_instrucciones->tabla_segmentos, num_segmento);
								if(segmento_encontrado->limite >= (desplazamiento_segmento + info_instrucciones->sizeSegmento)){
									info_instrucciones->direccionFisica = segmento_encontrado->base + desplazamiento_segmento;
								}else{
									log_info(cpu_logger, "PID: %d - Error SEG_FAULT- Segmento: %d - Offset: %d - Tamaño: %d", info_instrucciones->pid, num_segmento, desplazamiento_segmento, segmento_encontrado->limite);
									info_instrucciones->flag = SEG_FAULT;
								}
				break;
			case INSTRUC_F_TRUNCATE:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1, instruccion->parametro2);
				info_instrucciones->flag = I_TRUNCATE;
				info_instrucciones->archivo = instruccion->parametro1;
				info_instrucciones->sizeSegmento = atoi(instruccion->parametro2);
				break;
			case INSTRUC_F_SEEK:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1, instruccion->parametro2);
				info_instrucciones->flag = I_SEEK;
				info_instrucciones->archivo = instruccion->parametro1;
				info_instrucciones->sizeSegmento = atoi(instruccion->parametro2);
				break;
			case INSTRUC_CREATE_SEGMENT:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1, instruccion->parametro2);
				info_instrucciones->flag = I_CREATE_SEGMENT;
				info_instrucciones->id_segmento= atoi(instruccion->parametro1);
				info_instrucciones->sizeSegmento = atoi(instruccion->parametro2);
				break;
			case INSTRUC_IO:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1);
				info_instrucciones->flag = I_IO;
				info_instrucciones->tiempo = atoi(instruccion->parametro1);
				break;
			case INSTRUC_WAIT:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1);
				info_instrucciones->flag = I_WAIT;
				info_instrucciones->recurso= instruccion->parametro1;
				break;
			case INSTRUC_SIGNAL:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1);
				info_instrucciones->flag = I_SIGNAL;
				info_instrucciones->recurso= instruccion->parametro1;
				break;
			case INSTRUC_F_OPEN:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1);
				info_instrucciones->flag = I_OPEN;
				info_instrucciones->archivo= instruccion->parametro1;
				break;
			case INSTRUC_F_CLOSE:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1);
				info_instrucciones->flag = I_CLOSE;
				info_instrucciones->archivo = instruccion->parametro1;
				break;
			case INSTRUC_DELETE_SEGMENT:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u - %s", info_instrucciones->pid, instruccion->instruccion_nombre, instruccion->parametro1);
				info_instrucciones->flag = I_DELETE_SEGMENT;
				info_instrucciones->id_segmento= atoi(instruccion->parametro1);
				break;
			case INSTRUC_EXIT:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u", info_instrucciones->pid, instruccion->instruccion_nombre);
				info_instrucciones->flag = I_EXIT;
				break;
			case INSTRUC_YIELD:
				log_info(cpu_logger, "PID: %d - Ejecutando: %u", info_instrucciones->pid, instruccion->instruccion_nombre);
				info_instrucciones->flag = I_YIELD;
				break;
			default:
				log_error(cpu_logger,"Error al intentar desempaquetar una instrucción");
				exit(-1);

			}
			info_instrucciones->pc +=1;
			if(hayQueMandarContexto(instruccion)){
				//info_instrucciones->pc = info_instrucciones->pc;
				enviar_instruccion_kernel(info_instrucciones);
				buffer_destroy(buffer_respuesta_kernel);
				buffer_destroy(buffer_mem);
			}
}

tabla_segmento* encontrar_segmento(t_list* tabla_segmentos, int num_segmento){
	tabla_segmento* segmento_encontrado = malloc(sizeof(segmento_encontrado));
	int tamanio_tabla = list_size(tabla_segmentos);
	for(int i = 0; i<tamanio_tabla; i++){
		tabla_segmento* segmento_actual = list_get(tabla_segmentos, i);
		if(segmento_actual->id == num_segmento){
			segmento_encontrado = segmento_actual;
		}
	}
	return segmento_encontrado;
}

int calcular_direccion_fisica(){
	return 0;
}

bool hayQueMandarContexto(instrucciones* instruccion){
	bool ret = true;
	if(instruccion->instruccion_nombre == INSTRUC_SET || instruccion->instruccion_nombre == INSTRUC_MOV_IN || instruccion->instruccion_nombre == INSTRUC_MOV_OUT){
		ret = false;
	}
	return ret;
}

void escribir_valor_en_registro_con_N_caracteres(char* valor,char* registro,int longitud){
	for(int i = 0; i<longitud; i++){
		registro[i]='0';
	}
	for(int j=0; j<strlen(valor);j++){
		registro[j] = valor[j];
	}
}

void escribir_valor_en_registro(char* valor,char* registro, registros_cpu* registros){
	log_info(cpu_logger, "escribiendo registro %s el valor %s ", registro, valor);
	if(strcmp(registro, "AX") == 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->AX, 4);
	}
	else if(strcmp(registro, "BX") == 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->BX, 4);
	}
	else if(strcmp(registro, "CX")== 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->CX, 4);
	}
	else if(strcmp(registro, "DX")== 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->DX, 4);
	}
	else if(strcmp(registro, "EAX")== 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->EAX, 8);
	}
	else if(strcmp(registro, "EBX")== 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->EBX, 8);
	}
	else if(strcmp(registro, "ECX")== 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->ECX, 8);
	}
	else if(strcmp(registro, "EDX")== 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->EDX, 8);
	}
	else if(strcmp(registro, "RAX")== 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->RAX, 16);
	}
	else if(strcmp(registro, "RBX")== 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->RBX, 16);
	}
	else if(strcmp(registro, "RCX")== 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->RCX, 16);
	}
	else if(strcmp(registro, "RDX")== 0){
		escribir_valor_en_registro_con_N_caracteres(valor,registros->RDX, 16);
	}
	log_info(cpu_logger,"valor escrito en registro con exito: %s",encontrar_valor_registro(registro,registros));
}

char* encontrar_valor_registro(char* registro, registros_cpu* registros){
	char* ret="";
	if(strcmp(registro, "AX") == 0){
		ret=registros->AX;
		}
		else if(strcmp(registro, "BX") == 0){
			ret=registros->BX;
		}
		else if(strcmp(registro, "CX")== 0){
			ret=registros->CX;
		}
		else if(strcmp(registro, "DX")== 0){
			ret=registros->DX;
		}
		else if(strcmp(registro, "EAX")== 0){
			ret=registros->EAX;
		}
		else if(strcmp(registro, "EBX")== 0){
			ret=registros->EBX;
		}
		else if(strcmp(registro, "ECX")== 0){
			ret=registros->ECX;
		}
		else if(strcmp(registro, "EDX")== 0){
			ret=registros->EDX;
		}
		else if(strcmp(registro, "RAX")== 0){
			ret=registros->RAX;
		}
		else if(strcmp(registro, "RBX")== 0){
			ret=registros->RBX;
		}
		else if(strcmp(registro, "RCX")== 0){
			ret=registros->RCX;
		}
		else if(strcmp(registro, "RDX")== 0){
			ret=registros->RDX;
		}
	return ret;
}

int encontrar_tamanio_registro(char* registro){
	int ret = 0;
	if(registro[0] == 'E'){
		ret = 8;
	}
	else if(registro[0] == 'R'){
		ret = 16;
	}
	else{
		ret = 4;
	}
	return ret;
}
void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion);
	/* Y por ultimo, hay que liberar lo que utilizamos (conexion, log y config)
	  con las funciones de las commons y del TP mencionadas en el enunciado */
}


static void cpu_config_iniciar(void* moduleConfig, t_config* config){
	cpu_config* cpuConfig = (cpu_config*)moduleConfig;
	cpuConfig->ip_memoria = strdup(config_get_string_value(config, "IP_MEMORIA"));
	cpuConfig->puerto_escucha = strdup(config_get_string_value(config, "PUERTO_ESCUCHA"));
	cpuConfig->puerto_memoria = strdup(config_get_string_value(config, "PUERTO_MEMORIA"));
	cpuConfig->tam_max_segmento = strdup(config_get_string_value(config, "TAM_MAX_SEGMENTO"));
	cpuConfig->retardo_instruccion = config_get_int_value(config, "RETARDO_INSTRUCCION");


	cpuConfig->socket_kernel = -1;
	cpuConfig->socket_memoria = -1;

}

cpu_config* cpu_config_crear(char* path){
	cpu_config* config_aux = malloc(sizeof(*config_aux));
	config_init(config_aux, path, cpu_logger, cpu_config_iniciar); //hay que averiguar como hacer para que levante esto por argumento
	return config_aux;
}

/*
void desempaquetar_registros_cpu(t_buffer* buffer, registros_cpu* registros) {
    buffer_unpack(buffer, registros->AX, sizeof(registros->AX));
    buffer_unpack(buffer, registros->BX, sizeof(registros->BX));
    buffer_unpack(buffer, registros->CX, sizeof(registros->CX));
    buffer_unpack(buffer, registros->DX, sizeof(registros->DX));

    buffer_unpack(buffer, registros->EAX, sizeof(registros->EAX));
    buffer_unpack(buffer, registros->EBX, sizeof(registros->EBX));
    buffer_unpack(buffer, registros->ECX, sizeof(registros->ECX));
    buffer_unpack(buffer, registros->EDX, sizeof(registros->EDX));

    buffer_unpack(buffer, registros->RAX, sizeof(registros->RAX));
    buffer_unpack(buffer, registros->RBX, sizeof(registros->RBX));
    buffer_unpack(buffer, registros->RCX, sizeof(registros->RCX));
    buffer_unpack(buffer, registros->RDX, sizeof(registros->RDX));
}*/


void enviar_instruccion_kernel (cpuResponse* cpu_enviar){
	t_buffer* bufferEjecutar = buffer_create();
	//cpuResponse* cpu_enviar = malloc(sizeof(cpuResponse));

	buffer_pack(bufferEjecutar, &cpu_enviar->pid, sizeof(cpu_enviar->pid));
	log_info(cpu_logger, "mando pid: %d", cpu_enviar->pid);


	buffer_pack(bufferEjecutar, &cpu_enviar->pc, sizeof(cpu_enviar->pc));
	log_info(cpu_logger, "mando pc: %d", cpu_enviar->pc);

	buffer_pack(bufferEjecutar, &cpu_enviar->registros, sizeof(cpu_enviar->registros));
	//pack_registros(bufferEjecutar, cpu_enviar);

	log_info(cpu_logger, "por agarrar tamanio");
	int tam_lista_instruc = list_size(cpu_enviar->lista_instrucciones); //me esta dando un numero enorme esto.
	instrucciones* instruc_ejemplo = list_get(cpu_enviar->lista_instrucciones, 0);
	buffer_pack(bufferEjecutar, &tam_lista_instruc, sizeof(int));
	log_info(cpu_logger, "por empaquetar listas");
	empaquetar_lista_instrucciones(bufferEjecutar,tam_lista_instruc,cpu_enviar->lista_instrucciones, cpu_logger);
	log_info(cpu_logger, "tamanio lista: %d", list_size(cpu_enviar->lista_instrucciones));

	int tam_lista_segmentos = list_size(cpu_enviar->tabla_segmentos);
	tabla_segmento* segmento_ejemplo = list_get(cpu_enviar->tabla_segmentos,0);
	buffer_pack(bufferEjecutar, &tam_lista_segmentos, sizeof(int));
	log_info(cpu_logger, "por empaquetar lista de segmentos");
	empaquetar_lista_segmentos(bufferEjecutar,tam_lista_segmentos,cpu_enviar->tabla_segmentos, cpu_logger);
	log_info(cpu_logger, "empaquetado con exito");

	buffer_pack_string(bufferEjecutar, cpu_enviar->archivo);
	buffer_pack(bufferEjecutar,&cpu_enviar->flag,sizeof(cpu_enviar->flag));
	log_info(cpu_logger, "empaquetado flag");
	buffer_pack(bufferEjecutar,&cpu_enviar->tiempo,sizeof(cpu_enviar->tiempo));
	buffer_pack_string(bufferEjecutar, cpu_enviar->recurso);//revisar si falla
	buffer_pack(bufferEjecutar,&cpu_enviar->sizeSegmento,sizeof(cpu_enviar->sizeSegmento));
	buffer_pack(bufferEjecutar,&cpu_enviar->id_segmento,sizeof(cpu_enviar->id_segmento));
	buffer_pack(bufferEjecutar,&cpu_enviar->posicionPuntero,sizeof(cpu_enviar->posicionPuntero));
	buffer_pack(bufferEjecutar,&cpu_enviar->direccionFisica,sizeof(cpu_enviar->direccionFisica));

	stream_send_buffer(cpu_configs->socket_kernel, header_respuesta_cpu, bufferEjecutar);
	buffer_destroy(bufferEjecutar);
}
