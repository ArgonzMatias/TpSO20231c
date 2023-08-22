#include "memoria_solicitudes.h"

extern memoria_config* memoria_configs;
extern char* memoria_Principal;
extern t_log* logger_memoria;


void* peticiones_kernel(void* socketKernel){
	for(;;){
		instruccion instruccion = stream_recv_header(socketKernel);
		stream_recv_empty_buffer(socketKernel);
		int size, id_segmento;
		switch(instruccion){
		//los casos de cada header
			case I_CREATE_SEGMENT:
				log_info(logger_memoria, "Empezando proceso creacion de segmento");
				int pid= 0;
				//recibo el pid del segmento
				t_buffer* bufferPId = buffer_create();
				stream_recv_header(socketKernel);
				stream_recv_buffer(socketKernel, bufferPId);
				buffer_unpack(bufferPId, &pid, sizeof(pid));

				//recibo el tamanio del segmento
				t_buffer* buffer = buffer_create();
				stream_recv_header(socketKernel);
				stream_recv_buffer(socketKernel, buffer);
				buffer_unpack(buffer, &size, sizeof(size));

				//recibo el tamanio del segmento
				t_buffer* bufferIdSegmento = buffer_create();
				stream_recv_header(socketKernel);
				stream_recv_buffer(socketKernel, bufferIdSegmento);
				buffer_unpack(bufferIdSegmento, &id_segmento, sizeof(id_segmento));

				//creo segmento
				log_info(logger_memoria, "size: %d",size);
				tabla_segmento* tablaNueva = crear_nuevo_segmento(id_segmento,size, pid);
				if(tablaNueva!= NULL){
					log_info(logger_memoria, "creando buffer para mandar a kernel de memoria");
				t_buffer* bufferSegmentoNuevo = buffer_create();
				empaquetar_segmento(bufferSegmentoNuevo, tablaNueva);
				stream_send_buffer(socketKernel,header_general,bufferSegmentoNuevo);
				buffer_destroy(bufferSegmentoNuevo);
				}
				log_info(logger_memoria, "PID: %d - Crear Segmento: %d - Base: %d - TAMAÑO: %d",pid, id_segmento, tablaNueva->base ,  size);

				buffer_destroy(bufferPId);
				buffer_destroy(buffer);

				break;
			case I_DELETE_SEGMENT:
				t_buffer* buffer3 = buffer_create();
				stream_recv_buffer(socketKernel, buffer3);
				buffer_unpack(buffer3, &id_segmento, sizeof(id_segmento));
				log_info(logger_memoria, "Eliminando segmento numero %d ", id_segmento);

				if(id_segmento == 0){
					log_error(logger_memoria, "segmento invalido");
					break;
				}
				eleminarSegmento(id_segmento);
				bool mismo_id(tabla_segmento tablaAux){
												return(tablaAux.id == id_segmento);
											}
				list_remove_by_condition(get_tabla_segmentos(), (void*) mismo_id);
				buffer_destroy(buffer3);
				break;
			case I_EXIT:
				log_info(logger_memoria, "eliminando segmentos de proceso en EXIT");

				t_buffer* bufferpcb = buffer_create();
				procesoNuevo* aux; //aca capaz hay q hacer un malloc
				stream_recv_buffer(socketKernel, bufferpcb);
				buffer_unpack(bufferpcb, &aux, sizeof(aux));

				for(int i=0; i<sizeof(aux->tabla_de_segmentos); i++){
					eleminarSegmento(i);
				}

				log_info(logger_memoria, "Eliminación de Proceso PID: %d", aux->pid);
				log_info(logger_memoria, "segmentos eliminados correctamente");
				stream_send_empty_buffer(socket, hs_aux);
				free(aux);
				break;

			case CREAR_PROCESO:
				int pidCrearProceso= 0;
					//recibo el tamanio del segmento
					t_buffer* bufferPIdCrear = buffer_create();
					stream_recv_header(socketKernel);
					stream_recv_buffer(socketKernel, bufferPIdCrear);
					buffer_unpack(bufferPIdCrear, &pidCrearProceso, sizeof(pidCrearProceso));
					buffer_destroy(bufferPIdCrear);
					log_info(logger_memoria, "Creación de Proceso PID: %d", pidCrearProceso);
				break;
			default:
				log_error(logger_memoria, "instruccion invalida");
				break;
		}
	}

}

void* peticiones_cpu(void* socketCpu){
	for(;;){
		headers header = stream_recv_header(socketCpu);

		simulador_de_espera(memoria_configs->retardo_memoria);
		switch(header){  //los casos de cada header
			case I_READ:
				m_read(socketCpu, "CPU");
				break;

			case I_WRITE:
				m_write(socketCpu,"CPU");
				break;

		}
	}
}

void* peticiones_fileSystem(void* socketFile){
	for(;;){
		instruccion header = stream_recv_header(socketFile);

		simulador_de_espera(memoria_configs->retardo_memoria);
		switch(header){
		case I_WRITE:
				f_write(socketFile, "FS");
				break;
		case I_READ:
				f_read(socketFile, "FS");
				break;

		}
	}

}

