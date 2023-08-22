#include "filesystem.h"

#define PATH_LOG_FS "filesystem.log"
#define PATH_CONFIG_FS "filesystem.config"

uint32_t long_maxima_instruccion;

t_log* logger_fileSystem;
filesystem_config* filesystem_configs;
t_bitarray* bitmap;
void* archivo_bloques;
super_bloque* superbloque;

int main(int argc, char *argv[]) {
	logger_fileSystem = log_create(PATH_LOG_FS, "FILESYSTEM", 1, LOG_LEVEL_DEBUG);
	filesystem_configs = malloc(sizeof(*filesystem_configs));
	config_init(filesystem_configs, PATH_CONFIG_FS, logger_fileSystem, fs_config_iniciar);

	const int memoriaSocket = conectar_a_servidor(filesystem_configs->ip_memoria,filesystem_configs->puerto_memoria);
		if (memoriaSocket == -1) {
			log_error(logger_fileSystem,"Error al intentar establecer conexión inicial con módulo Memoria");
			log_destroy(logger_fileSystem);
			return -1;
		}

	filesystem_configs->socketMemoria = memoriaSocket;

	stream_send_empty_buffer(memoriaSocket, hs_filesystem);
	 uint8_t memoriaResponse = stream_recv_header(memoriaSocket);
	    //t_buffer* bufferMemoria = buffer_create();
	    stream_recv_empty_buffer(filesystem_configs->socketMemoria);
	    if (memoriaResponse != hs_aux) {
	        log_error(logger_fileSystem, "Error al intentar establecer Handshake inicial con módulo Memoria");
	        log_destroy(logger_fileSystem);
	        return -1;
	    }
	    superbloque = malloc(sizeof(*superbloque));
	    char* newpath = filesystem_configs->path_superbloque;
	    config_init(superbloque, newpath, logger_fileSystem, super_bloque_iniciar);//aca rompe  -> arreglado por mati 28/7/2023 1:38 am (aka mi cumple gil)
	    crearBitMap();
	    levantar_archivo_bloques();

	    //recorrer directorio de FCB y generar estructuras
	    leerFCBs();
	int socketFS = iniciar_servidor(filesystem_configs->puerto_escucha);

	int socketKernel = aceptar_conexion_server(socketFS);
	filesystem_configs->socketKernel = socketKernel;
	handsakes kernelHeader = stream_recv_header(socketKernel);
	    if(kernelHeader != hs_filesystem){
			log_error(logger_fileSystem,"Error al intentar establecer Handshake inicial con módulo Kernel");
			log_destroy(logger_fileSystem);
			return -1;
	    }
	    stream_recv_empty_buffer(socketKernel);

	    //mandar hs a kernel
	    stream_send_empty_buffer(socketKernel, hs_aux);

	    int socketFSBlocked = aceptar_conexion_server(socketFS);
	    filesystem_configs->socketFSBlocked = socketFSBlocked;
	    handsakes kernelBlockedHeader = stream_recv_header(socketFSBlocked);
	    	    if(kernelBlockedHeader != hs_fs_blocked){
	    			log_error(logger_fileSystem,"Error al intentar establecer Handshake inicial con módulo Kernel");
	    			log_destroy(logger_fileSystem);
	    			return -1;
	    	    }
	    	    stream_recv_empty_buffer(socketFSBlocked);

	    	    //mandar hs a kernel
	    	    stream_send_empty_buffer(socketFSBlocked, hs_aux);


	pthread_t  th_atender_kernel;
	log_info(logger_fileSystem, "por crear el hilo");
	pthread_create(&th_atender_kernel, NULL, (void*)atenderKernel, NULL);
	pthread_join(th_atender_kernel, NULL);

	return 0;
}


