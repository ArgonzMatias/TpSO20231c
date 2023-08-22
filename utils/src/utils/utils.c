#include "utils.h"
#include <errno.h>

void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	// Ahora vamos a crear el socket.
	int socket_cliente = socket(server_info->ai_family,
			 	 	 	 	 	server_info->ai_socktype,
								server_info->ai_protocol);

	// Ahora que tenemos el socket, vamos a conectarlo
	connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);

	freeaddrinfo(server_info);

	return socket_cliente;
}

void enviar_mensaje(char* mensaje, int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}


void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(void)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}
//otro file

//t_buffer* crear_buffer_instruccion (instrucciones* instruccion )
//{
//
//
//	t_buffer* buffer = malloc(sizeof(t_buffer));
//
//	buffer->size = sizeof(char*) * 3 ;
//
//	void* stream = malloc(buffer->size);
//	int offset = 0; // Desplazamiento
//
//	memcpy(stream + offset, &instruccion->parametro1,(strlen(instruccion->parametro1)+1)); // size of es el tipo de dato de cada param
//	offset += sizeof(strlen(instruccion->parametro1));
//	memcpy(stream + offset, &instruccion->parametro2, (strlen(instruccion->parametro2)+1));
//	offset += sizeof(strlen(instruccion->parametro2));
//	memcpy(stream + offset, &instruccion->parametro3, (strlen(instruccion->parametro3)+1));
//	offset += sizeof(strlen(instruccion->parametro3));
//
//	// Para el nombre primero mandamos el tamaño y luego el texto en sí:
//	//memcpy(stream + offset, &persona.nombre_length, sizeof(uint32_t));
//	//offset += sizeof(uint32_t);
//	//memcpy(stream + offset, persona.nombre, strlen(persona.nombre) + 1);
//	// No tiene sentido seguir calculando el desplazamiento, ya ocupamos el buffer completo
//
//	buffer->stream = stream;
//
//	// Si usamos memoria dinámica para el nombre, y no la precisamos más, ya podemos liberarla:
//	free(instruccion);
//	return buffer;
//}
void agregar_instrucciones_al_paquete(t_buffer *buffer,int socket)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	//paquete->codigo_operacion = PERSONA; // Podemos usar una constante por operación//struct de codigos del atchivo ps
	paquete->buffer = buffer; // Nuestro buffer de antes.

	// Armamos el stream a enviar
	void* a_enviar = malloc(buffer->size + sizeof(uint8_t) + sizeof(uint32_t));
	int offset = 0;

	memcpy(a_enviar + offset, &(paquete->codigo_operacion), sizeof(uint8_t));

	offset += sizeof(uint8_t);
	memcpy(a_enviar + offset, &(paquete->buffer->size), sizeof(uint32_t));
	offset += sizeof(uint32_t);
	memcpy(a_enviar + offset, paquete->buffer->stream, paquete->buffer->size);

	// Por último enviamos
	send(socket, a_enviar, buffer->size + sizeof(uint8_t) + sizeof(uint32_t), 0);

	//iberamos la memoria que ya no usamos
	free(a_enviar);
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

//void deserializar(t_buffer buffer , int socket )
//{
//	t_paquete* paquete = malloc(sizeof(t_paquete));
//	paquete->buffer = malloc(sizeof(t_buffer));
//
//	// Primero recibimos el codigo de operacion
//	recv(socket, &(paquete->codigo_operacion), sizeof(uint8_t), 0);
//
//	// Después ya podemos recibir el buffer. Primero su tamaño seguido del contenido
//	recv(socket, &(paquete->buffer->size), sizeof(uint32_t), 0);
//	paquete->buffer->stream = malloc(paquete->buffer->size);
//	recv(socket, paquete->buffer->stream, paquete->buffer->size, 0);
//
//	// Ahora en función del código recibido procedemos a deserializar el resto
//	switch(paquete->codigo_operacion) {
//	   // case PERSONA:
//	        instrucciones* persona = deserializar_persona(paquete->buffer);
//	        //...
//	        // Hacemos lo que necesitemos con esta info
//	        // Y eventualmente liberamos memoria
//	       // free(persona);
//	       // ...
//	        break;
//	     // ...Evaluamos los demás casos según corresponda
//	}
//
//	// Liberamos memoria
//	free(paquete->buffer->stream);
//	free(paquete->buffer);
//	free(paquete);
//}