void m_write(void *socket, char* fuente){
	log_info(logger_memoria, "Pedido de escritura");
	int direccionFisica, size, pid;
	char *value;
	//recibo la direccion fisica
	t_buffer *buffer2 = buffer_create();
	stream_recv_buffer(socket, buffer2);

	buffer_unpack(buffer2, &direccionFisica, sizeof(direccionFisica));
	log_info(logger_memoria, "df: %d", direccionFisica);

	buffer_unpack(buffer2, &size, sizeof(size));
	log_info(logger_memoria, "size: %d", size);

	value = malloc(size);
	buffer_unpack(buffer2, &pid, sizeof(int));
	log_info(logger_memoria, "pid: %d", pid);

	value = buffer_unpack_string(buffer2);
	log_info(logger_memoria, "value: %s", value);

	memcpy((void*) (memoria_Principal + direccionFisica), value, size);

	stream_send_empty_buffer(socket, hs_aux); //esto seria el OK
	log_info(logger_memoria,"PID: %d - Acción: ESCRIBIR - Dirección física: %d - Tamaño: %d- Origen: %s ",pid, direccionFisica, size, fuente);
	log_info(logger_memoria,"Se escribio el valor %s en la direccion fisica %d", value,direccionFisica);

	free(value);
	buffer_destroy(buffer2);
}

void m_read(int socket, char* fuente){
	int direccionFisica, size, pid;
	char* value;
	log_info(logger_memoria, "Pedido de lectura");
	t_buffer* buffer = buffer_create();
	stream_recv_buffer(socket, buffer);

	buffer_unpack(buffer, &direccionFisica,sizeof(&direccionFisica));
	log_info(logger_memoria, "df: %d", direccionFisica);

	buffer_unpack(buffer, &size,sizeof(&size));
	log_info(logger_memoria, "size: %d", size);

	buffer_unpack(buffer, &pid,sizeof(int));
	log_info(logger_memoria, "pid: %d", pid);

	value = malloc(size+1);

	memcpy(value, memoria_Principal + direccionFisica, size);
	value[size]='\0';
	t_buffer* rtaRead = buffer_create();
	buffer_pack_string(rtaRead, value);
	stream_send_buffer(socket, I_READ, rtaRead);
	buffer_destroy(rtaRead);

	log_info(logger_memoria, "PID: %d - Acción: LEER - Dirección física: %d - Tamaño: %d- Origen: %s ", pid, direccionFisica, size, fuente);
	log_info(logger_memoria, "Se envio el valor %s de la direccion fisica %d", value, direccionFisica);

	free(value);
	buffer_destroy(buffer);

}

void f_write(void *socket, char* fuente){
	log_info(logger_memoria, "Pedido de escritura");
					int direccionFisica, size, pid;
					void* value;
					//recibo la direccion fisica
					t_buffer* buffer2 = buffer_create();
					stream_recv_buffer(socket, buffer2);
					buffer_unpack(buffer2, &direccionFisica,sizeof(direccionFisica));
					log_info(logger_memoria, "df: %d", direccionFisica);
					buffer_unpack(buffer2, &size,sizeof(size));
					log_info(logger_memoria, "size: %d", size);
					 value = malloc(size);
					buffer_unpack(buffer2, &pid,sizeof(int));
					log_info(logger_memoria, "pid: %d", pid);
					buffer_unpack(buffer2, value,size);
					log_info(logger_memoria, "value: %s", value);



					memcpy((void*) (memoria_Principal + direccionFisica), value, size);
					stream_send_empty_buffer(socket, hs_aux); //esto seria el OK
					log_info(logger_memoria, "PID: %d - Acción: ESCRIBIR - Dirección física: %d - Tamaño: %d- Origen: %s ", pid, direccionFisica, size, fuente);
					log_info(logger_memoria, "Se escribio el valor %d en la direccion fisica %d", value, direccionFisica);
					buffer_destroy(buffer2);
}


void f_read(int socket, char* fuente){
	int direccionFisica, size, pid;

	log_info(logger_memoria, "Pedido de lectura");
	t_buffer* buffer = buffer_create();
	stream_recv_buffer(socket, buffer);
	buffer_unpack(buffer, &direccionFisica,sizeof(direccionFisica));
	log_info(logger_memoria, "df: %d", direccionFisica);
	buffer_unpack(buffer, &size,sizeof(size));
	char* value = malloc(size+1);
	log_info(logger_memoria, "size: %d", size);
	buffer_unpack(buffer, &pid,sizeof(int));
	log_info(logger_memoria, "pid: %d", pid);
	memcpy((void*)value, (void*)(memoria_Principal + direccionFisica), size);
	t_buffer* rtaRead = buffer_create();

	value[size] = "\0";

	buffer_pack_string(rtaRead, value);
	stream_send_buffer(socket, I_READ, rtaRead);
	buffer_destroy(rtaRead);
	log_info(logger_memoria, "PID: %d - Acción: LEER - Dirección física: %d - Tamaño: %d- Origen: %s ", pid, direccionFisica, size, fuente);
	log_info(logger_memoria, "Se envio el valor %u de la direccion fisica %d", value, direccionFisica);
	buffer_destroy(buffer);
}

void* stream_tabla_actualizada(){
	t_buffer* tablaSegmentosBuffer = buffer_create();
	buffer_pack(tablaSegmentosBuffer, get_tabla_segmentos(), sizeof(get_tabla_segmentos()));
	stream_send_buffer(memoria_configs->socket_kernel,header_general,tablaSegmentosBuffer);
	buffer_destroy(tablaSegmentosBuffer);
}