//instrucciones que le manda kernell que hay que hacer un switch con fopen,
//fread,fwrite, fclose y fseek y usar las funciones de operaciones
void atenderKernel(){
	while(1){
		log_info(logger_fileSystem, "entre a atenderKernel");
	instruccion solicitudKernel = stream_recv_header(filesystem_configs->socketKernel);
	log_info(logger_fileSystem, "pase solicitudKernel");
	rtaKernelFs rtaKernel = recv_peticion_fs_kernel();
	log_info(logger_fileSystem, "por entrar al switch");
	int pid = 0;
	switch(solicitudKernel){
	case I_OPEN:
		if(buscarArchivo(rtaKernel.nombre_archivo)){
			log_info(logger_fileSystem, "Abrir Archivo: %s", rtaKernel.nombre_archivo);

			stream_send_empty_buffer(filesystem_configs->socketKernel,header_general);
		}
		else{
			log_info(logger_fileSystem, "Archivo: %s no existe, debe crearlo", rtaKernel.nombre_archivo);
			stream_send_empty_buffer(filesystem_configs->socketKernel,header_nuevo_archivo_filesystem);
		}
		break;
	case I_CREATE_ARCHIVO:
		log_info(logger_fileSystem, "Crear Archivo: %s", rtaKernel.nombre_archivo);
		crearFCB(rtaKernel.nombre_archivo);
		stream_send_empty_buffer(filesystem_configs->socketKernel, hs_aux);
		break;
	case I_READ:
		pid = recibirPidBLoqueado();
	void* info = leer_archivo(rtaKernel, filesystem_configs, superbloque, archivo_bloques);
	//enviar a memoria la info con la direccion fisica

	//le envio la direccion, size y valor
	t_buffer* bufferDirecArchivo = buffer_create();
	buffer_pack(bufferDirecArchivo, &rtaKernel.direccionFisica, sizeof(&rtaKernel.direccionFisica));
	buffer_pack(bufferDirecArchivo, &rtaKernel.size, sizeof(&rtaKernel.size));
	buffer_pack(bufferDirecArchivo, info, rtaKernel.size);
	buffer_pack(bufferDirecArchivo, rtaKernel.pid, sizeof(&rtaKernel.pid) );
	stream_send_buffer(filesystem_configs->socketMemoria , I_WRITE, bufferDirecArchivo);
	buffer_destroy(bufferDirecArchivo);

	handsakes rtaMemoriaRead = stream_recv_header(filesystem_configs->socketMemoria);
	stream_recv_empty_buffer(filesystem_configs->socketMemoria);
	if(rtaMemoriaRead != hs_aux){
		log_error(logger_fileSystem, "error en el fread al recibir rta de memoria a filesystem");
	}


	log_info(logger_fileSystem, "Leer Archivo: %s - Puntero: %u - Memoria: %d - Tamaño: %d", rtaKernel.nombre_archivo, rtaKernel.puntero, rtaKernel.direccionFisica, rtaKernel.size);

	//recibir confirmacion de memoria
	//mandar a kernel q se termino
	liberarPcb(pid);
	/*mensaje_enviado = "el archivo fue leido";

	paquete = crear_paquete();
	agregar_a_paquete(paquete, mensaje_enviado, sizeof(char*));
	enviar_paquete(paquete, socketKernel);*/



	break;

	case I_WRITE:
		//pedir a memoria la info a escribir(a memoria le paso la direccion fisica)
		pid = recibirPidBLoqueado();
		t_buffer *buffer_send_mem= buffer_create();
		buffer_pack(buffer_send_mem, &rtaKernel.direccionFisica, sizeof(rtaKernel.direccionFisica));
		buffer_pack(buffer_send_mem, &rtaKernel.size,	sizeof(rtaKernel.size));
		buffer_pack(buffer_send_mem, &rtaKernel.pid, sizeof(rtaKernel.pid));
		stream_send_buffer(filesystem_configs->socketMemoria, I_READ, buffer_send_mem);

		log_info(logger_fileSystem, "esperando respuesta de memoria");
		//recibo de memoria el valor a escribir(data_a_escribir)

		t_buffer *buffer_respuesta_mem = buffer_create();
		instruccion instrucc = stream_recv_header(filesystem_configs->socketMemoria);
		if(instrucc != I_READ){
			log_info(logger_fileSystem, "error en la recepcion del buffer i_write");
		}
		stream_recv_buffer(filesystem_configs->socketMemoria, buffer_respuesta_mem);
		char* data_a_escribir = buffer_unpack_string(buffer_respuesta_mem);
		//buffer_unpack(buffer_respuesta_mem, data_a_escribir, rtaKernel.size);
		buffer_destroy(buffer_respuesta_mem);
		log_info(logger_fileSystem, "Entrando a escribir archivo");

		escribir_archivo(rtaKernel, filesystem_configs, superbloque, archivo_bloques, data_a_escribir);
		log_info(logger_fileSystem, "Escribir Archivo: %s - Puntero: %u - Memoria: %d - Tamaño: %d", rtaKernel.nombre_archivo, rtaKernel.puntero, rtaKernel.direccionFisica, rtaKernel.size);
		liberarPcb(pid);

	break;
	case I_TRUNCATE:
		pid = recibirPidBLoqueado();
		log_info(logger_fileSystem, "Truncar Archivo: %s - Tamaño: %d", rtaKernel.nombre_archivo, rtaKernel.size);
		truncar_archivo(rtaKernel.nombre_archivo, rtaKernel.size, superbloque, archivo_bloques, bitmap, filesystem_configs);
		liberarPcb(pid);
	break;
	}
}
}