//instrucciones* deserializar_instruccion(t_buffer* buffer)
//{
//    instrucciones* instruccion = malloc(sizeof(instrucciones));
//
//    void* stream = buffer->stream;
//    // Deserializamos los campos que tenemos en el buffer
//    memcpy(&(instruccion->parametro1), stream, sizeof(char*));
//    stream += sizeof(uint32_t);
//    memcpy(&(instruccion->parametro2), stream, sizeof(char*));
//    stream += sizeof(uint8_t);
//    memcpy(&(instruccion->parametro3), stream, sizeof(char*));
//    stream += sizeof(uint32_t);
//
//    // Por último, para obtener el nombre, primero recibimos el tamaño y luego el texto en sí:
//    //memcpy(&(persona->nombre_length), stream, sizeof(uint32_t));
//    //stream += sizeof(uint32_t);
//    //persona->nombre = malloc(persona->nombre_length);
//    //memcpy(persona->nombre, stream, persona->nombre_length);
//
//    return instruccion;
//}
int iniciar_servidor(char* puerto)
{
	// Quitar esta línea cuando hayamos terminado de implementar la funcion
	//assert(!"no implementado!");

	int socket_servidor;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto, &hints, &servinfo);

	// Creamos el socket de escucha del servidor
	socket_servidor = socket(servinfo-> ai_family,
							servinfo->ai_socktype,
							servinfo->ai_protocol);
	// Asociamos el socket a un puerto
	bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
	// Escuchamos las conexiones entrantes

	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);

	return socket_servidor;
}

//int esperar_cliente(int socket_servidor)
//{
//	// Aceptamos un nuevo cliente
//	int socket_cliente = accept(socket_servidor, NULL, NULL);
//	log_info(logger, "Se conecto un cliente!");
//
//	return socket_cliente;
//}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void* recibir_buffer(int* size, int socket_cliente)
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

//void recibir_mensaje(int socket_cliente)
//{
//	int size;
//	char* buffer = recibir_buffer(&size, socket_cliente);
//	log_info(logger, "Me llego el mensaje %s", buffer);
//	free(buffer);
//}

