#include "operaciones.h"

extern t_log* logger_fileSystem;

void truncar_archivo(char* archivo, int size, super_bloque* superbloque, void* archivo_bloques, t_bitarray* bitmap,filesystem_config* filesystem_configs)
{

	char* pathFCB = strdup(filesystem_configs->path_FCB);
	string_append(&pathFCB,"/");
	string_append(&pathFCB,archivo);

	t_config* configFCB = config_create(pathFCB);

	int tamanioArchivo = config_get_int_value(configFCB, "TAMANIO_ARCHIVO");
	uint32_t puntero_indirecto = config_get_int_value(configFCB, "PUNTERO_INDIRECTO");
	int bloques_actuales = tamanioArchivo/superbloque->block_size;
	if(tamanioArchivo > size){
		int bloques_a_quitar = bloques_actuales-(size/superbloque->block_size);

		uint32_t* puntero_a_sacar = malloc(sizeof(uint32_t));
		for(int i=0; i<bloques_a_quitar; i++){
			memcpy(puntero_a_sacar,archivo_bloques+(puntero_indirecto*superbloque->block_size + (bloques_actuales-i)*sizeof(uint32_t)),sizeof(uint32_t));//map del archivo de bloques y muevo el offset de los punteros
			bitarray_clean_bit(bitmap,*puntero_a_sacar);
			log_info(logger_fileSystem, "Acceso a Bitmap - Bloque: %d - Estado: 0", *puntero_a_sacar);
		}
		free(puntero_a_sacar);
	}
	else if(tamanioArchivo < size){
		int bloques_a_agregar = (size/superbloque->block_size)-bloques_actuales;

		for(int i=0; i<bloques_a_agregar;i++){
			uint32_t bloque_libre = buscar_proximo_bloque_libre(bitmap);
			if(bloques_actuales == 0){
				bitarray_set_bit(bitmap,bloque_libre);
				config_set_value(configFCB, "PUNTERO_DIRECTO", string_itoa(bloque_libre));
			}
			else if(bloques_actuales == 1){
				bitarray_set_bit(bitmap,bloque_libre);
				puntero_indirecto = bloque_libre;
				config_set_value(configFCB, "PUNTERO_INDIRECTO", string_itoa(bloque_libre));
			}
			else{
				bitarray_set_bit(bitmap,bloque_libre);
				memcpy(archivo_bloques+(puntero_indirecto*superbloque->block_size)+(bloques_actuales+i)*sizeof(uint32_t),&bloque_libre,sizeof(uint32_t));
			}
			log_info(logger_fileSystem, "Acceso a Bitmap - Bloque: %d - Estado: 1", bloque_libre);
		}
	}
	config_set_value(configFCB, "TAMANIO_ARCHIVO", string_itoa(size));//si lo hago con listas guardando los fcbs tambien la tengo que actualizar
	config_save(configFCB);
	config_destroy(configFCB);
	free(pathFCB);

}

uint32_t buscar_proximo_bloque_libre(t_bitarray* bitmap){
	uint32_t bloque_libre = 0;
	int cant_bits = bitarray_get_max_bit(bitmap);
	int estado = 0;
	for(uint32_t i=0; (i < cant_bits) && (estado == 1);i++){
		estado = bitarray_test_bit(bitmap,i)? 1:0;

		log_info(logger_fileSystem, "Acceso a Bitmap - Bloque: %u - Estado: %d", i,estado);
		bloque_libre = i;
	}
	return bloque_libre;
}