int recibirPidBLoqueado(){
	t_buffer* pidBUffer = buffer_create();
	int pid;

	stream_recv_header(filesystem_configs->socketFSBlocked);
	stream_recv_buffer(filesystem_configs->socketFSBlocked,pidBUffer);
	buffer_unpack(pidBUffer, &pid, sizeof(int));
	log_info(logger_fileSystem, "se bloqueo el pcb con pid %d ", pid);
	return pid;

}


void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion);
	/* Y por ultimo, hay que liberar lo que utilizamos (conexion, log y config)
	  con las funciones de las commons y del TP mencionadas en el enunciado */
}





rtaKernelFs recv_peticion_fs_kernel(){
	rtaKernelFs* archivo = malloc(sizeof(*archivo));
	log_info(logger_fileSystem, "entro a recibir peticion");
	t_buffer* archivoBuffer = buffer_create();
	stream_recv_buffer(filesystem_configs->socketKernel,archivoBuffer);
	log_info(logger_fileSystem, "reecibo buffer");
	buffer_unpack(archivoBuffer, &archivo->direccionFisica, sizeof(int));
	buffer_unpack(archivoBuffer, &archivo->pid, sizeof(int));
	buffer_unpack(archivoBuffer, &archivo->puntero, sizeof(uint32_t));
	buffer_unpack(archivoBuffer, &archivo->size, sizeof(int));
	archivo->nombre_archivo = buffer_unpack_string(archivoBuffer);
	log_info(logger_fileSystem, "unpackeo todo");
	buffer_destroy(archivoBuffer);
	rtaKernelFs ret = *archivo;
	free(archivo);
	return ret;
}

void liberarPcb(int pid){
	t_buffer* bufferLiberar = buffer_create();
	log_info(logger_fileSystem, "liberando el pcb con pid %d ", pid);
		buffer_pack(bufferLiberar, &pid, sizeof(int));
		stream_send_buffer(filesystem_configs->socketFSBlocked, header_general,bufferLiberar);
		buffer_destroy(bufferLiberar);
}


static void fs_config_iniciar(void* moduleConfig, t_config* config){
	filesystem_config* fsConfig = (filesystem_config*)moduleConfig; //Logre arreglarlo parece
	fsConfig->puerto_escucha = strdup(config_get_string_value(config, "PUERTO_ESCUCHA"));
	fsConfig->ip_memoria = strdup(config_get_string_value(config, "IP_MEMORIA"));
	fsConfig->puerto_memoria = strdup(config_get_string_value(config, "PUERTO_MEMORIA"));
	fsConfig->path_superbloque = strdup(config_get_string_value(config, "PATH_SUPERBLOQUE"));
	fsConfig->path_bitmap = strdup(config_get_string_value(config, "PATH_BITMAP"));
	fsConfig->path_bloques = strdup(config_get_string_value(config, "PATH_BLOQUES"));
	fsConfig->path_FCB = strdup(config_get_string_value(config, "PATH_FCB"));
	fsConfig->retardo_acceso_bloque = config_get_int_value(config, "RETARDO_ACCESO_BLOQUE");
	fsConfig->socketKernel = -1;
	fsConfig->socketFS = -1;
	fsConfig->socketMemoria = -1;
	fsConfig->socketFSBlocked = -1;
}