t_list* recibir_paquete(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

int config_init(void* moduleConfig, char* pathToConfig, t_log* moduleLogger,
                void (*config_initializer)(void* moduleConfig, t_config* tempConfig)) {
    t_config* tempConfig = config_create(pathToConfig);
    if (NULL == tempConfig) {
        log_error(moduleLogger, "Path \"%s\" no encontrado", pathToConfig);
        return -1;
    }
    config_initializer(moduleConfig, tempConfig);
    log_info(moduleLogger, "Inicialización de campos correcta"); //esto me tira error nose pq
    config_destroy(tempConfig);
    return 1;
}

int conectar_a_servidor(char* ip, char* port) {
    int conn;
    struct addrinfo* hints = malloc(sizeof(*hints));
    struct addrinfo* serverInfo = malloc(sizeof(*serverInfo));
    struct addrinfo* p = malloc(sizeof(*p));

    memset(hints, 0, sizeof(*hints));
    	hints->ai_family = AF_INET;
    	hints->ai_socktype = SOCK_STREAM;

    int rv = getaddrinfo(ip, port, hints, &serverInfo);
    if (rv != 0) {
        fprintf(stderr, "getaddrinfo error: %s", gai_strerror(rv));
        return EXIT_FAILURE;
    }
    for (p = serverInfo; p != NULL; p = p->ai_next) {
        conn = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (-1 == conn) {
            continue;
        }
        if (connect(conn, p->ai_addr, p->ai_addrlen) != -1) {
            break;
        }
        close(conn);
    }
    freeaddrinfo(serverInfo);
    freeaddrinfo(hints);
    if (conn != -1 && p != NULL) {
        return conn;
    }
    freeaddrinfo(p);
    return -1;
}


static void __stream_send(int toSocket, void* streamToSend, int bufferSize) {
    uint8_t header = 0;
    uint32_t size = 0;
    ssize_t bytesSent = send(toSocket, streamToSend, sizeof(header) + sizeof(size) + bufferSize, 0);
    if (bytesSent == -1) {
        printf("\e[0;31m__stream_send: Error en el envío del buffer [%s]\e[0m\n", strerror(errno));
    }
}

static void* __stream_create(uint8_t header, t_buffer* buffer) {
    void* streamToSend = malloc(sizeof(header) + sizeof(buffer->size) + buffer->size);
    int offset = 0;
    memcpy(streamToSend + offset, &header, sizeof(header));
    offset += sizeof(header);
    memcpy(streamToSend + offset, &(buffer->size), sizeof(buffer->size));
    offset += sizeof(buffer->size);
    memcpy(streamToSend + offset, buffer->stream, buffer->size);
    return streamToSend;
}

void stream_send_buffer(int toSocket, uint8_t header, t_buffer* buffer) {
    void* stream = __stream_create(header, buffer);
    __stream_send(toSocket, stream, buffer->size);
    free(stream);
}

void stream_send_empty_buffer(int toSocket, uint8_t header) {
    t_buffer* emptyBuffer = buffer_create();
    stream_send_buffer(toSocket, header, emptyBuffer);
    buffer_destroy(emptyBuffer);
}

void stream_recv_buffer(int fromSocket, t_buffer* destBuffer) {
    ssize_t msgBytes = recv(fromSocket, &(destBuffer->size), sizeof(destBuffer->size), 0);
    if (msgBytes <= 0) {
        if (msgBytes == 0) {
            printf("\e[0;31mstream_recv_buffer: La conexión ha sido cerrada por el otro extremo\e[0m\n");
        } else {
            printf("\e[0;31mstream_recv_buffer: Error en la recepción del buffer [%s]\e[0m\n", strerror(errno));
        }
        exit(-1);
    } else if (destBuffer->size > 0) {
        destBuffer->stream = malloc(destBuffer->size);
        if (destBuffer->stream == NULL) {
            printf("\e[0;31mstream_recv_buffer: Error en la asignación de memoria\e[0m\n");
            exit(-1);
        }
        recv(fromSocket, destBuffer->stream, destBuffer->size, 0);
    }
}

uint8_t stream_recv_header(int fromSocket) {
    uint8_t header;
    ssize_t msgBytes = recv(fromSocket, &header, sizeof(header), 0);
    if (msgBytes == -1) {
        printf("\e[0;31mstream_recv_buffer: Error en la recepción del header [%s]\e[0m\n", strerror(errno));
    }
    return header;
}

cpuResponse stream_recv_cpu_response(int fromSocket) {
    cpuResponse respuesta;
    ssize_t msgBytes = recv(fromSocket, &respuesta, sizeof(respuesta), 0);
    if (msgBytes == -1) {
        printf("\e[0;31mstream_recv_buffer: Error en la recepción del header [%s]\e[0m\n", strerror(errno));
    }
    return respuesta;
}


void stream_recv_empty_buffer(int fromSocket) {
    t_buffer* buffer = buffer_create();
    stream_recv_buffer(fromSocket, buffer);
    buffer_destroy(buffer);
}



//server y handshake

int aceptar_conexion_server(int socket){
	struct sockaddr cliente = {0};
	socklen_t aux = sizeof(cliente);
	int cliente_aux = accept(socket, &cliente, &aux);
		if(cliente_aux > -1){
			int* socketCliente = malloc(sizeof(*socketCliente));
			*socketCliente = cliente_aux;
			return cliente_aux;
		}
		return -1;
}

int handshake(int* socket, handsakes hs, char* num, t_log* logger){
	handsakes handshakeRecibido = stream_recv_header(*socket);
	stream_recv_empty_buffer(*socket);
			if(handshakeRecibido != hs){
				log_error(logger, "error en handshake %s", num);
				return -1;
			}
			stream_send_empty_buffer(*socket, hs_aux);
			return 1;
}


tabla_segmento* findSegmentoConId(t_list* tablaSegmentos, int id){
	tabla_segmento* tabla;
	int id_compare(tabla_segmento* tablaAux){
		return(id == tablaAux->id);
	}
	tabla = list_find(tablaSegmentos, (void*)id_compare);
	return tabla;
}


char* arrayToString(char* parametro){
	return parametro;
}

instrucciones* armar_instruccion(tipo_instruccion_consola nombre_instruccion, char* parametro1, char* parametro2, char* parametro3){
	instrucciones* instruccion_ret = malloc(sizeof(*instruccion_ret));
	instruccion_ret->instruccion_nombre = nombre_instruccion;
	instruccion_ret->parametro1 = parametro1;
	instruccion_ret->parametro2 = parametro2;
	instruccion_ret->parametro3 = parametro3;
	return instruccion_ret;
}

void empaquetar_lista(t_list* lista, int longitud, void* tipodato, t_buffer* buffer, t_log* cpu_logger){

	for(int i=0;i<longitud;i++){
		void* elem = list_get(lista, i);
		buffer_pack(buffer, elem, sizeof(*tipodato));
		log_info(cpu_logger, "numero en la lista guardado %d", i);
	}
	log_info(cpu_logger, "todo listo crack");

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
}

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


t_list* obtener_lista_instrucciones(t_buffer* bufferInstrucciones, t_log* logger){
	t_list* lista_instrucciones = list_create();
	instrucciones* instruc = malloc(sizeof(instruccion));
	uint8_t nombre_instruccion = -1;
	size_t size = 0;
	bool exit = false;
	//instrucciones* instruc = malloc(sizeof(instrucciones));
	log_info(logger, "por obtener lista instrucciones");
	//buffer_unpack(bufferInstrucciones, instruc,sizeof(instrucciones));
	log_info(logger, "hice el unpack");

	char* paramatroNull = "";
	 	 	char* parametro1 = malloc(20*sizeof(char));
			char* parametro2 = malloc(20*sizeof(char));
			char* parametro3 = malloc(20*sizeof(char));
	while(! exit){
		tipo_instruccion_consola* instruccionAnalisis = malloc(sizeof(tipo_instruccion_consola));
		buffer_unpack(bufferInstrucciones, instruccionAnalisis,sizeof(tipo_instruccion_consola));
		log_info(logger, "instruccion: %u", *instruccionAnalisis);
		switch(*instruccionAnalisis){
		case INSTRUC_F_READ:
			log_info(logger, "obteniendo Read");
			parametro1 =  buffer_unpack_string(bufferInstrucciones);
			parametro2 =  buffer_unpack_string(bufferInstrucciones);
			parametro3 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "obteniendo %s", parametro1);
			log_info(logger, "obteniendo %s", parametro2);
			log_info(logger, "obteniendo %s", parametro3);
			break;
		case INSTRUC_F_WRITE:
			log_info(logger, "obteniendo Write");
			parametro1 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			parametro2 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro2);
			parametro3 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro3);
			log_info(logger, "obteniendo %s", parametro1);
			break;
		case INSTRUC_SET:
			log_info(logger, "obteniendo Set");
			parametro1 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			parametro2 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "obteniendo %s", parametro1);
			log_info(logger, "set tiene como segundo valor %s", parametro2);
			log_info(logger, "obteniendo %s", parametro2);
			break;
		case INSTRUC_MOV_IN:
			log_info(logger, "obteniendo Mov In");
			parametro1 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			parametro2 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como segundo valor %s", parametro2);
			break;
		case INSTRUC_MOV_OUT:
			log_info(logger, "obteniendo Mov Out");
			parametro1 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			parametro2 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro2);
			break;
		case INSTRUC_F_TRUNCATE:
			log_info(logger, "obteniendo Truncate");
			parametro1 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			parametro2 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro2);
			break;
		case INSTRUC_F_SEEK:
			log_info(logger, "obteniendo Seek");
			parametro1 =  buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			parametro2 = buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro2);
			break;
		case INSTRUC_CREATE_SEGMENT:
			log_info(logger, "obteniendo Create Segment");
			parametro1 = buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			parametro2 = buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro2);
			break;
		case INSTRUC_IO:
			log_info(logger, "obteniendo IO");
			parametro1 = buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			break;
		case INSTRUC_WAIT:
			log_info(logger, "obteniendo Wait");
			parametro1 = buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			break;
		case INSTRUC_SIGNAL:
			log_info(logger, "obteniendo Signal");
			parametro1 = buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			break;
		case INSTRUC_F_OPEN:
			log_info(logger, "obteniendo Fopen");
			parametro1 = buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			break;
		case INSTRUC_F_CLOSE:
			log_info(logger, "obteniendo Fclose");
			parametro1 = buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			break;
		case INSTRUC_DELETE_SEGMENT:
			log_info(logger, "obteniendo Delete Segment");
			parametro1 = buffer_unpack_string(bufferInstrucciones);
			log_info(logger, "set tiene como primer valor %s", parametro1);

			break;
		case INSTRUC_EXIT:
			log_info(logger, "obteniendo Exit");
			exit = true;
			break;
		case INSTRUC_YIELD:
			log_info(logger, "obteniendo Yield");
			break;
		default:
			log_error(logger,"Error al intentar desempaquetar una instrucción");

		}
		instrucciones* instruccion_actual = armar_instruccion(*instruccionAnalisis, parametro1, parametro2, parametro3);

		list_add(lista_instrucciones, instruccion_actual);
		//buffer_unpack(bufferInstrucciones, &instruc,sizeof(instrucciones));
	}
	for(int i = 0; i< list_size(lista_instrucciones); i++){
		instrucciones* aux =  list_get(lista_instrucciones,i);
					log_info(logger, "numero instruccion  %u " , aux->instruccion_nombre);
				}
	return lista_instrucciones;
}