void escribir_archivo(rtaKernelFs rta_kernel, filesystem_config* filesystem_configs,super_bloque* superbloque,void* archivo_bloques, void* data_a_escribir){
	int escrito = 0;
	char* pathFCB = strdup(filesystem_configs->path_FCB);
	string_append(&pathFCB,"/");
	string_append(&pathFCB,rta_kernel.nombre_archivo);
	t_config* configFCB = config_create(pathFCB);
	int bloque_a_acceder = rta_kernel.puntero/superbloque->block_size;
	uint32_t bloque_n_archivo = encontrar_bloque_n(rta_kernel, configFCB, bloque_a_acceder, archivo_bloques, superbloque);
	int offset = rta_kernel.puntero % superbloque->block_size;
	free(pathFCB);

	memcpy(archivo_bloques+bloque_n_archivo*superbloque->block_size+offset,data_a_escribir,superbloque->block_size - offset);
	log_info(logger_fileSystem, "Acceso Bloque - Archivo: %s- Bloque Archivo: &u - Bloque File System %d",rta_kernel.nombre_archivo,bloque_n_archivo,bloque_a_acceder);

	escrito = superbloque->block_size - offset;
	rta_kernel.size = rta_kernel.size-offset;
	int cant_bloques_acceder = (rta_kernel.size)/superbloque->block_size;
	for(int i=0; i<cant_bloques_acceder;i++){
		bloque_a_acceder++;
		bloque_n_archivo = encontrar_bloque_n(rta_kernel, configFCB, bloque_a_acceder, archivo_bloques, superbloque);
		memcpy(archivo_bloques+bloque_n_archivo*superbloque->block_size,data_a_escribir+escrito,sizeof(uint32_t));
		log_info(logger_fileSystem, "Acceso Bloque - Archivo: %s- Bloque Archivo: &u - Bloque File System %d",rta_kernel.nombre_archivo,bloque_n_archivo,bloque_a_acceder);
		rta_kernel.size = rta_kernel.size-sizeof(uint32_t);
		escrito +=sizeof(uint32_t);
	}
	if(rta_kernel.size > 0){
		bloque_a_acceder++;
		bloque_n_archivo = encontrar_bloque_n(rta_kernel, configFCB, bloque_a_acceder, archivo_bloques, superbloque);
		memcpy(archivo_bloques+bloque_n_archivo*superbloque->block_size,data_a_escribir+escrito,rta_kernel.size);
		log_info(logger_fileSystem, "Acceso Bloque - Archivo: %s- Bloque Archivo: &u - Bloque File System %d",rta_kernel.nombre_archivo,bloque_n_archivo,bloque_a_acceder);
	}
}



void* leer_archivo(rtaKernelFs rta_kernel, filesystem_config* filesystem_configs,super_bloque* superbloque,void* archivo_bloques)
{
	char* pathFCB = strdup(filesystem_configs->path_FCB);
	string_append(&pathFCB,"/");
	string_append(&pathFCB,rta_kernel.nombre_archivo);
	t_config* configFCB = config_create(pathFCB);
	free(pathFCB);
	int bloque_a_acceder = rta_kernel.puntero/superbloque->block_size;
	uint32_t bloque_n_archivo = encontrar_bloque_n(rta_kernel, configFCB, bloque_a_acceder, archivo_bloques, superbloque);
	int offset = rta_kernel.puntero % superbloque->block_size;
	void* leido = malloc(rta_kernel.size);
	memcpy(leido,archivo_bloques+bloque_n_archivo*superbloque->block_size+offset,superbloque->block_size - offset);
	log_info(logger_fileSystem, "Acceso Bloque - Archivo: %s- Bloque Archivo: &u - Bloque File System %d",rta_kernel.nombre_archivo,bloque_n_archivo,bloque_a_acceder);

	rta_kernel.size = rta_kernel.size-offset;
	int cant_bloques_acceder = (rta_kernel.size)/superbloque->block_size;
	for(int i=0; i<cant_bloques_acceder;i++){
		bloque_a_acceder++;
		bloque_n_archivo = encontrar_bloque_n(rta_kernel, configFCB, bloque_a_acceder, archivo_bloques, superbloque);
		memcpy(leido,archivo_bloques+bloque_n_archivo*superbloque->block_size,sizeof(uint32_t));
		log_info(logger_fileSystem, "Acceso Bloque - Archivo: %s- Bloque Archivo: &u - Bloque File System %d",rta_kernel.nombre_archivo,bloque_n_archivo,bloque_a_acceder);
		rta_kernel.size = rta_kernel.size-sizeof(uint32_t);
	}
	if(rta_kernel.size > 0){
		bloque_a_acceder++;
		bloque_n_archivo = encontrar_bloque_n(rta_kernel, configFCB, bloque_a_acceder, archivo_bloques, superbloque);
		memcpy(leido,archivo_bloques+bloque_n_archivo*superbloque->block_size,rta_kernel.size);
		log_info(logger_fileSystem, "Acceso Bloque - Archivo: %s- Bloque Archivo: &u - Bloque File System %d",rta_kernel.nombre_archivo,bloque_n_archivo,bloque_a_acceder);
	}
	return leido;
}

uint32_t encontrar_bloque_n(rtaKernelFs rta_kernel,t_config* configFCB, int n,void* archivo_bloques,super_bloque* superbloque){
	uint32_t ret = config_get_int_value(configFCB, "PUNTERO_DIRECTO");
	if(n!=0){
		uint32_t puntero_indirecto = config_get_int_value(configFCB, "PUNTERO_INDIRECTO");
		  uint32_t* ptr = archivo_bloques+puntero_indirecto*superbloque->block_size+(n-1)*sizeof(uint32_t);
		     memcpy(&ret,&ptr,sizeof(uint32_t));
		//memcpy(&ret,archivo_bloques+puntero_indirecto*superbloque->block_size+(n-1)*sizeof(uint32_t),sizeof(uint32_t));
	}
	return ret;
}