static void super_bloque_iniciar(void* moduleConfig, t_config* config){
	super_bloque* superbloque = (super_bloque*)moduleConfig;
	superbloque->block_count = config_get_int_value(config, "BLOCK_COUNT");
	superbloque->block_size = config_get_int_value(config, "BLOCK_SIZE");
}

static void fcb_iniciar(void* moduleConfig, t_config* config){
	fcb* fcb_archivo = (fcb*)moduleConfig;
	fcb_archivo->nombreArchivo = strdup(config_get_string_value(config,"NOMBRE_ARCHIVO")); // agrego strdup pero me falla aca :(
	fcb_archivo->size = config_get_int_value(config,"TAMANIO_ARCHIVO");
	fcb_archivo->puntero_directo = config_get_int_value(config,"PUNTERO_DIRECTO");
	fcb_archivo->puntero_indirecto = config_get_int_value(config,"PUNTERO_INDIRECTO");
}

void crearBitMap(){
	int cant_bytes = superbloque->block_count/8; //cantidad de bytes necesarios para representar los bloques en bits
	int fd = open(filesystem_configs->path_bitmap, O_RDWR | O_CREAT, 0750);
	if(fd == -1){
		//error al abrir
		printf("NO SE PUDO ABRIR EL BITMAP");
	}

	struct stat f_stat;
	fstat(fd, &f_stat);
	if(f_stat.st_size == 0){
		if(ftruncate(fd, cant_bytes) == -1){
			//error al truncar
			printf("NO SE PUDO TRUNCAR EL BITMAP");
		}
	}
	char* bitarray = mmap(NULL, cant_bytes, PROT_READ| PROT_WRITE, MAP_SHARED, fd, 0);
	if(bitarray == MAP_FAILED){
			//Fallo mapeo
		}
	bitmap = bitarray_create_with_mode(bitarray, cant_bytes, LSB_FIRST);
	for(int i = 0; i< superbloque->block_count;i++){
		bitarray_clean_bit(bitmap,i);
		log_info(logger_fileSystem, "Acceso a Bitmap - Bloque: %d - Estado: 0", i);
	}

	close(fd);


}
void leerFCBs(){
	DIR* dir = opendir(filesystem_configs->path_FCB);

	struct dirent* newfile = readdir(dir); // fix, era q necesitaba crear la carpeta fcb lol
	if(strcmp(newfile->d_name, ".") != 0 && strcmp(newfile->d_name, "..") != 0){
		log_info(logger_fileSystem, "carpeta de fcbs vacia");
	}
	while(strcmp(newfile->d_name, ".") != 0 && strcmp(newfile->d_name, "..") != 0){
		// if (strcmp(newfile->d_name, ".") != 0 && strcmp(newfile->d_name, "..") != 0) {
		char* newpath = strdup(filesystem_configs->path_FCB);
		string_append(&newpath , "/");
		string_append(&newpath , newfile->d_name);
		fcb* fcb_archivo = malloc(sizeof(*fcb_archivo));;
		config_init(fcb_archivo, newpath, logger_fileSystem, fcb_iniciar);
		free(newpath);
		if(fcb_archivo->size > 0){
			marcarBloques(fcb_archivo);
		}
		newfile = readdir(dir);
		 //}
	}
}

bool buscarArchivo(char* archivo) {
    bool ret = false;
    DIR* dir = opendir(filesystem_configs->path_FCB);
    if (dir == NULL) {
    	log_info(logger_fileSystem, "path %s no existe", filesystem_configs->path_FCB);
        return false;
    }

    struct dirent* newfile = readdir(dir);
    while (newfile != NULL) {
        if (strcmp(newfile->d_name, archivo) == 0) {
            ret = true;
            break; // File found, no need to continue checking
        }
        newfile = readdir(dir);
    }

    closedir(dir); // Close the directory after use
    return ret;
}