tabla_de_archivos* crear_tabla_de_archivos(char* nombre_archivo) {
    tabla_de_archivos* archivoNuevo = malloc(sizeof(tabla_de_archivos));
    if (archivoNuevo != NULL) {
    	perror("esta entrando bien");

        archivoNuevo->archivo = malloc(strlen(nombre_archivo) + 1);
        if (archivoNuevo->archivo != NULL) {
        	perror("esta entrando bien");
            strcpy(archivoNuevo->archivo, nombre_archivo);
            printf("SE ESTA ESCRIBIENDO %s", archivoNuevo->archivo);
            archivoNuevo->mutexArchivo = malloc(sizeof(pthread_mutex_t));
            if (archivoNuevo->mutexArchivo != NULL) {
                pthread_mutex_init(archivoNuevo->mutexArchivo, NULL);
                archivoNuevo->pcb_en_espera = list_create();
                archivoNuevo->posicion_puntero = 0;
            } else {
            	perror("libera1");
                free(archivoNuevo->archivo);
                free(archivoNuevo);
                archivoNuevo = NULL;
            }
        } else {
        	perror("libera2");
            free(archivoNuevo);
            archivoNuevo = NULL;
        }
    } else {
    	perror("esta entrando siempre aca");
    }
    return archivoNuevo;
}
/*
void free_tabla_de_archivos(tabla_de_archivos* archivo) {
    if (archivo != NULL) {
        free(archivo->archivo);
        free(archivo->mutexArchivo);
        list_destroy(archivo->pcb_en_espera);
        free(archivo);
    }
}*/
void free_tabla_de_archivos(tabla_de_archivos* archivo) {
    if (archivo != NULL) {
        free(archivo->archivo);
       if (archivo->mutexArchivo != NULL) {
            pthread_mutex_destroy(archivo->mutexArchivo);
            free(archivo->mutexArchivo);
        }
        if (archivo->pcb_en_espera != NULL) {
            list_destroy_and_destroy_elements(archivo->pcb_en_espera, free);
        }

        free(archivo);
    }
}

cpuResponse* desempaquetar_cpu_response(t_buffer* bufferCpu, t_log* cpu_logger){
	cpuResponse* ret = malloc(sizeof(cpuResponse));
	log_info(cpu_logger, "entre desempaquetar");
	buffer_unpack(bufferCpu,&ret->pid, sizeof(int));
	buffer_unpack(bufferCpu,&ret->pc, sizeof(int));
	//desempaquetar_registros_cpu(bufferCpu, ret);
	log_info(cpu_logger, "program counter: %u", ret->pc);
	ret->registros = malloc(sizeof(registros_cpu));
	buffer_unpack(bufferCpu,ret->registros, sizeof(ret->registros));
	int* tam_lista_instruc = malloc(sizeof(int));
	buffer_unpack(bufferCpu,tam_lista_instruc, sizeof(int));
	log_info(cpu_logger, "tamanio de la lista antes desempaquetar: %d", *tam_lista_instruc);

	ret->lista_instrucciones = desempaquetar_lista_instrucciones(bufferCpu,*tam_lista_instruc,cpu_logger);
	for(int i = 0; i< list_size(ret->lista_instrucciones); i++){
			instrucciones* aux =  list_get(ret->lista_instrucciones,i);
						log_info(bufferCpu, "numero instruccion  %u " , aux->instruccion_nombre);
		}
	log_info(cpu_logger, "lista de instruccion ya desempaquetada size %d", list_size(ret->lista_instrucciones));

	int* tam_lista_segmentos = malloc(sizeof(int));
	buffer_unpack(bufferCpu,tam_lista_segmentos, sizeof(int));
	ret->tabla_segmentos = desempaquetar_lista_segmentos(bufferCpu,*tam_lista_segmentos,cpu_logger);
	log_info(cpu_logger, "pase tabla de segmentos ");

	ret->archivo = buffer_unpack_string(bufferCpu);
	buffer_unpack(bufferCpu,&ret->flag, sizeof(ret->flag));
	log_info(cpu_logger, "pase flags ");
	buffer_unpack(bufferCpu,&ret->tiempo, sizeof(ret->tiempo));
	log_info(cpu_logger, "pase tiempo ");
	ret->recurso = buffer_unpack_string(bufferCpu);
	log_info(cpu_logger, "pase recursos ");
	buffer_unpack(bufferCpu,&ret->sizeSegmento, sizeof(ret->sizeSegmento));
	log_info(cpu_logger, "pase sizesegmentos ");
	buffer_unpack(bufferCpu,&ret->id_segmento, sizeof(ret->id_segmento));
	log_info(cpu_logger, "pase idsegmento ");
	buffer_unpack(bufferCpu,&ret->posicionPuntero, sizeof(ret->posicionPuntero));
	log_info(cpu_logger, "pase posicionPuntero ");
	buffer_unpack(bufferCpu,&ret->direccionFisica, sizeof(ret->direccionFisica));
	log_info(cpu_logger, "pase df ");

	return ret;
}