/*
bool buscarArchivo(char* archivo){//podria cuando hago leer archivo cargar los fcbs y chequear directo ahi(mas facil)
	bool ret = false;
	DIR* dir = opendir(filesystem_configs->path_FCB);
		struct dirent* newfile = readdir(dir);
		while(newfile != NULL){
			if(strcmp(newfile->d_name, archivo) == 0){
				ret = true;
			}
			newfile = readdir(dir);
		}
	return ret;
}*/

void crearFCB(char* nombreArchivo){//si cambio lo de abrir archivo aca lo deberia agregar a la lista
	char* newpath = strdup(filesystem_configs->path_FCB);
	log_info(logger_fileSystem, "se agarro el path %s ", newpath);
	string_append(&newpath, "/");
	string_append(&newpath, nombreArchivo);
	log_info(logger_fileSystem, "abriendo el archivo");
	int fd = open(newpath, O_RDWR | O_CREAT, 0750);
	log_info(logger_fileSystem, "se abrio el archivo");
	close(fd);
	t_config* configFCB = config_create(newpath);
	config_set_value(configFCB, "NOMBRE_ARCHIVO", nombreArchivo);
	config_set_value(configFCB, "TAMANIO_ARCHIVO", "0");
	config_set_value(configFCB, "PUNTERO_DIRECTO", "0");
	config_set_value(configFCB, "PUNTERO_INDIRECTO", "0");
	config_save(configFCB);
	free(newpath);

}

void marcarBloques(fcb* fcb_archivo){
	uint32_t puntero_indirecto = fcb_archivo->puntero_indirecto;
	bitarray_set_bit(bitmap,fcb_archivo->puntero_directo);
	log_info(logger_fileSystem, "Acceso a Bitmap - Bloque: %u - Estado: 1", fcb_archivo->puntero_directo);
	bitarray_set_bit(bitmap,puntero_indirecto);
	log_info(logger_fileSystem, "Acceso a Bitmap - Bloque: %u - Estado: 1", fcb_archivo->puntero_indirecto);
	//habria que leer del archivo de bloques todos los bloques que referencia el puntero indirecto
	int cant_punteros = fcb_archivo->size/superbloque->block_size;
	cant_punteros--;//esto es porq ya marque el directo
	uint32_t* nuevo_puntero = malloc(sizeof(uint32_t));
	for(int i=0; i<cant_punteros; i++){
		memcpy(nuevo_puntero,archivo_bloques+(puntero_indirecto*superbloque->block_size + i*sizeof(uint32_t)),sizeof(uint32_t));//map del archivo de bloques y muevo el offset de los punteros
		bitarray_set_bit(bitmap,*nuevo_puntero);
		log_info(logger_fileSystem, "Acceso a Bitmap - Bloque: %u - Estado: 1", *nuevo_puntero);
	}
	free(nuevo_puntero);
}

void levantar_archivo_bloques(){
    int cant_bytes = superbloque->block_count * superbloque->block_size; //tamanio archivobloques
	int fd = open(filesystem_configs->path_bloques, O_RDWR | O_CREAT, 0750);
	if(fd == -1){
		//error al abrir
		printf("NO SE PUDO ABRIR EL BITMAP");
	}

	struct stat f_stat;
	fstat(fd, &f_stat);
	if(f_stat.st_size == 0){
		if(ftruncate(fd, cant_bytes) == -1){
			//error al truncar
			printf("NO SE PUDO TRUNCAR EL BITMAP");
		}
	}
 archivo_bloques = mmap(NULL, cant_bytes, PROT_READ| PROT_WRITE, MAP_SHARED, fd, 0);
	if(archivo_bloques == MAP_FAILED){
		log_info(logger_fileSystem, "fallo el mapeo del archivo de bloques");
		}
	close(fd);
}

//todo free del malloc del fcb config