t_list* desempaquetar_lista(t_buffer* buffer,int longitud,void* tipo_dato, t_log* cpu_logger){
	t_list* list_ret = list_create();
	log_info(cpu_logger, "numero de la lista desempaquetado %d ", longitud);

	for(int i=0;i<longitud; i++){
		buffer_unpack(buffer,tipo_dato, sizeof(*tipo_dato));
		list_add(list_ret, tipo_dato);
		log_info(cpu_logger, "numero de la lista size %d ", list_size(list_ret));
		log_info(cpu_logger, "numero de la lista desmpaquetado %d ", i);
	}
	log_info(cpu_logger, "todo listo amigo ");

	return list_ret;
}

void empaquetar_instruccion(instrucciones* instruc, t_buffer* buffer){
	if(instruc->parametro1 == NULL){
		buffer_pack(buffer,&instruc->instruccion_nombre, sizeof(tipo_instruccion_consola));
	}
	else if(instruc->parametro2 == NULL){
		buffer_pack(buffer,&instruc->instruccion_nombre, sizeof(tipo_instruccion_consola));
		buffer_pack_string(buffer, instruc->parametro1);
		}
	else if(instruc->parametro3 == NULL){
		buffer_pack(buffer,&instruc->instruccion_nombre, sizeof(tipo_instruccion_consola));
		buffer_pack_string(buffer,instruc->parametro1);
		buffer_pack_string(buffer,instruc->parametro2);
			}
	else{
		buffer_pack(buffer,&instruc->instruccion_nombre, sizeof(tipo_instruccion_consola));
		buffer_pack_string(buffer, instruc->parametro1);
		buffer_pack_string(buffer, instruc->parametro2);
		buffer_pack_string(buffer, instruc->parametro3);
	}
}

t_list* desempaquetar_lista_instrucciones(t_buffer* buffer,int longitud, t_log* logger){
	t_list* list_ret = list_create();
	char* parametro1 = malloc(20*sizeof(char));
	char* parametro2 = malloc(20*sizeof(char));
	char* parametro3 = malloc(20*sizeof(char));
	log_info(logger, "numero de la lista desempaquetado %d ", longitud);
	tipo_instruccion_consola* instruccion_nombre = malloc(sizeof(tipo_instruccion_consola));
	int parametros = 0;
	for(int i=0;i<longitud; i++){
		buffer_unpack(buffer,instruccion_nombre, sizeof(tipo_instruccion_consola));
		parametros = cuantosParametros(*instruccion_nombre);
		if(parametros == 3){
			parametro1 = buffer_unpack_string(buffer);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			parametro2 = buffer_unpack_string(buffer);
			log_info(logger, "set tiene como segundo valor %s", parametro2);
			parametro3 = buffer_unpack_string(buffer);
			log_info(logger, "set tiene como tercer valor %s", parametro3);
		}
		else if(parametros == 2){
			parametro1 = buffer_unpack_string(buffer);
			log_info(logger, "set tiene como primer valor %s", parametro1);
			parametro2 = buffer_unpack_string(buffer);
			log_info(logger, "set tiene como segundo valor %s", parametro2);
		}
		else if(parametros == 1){
			parametro1 = buffer_unpack_string(buffer);
			log_info(logger, "set tiene como primer valor %s", parametro1);
		}
		/*list_add(list_ret, tipo_dato);
		log_info(cpu_logger, "numero de la lista size %d ", list_size(list_ret));
		log_info(cpu_logger, "numero de la lista desmpaquetado %d ", i);*/
		instrucciones* instruccion_actual = armar_instruccion(*instruccion_nombre, parametro1, parametro2, parametro3);

		list_add(list_ret, instruccion_actual);

	}

	log_info(logger, "todo listo amigo ");

	return list_ret;
	free(parametro1);
	free(parametro2);
	free(parametro3);
}

int cuantosParametros(tipo_instruccion_consola instruccion_nombre){
	if(instruccion_nombre == INSTRUC_F_READ || instruccion_nombre == INSTRUC_F_WRITE){
		return 3;
	}
	else if(instruccion_nombre == INSTRUC_IO || instruccion_nombre == INSTRUC_WAIT || instruccion_nombre == INSTRUC_SIGNAL || instruccion_nombre == INSTRUC_F_OPEN || instruccion_nombre == INSTRUC_DELETE_SEGMENT || instruccion_nombre == INSTRUC_F_CLOSE   ){
		return 1;
	}
	else if(instruccion_nombre == INSTRUC_EXIT || instruccion_nombre == INSTRUC_YIELD){
		return 0;
	}
	else{
		return 2;
	}
}

void empaquetar_lista_instrucciones(t_buffer* buffer,int longitud,t_list* listaInstrucciones, t_log* logger){

	log_info(logger, "numero de la lista desempaquetado %d ", longitud);
	int parametros = 0;
	for(int i=0;i<longitud; i++){
		instrucciones* aux = list_get(listaInstrucciones, i);
		buffer_pack(buffer,&aux->instruccion_nombre, sizeof(tipo_instruccion_consola));
		parametros = cuantosParametros(aux->instruccion_nombre);
		if(parametros == 3){
			buffer_pack_string(buffer,aux->parametro1 );
			buffer_pack_string(buffer,aux->parametro2 );
			buffer_pack_string(buffer,aux->parametro3 );
		}
		else if(parametros == 2){
			buffer_pack_string(buffer,aux->parametro1 );
			buffer_pack_string(buffer,aux->parametro2 );
		}
		else if(parametros == 1){
			buffer_pack_string(buffer,aux->parametro1 );
		}
	}

	log_info(logger, "todo listo amigo ");
}

void empaquetar_segmento(t_buffer* buffer, tabla_segmento* seg){
	buffer_pack(buffer,&seg->id, sizeof(int));
	buffer_pack(buffer, &seg->idSegPcb, sizeof(int));
	buffer_pack(buffer, &seg->pid, sizeof(int));
	buffer_pack(buffer, &seg->base, sizeof(int));
	buffer_pack(buffer, &seg->limite, sizeof(int));
}

void empaquetar_lista_segmentos( t_buffer* buffer, int longitud, t_list* tablaDeSegmentos, t_log* logger){
	log_info(logger,"estoy en empaquetar lista");
	for(int i=0; i<longitud; i++){
		log_info(logger,"entre al for");
		tabla_segmento* elem = list_get(tablaDeSegmentos,i);
		buffer_pack(buffer,&elem->id, sizeof(int));
		buffer_pack(buffer,&elem->idSegPcb, sizeof(int));
		buffer_pack(buffer,&elem->pid, sizeof(int));
		buffer_pack(buffer,&elem->base, sizeof(int));
		buffer_pack(buffer,&elem->limite, sizeof(int));
		log_info(logger,"por empaquetar segmento");
		log_info(logger,"segmento base: %d", elem->base);
		log_info(logger,"segmento id: %d", elem->id);
		log_info(logger,"segmento limite: %d", elem->limite);
	}
	log_info(logger,"sali");

}

tabla_segmento* desempaquetar_segmento(t_buffer* buffer, t_log* logger){
	tabla_segmento* ret = malloc(sizeof(segmento));
	buffer_unpack(buffer,&ret->id, sizeof(int));
		buffer_unpack(buffer,&ret->idSegPcb, sizeof(int));
		buffer_unpack(buffer,&ret->pid, sizeof(int));
		buffer_unpack(buffer,&ret->base, sizeof(int));
		buffer_unpack(buffer,&ret->limite, sizeof(int));
		log_info(logger,"por desempaquetar segmento");
		log_info(logger, "segmento base desempaquetado : %d", ret->base);
		log_info(logger, "segmento id desempaquetado : %d", ret->id);
		log_info(logger, "segmento limite desempaquetado : %d", ret->limite);
	return ret;
}

t_list* desempaquetar_lista_segmentos(t_buffer* buffer,int longitud, t_log* logger){
	t_list* ret = list_create();
	tabla_segmento* aux = malloc(sizeof(tabla_segmento));
	for(int i=0; i<longitud;i++){
		aux = desempaquetar_segmento(buffer, logger);
		//aux->segmento = desempaquetar_segmento(buffer);
		list_add(ret, aux);
	}
	return ret;
}

