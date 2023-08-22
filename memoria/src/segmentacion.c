#include "segmentacion.h"


extern t_log* logger_memoria;
extern memoria_config* memoria_configs;
extern char* memoria_Principal; //toda la memoria general
extern char* datos;
extern t_bitarray* bitMapSegment;

pthread_mutex_t mutexBitMap;
pthread_mutex_t mutexMemory;
// memoria y segmento

tabla_segmento* crear_nuevo_segmento(int id, int size, int pid){
	t_list* tablaPid = tablaConPid(pid);
		/*if(list_size(tablaPid) == memoria_configs->cant_segmentos){
			log_error(logger_memoria, "maxima cantidad de segmentos alcanzado por pcb"); esta logica ya se chequea en el planificador.
			return NULL;
		}else{*/

	segmento* unSeg = create_segmento(id, size);
	if(unSeg != NULL){
	tabla_segmento* newSeg = tablaSegmentoCrear(pid, unSeg);
	return newSeg;
	}
	return NULL;
		//}
}

segmento* create_segmento(int id, int size){
	log_info(logger_memoria, "size en create segment %d", size);
		segmento* aux = buscarSegmentoSegunTamanio(size);
		if (aux == NULL){
			return NULL;
		} else{
		segmento* unSeg = malloc(sizeof(segmento));


	    unSeg->base = aux->base;
	    ocuparBitMap(bitMapSegment, unSeg->base, size);
	    unSeg->id = id;
	    unSeg->limite = size;
	    //aumentar_prox_id(); //aumento el id para el proximo que se use.
	    return unSeg;
	    free(aux);
		}
}

tabla_segmento* tablaSegmentoCrear(int pid, segmento* unSeg){

	t_list* tablaPid = tablaConPid(pid);
		tabla_segmento* newSeg = malloc(sizeof(tabla_segmento)); // Allocate memory for the new tabla_segmento
		newSeg->id = unSeg->id;
		newSeg->base = unSeg->base;
		newSeg->limite = unSeg->limite;
		newSeg->pid =pid;
			if(list_is_empty(tablaPid)){
			newSeg->idSegPcb = 0;
		} else{ //if(list_size(tablaPid) < memoria_configs->cant_segmentos){
			newSeg->idSegPcb = list_size(tablaPid)+1;
			// todavia hay lugar par agregar segmentos
			}
	list_add(get_tabla_segmentos(), newSeg); // Add the segmento to the tabla de segmentos
	return newSeg;
}

void free_tabla_de_Segmento(tabla_segmento* tabla) {
    if (tabla != NULL) {
        free(tabla);
    }
}

t_list* tablaConPid(int pid){
	int mismo_id(tabla_segmento tablaAux){
						return(tablaAux.pid == pid);
					}

		t_list* tablaPid = list_map(get_tabla_segmentos(),(void*)mismo_id);
		return tablaPid;
}

segmento* buscarSegmentoSegunTamanio(int size){
	segmento* unSeg = malloc(sizeof(segmento));
	log_info(logger_memoria,"entrando en segmento segun tamanio");
	t_list* segmentosLibres = buscarSegmentoDisponible(); //buscan todos los segmentos vacios

	log_info(logger_memoria, "tamanio de la lista: %d ", list_size(segmentosLibres));

	t_list* posiblesSegmentos = puedenGuardar(segmentosLibres, size); // buscan de los segmentos libres, cuales tienen suficieinte espacio;
	if(list_is_empty(posiblesSegmentos)){
		log_info(logger_memoria, "Solicitud de compactacion");
		stream_send_empty_buffer(memoria_configs->socket_kernel, header_compactacion);
		uint8_t memoriaRta = stream_recv_header(memoria_configs->socket_kernel);
		if(memoriaRta != hs_aux){
			log_error(logger_memoria, "error recepcion de solicitud de compactacion ");
						memoria_destruir(memoria_configs, logger_memoria);
						exit(-1);
		}
		simulador_de_espera(memoria_configs->retardo_compactacion);
		compact();
		int sizeNuevoTabla = list_size(get_tabla_segmentos());
		t_buffer* bufferSizeTablaSegmentos = buffer_create();
		buffer_pack(bufferSizeTablaSegmentos, sizeNuevoTabla, sizeof(sizeNuevoTabla));
		stream_send_buffer(memoria_configs->socket_kernel,header_general,bufferSizeTablaSegmentos); //le mando el tamanio de la tabla de segmentos actualizada
		buffer_destroy(bufferSizeTablaSegmentos);

		t_buffer* bufferTablaSegmentos = buffer_create();
				buffer_pack(bufferTablaSegmentos, get_tabla_segmentos(), sizeof(get_tabla_segmentos()));
				stream_send_buffer(memoria_configs->socket_kernel,header_general,bufferTablaSegmentos); //le mando la tabla de segmentos actualizada
				buffer_destroy(bufferTablaSegmentos);

		unSeg  = buscarSegmentoSegunTamanio(size);
	} else if(list_size(posiblesSegmentos) ==1){
		stream_send_empty_buffer(memoria_configs->socket_kernel, header_general);
		unSeg = list_get(posiblesSegmentos, 0); //si solo hay un segmento dispo, elige este.
	} else if(list_size(posiblesSegmentos) > 1) {
		stream_send_empty_buffer(memoria_configs->socket_kernel, header_general);
		unSeg = elegirSegunCriterio(posiblesSegmentos, size);
	} else{
		stream_send_empty_buffer(memoria_configs->socket_kernel, header_out_of_memory);
		return NULL;
	}


	list_destroy(segmentosLibres);
	list_destroy(posiblesSegmentos);

	return unSeg;

}


segmento* guardarDato(void* data, int size){
	segmento* unSeg = malloc(sizeof(segmento));
	segmento* aux;
	aux = buscarSegmentoSegunTamanio(size);
	guardarEnMemory(data, aux, size);

	unSeg->id = aux->id;
	unSeg->base = aux->base;
	unSeg->limite = size;

	free(aux);

	return unSeg;

}



void guardarEnMemory(void* data, segmento* unSeg, int size){
	ocuparBitMap(bitMapSegment, unSeg->base , size);
	pthread_mutex_lock(&mutexMemory);
	memcpy(memoria_Principal + unSeg->base , data, size);
	pthread_mutex_unlock(&mutexMemory);

}

//inicio de funcion usado en buscarSegmentoSegunTamanio

t_list* puedenGuardar(t_list* segmentos, int size){
	log_info(logger_memoria,"fijandome si se puede guardar el segmento");

	log_info(logger_memoria,"size %d : ", size);
	bool puedoGuardarSeg(segmento* otroSeg){
		log_info(logger_memoria,"limite %d : ", otroSeg->limite);
		return(otroSeg->limite >= size );
	}
	t_list* aux =list_filter(segmentos, (void*)puedoGuardarSeg);
	return aux;
}

bool puedoGuardar(int tamanioPedido){
	int tamanioLibre = tamanioTotalDispo();
	log_info(logger_memoria, "Hay %d espacio libre, se solicito guardar %d", tamanioLibre, tamanioPedido);
	if(tamanioPedido <= tamanioLibre){
		return true;
	}
	return false;
}



int tamanioTotalDispo(){
	int contador = 0;
	int despl = 0;

	while(despl < memoria_configs->tam_memoria){
		pthread_mutex_lock(&mutexBitMap);
		if(bitarray_test_bit(bitMapSegment, despl)==0){
			contador++;
		}
		pthread_mutex_unlock(&mutexBitMap);
		despl++;
	}
	return contador;
}

//fin de funcion usado en buscarSegmentoSegunTamanio

t_list* buscarSegmentoDisponible(){
	t_list* segmentos = list_create();
	int base = 0;
	while(base < memoria_configs->tam_memoria){
		segmento* unSeg = buscarLugarDisponibles(&base);
		list_add(segmentos, unSeg);
		log_info(logger_memoria,"se encontro el segmento %d con base %d y limite %d", unSeg->id, unSeg->base, unSeg->limite);
		//base++;
	}

	return segmentos;
}


segmento* buscarLugarDisponibles(int* base){  //macheamos el segmento con el pcb en cuestion
	segmento* unSeg = malloc(sizeof(segmento));
	int size = 0;

	if(bitarray_test_bit(bitMapSegment, *base) == 1){
		int despl = contarEspaciosOcupadosDesde(bitMapSegment, *base);
		*base += despl;
	}
	//aca la base esta en el primer 0 libre
	size = contarEspaciosLibresDesde(bitMapSegment, *base);

	log_info(logger_memoria,"re loco buscar lugar disponible");


	unSeg->base = *base;
	unSeg->limite = size;

	*base += size;

	return unSeg;
}


//Funciones de manejo del bitmap de espacios libres/ocupados

//INICIO


void ocuparBitMap(t_bitarray* bitmap, int base, int limite){
	pthread_mutex_lock(&mutexBitMap);
	for(int i = base; i <= base + limite; i++){
		bitarray_set_bit(bitmap, i);
		}
	pthread_mutex_unlock(&mutexBitMap);
}

void liberarBitMap(t_bitarray* bitmap, int base, int limite){
	for(int i = base; i <= base + limite; i++){
	bitarray_clean_bit(bitmap, i);
	}
}


void liberarMemoria(){
	free(memoria_Principal);
	log_destroy(logger_memoria);
	config_destroy(memoria_configs);
}

//FIN

//ALGORITMO DE SELECCION    INICIO

segmento* elegirSegunCriterio(t_list* listaDeSegmentos, int size){
	segmento* unSeg;
	if( strcmp(memoria_configs->algoritmo_asignacion, "FIRST") == 0){
		unSeg = list_get(listaDeSegmentos,0); //agarro el primer segmento
		log_info(logger_memoria, "algoritmo First Fit Seleccionado");
	} else if(strcmp(memoria_configs->algoritmo_asignacion, "BEST") == 0){
		log_info(logger_memoria, "algoritmo Best Fit Seleccionado");
		unSeg = bestFit(listaDeSegmentos, size);
	} else if(strcmp(memoria_configs->algoritmo_asignacion, "WORST") == 0){
		log_info(logger_memoria, "algoritmo Worst Fit Seleccionado");
		unSeg = worstFit(listaDeSegmentos, size);
	}
}

segmento*  bestFit(t_list* listaDeSegmentos,int size){
	segmento* unSeg;

	segmento* segSameSize = list_find(listaDeSegmentos, (void*) igualTamanio(size));

	if(segSameSize != NULL){
		unSeg = segSameSize;
	}
	else{
		unSeg = list_get_minimum(listaDeSegmentos, (void*) igualTamanio(size));
	}
	return unSeg;
}

segmento*  worstFit(t_list* listaDeSegmentos,int size){
	segmento* unSeg;

	unSeg = list_get_maximum(listaDeSegmentos, (void*) igualTamanio(size));

	if(unSeg == NULL){
		log_error(logger_memoria, "error en worst fit");
	}
	return unSeg;
}

int igualTamanio(segmento* seg, int size){
		return(seg->limite == size);
	}


//FIN


// Bits y bytes asignacion  INICIO

char* asignarMemoriaBytes(int bytes){
	char* aux;
	aux = malloc(bytes);
	memset(aux,0,bytes);
	return aux;
}
char* asignarMemoriaBites(int bits){
	char* aux;
	int bytes;
	bytes = bitsToBytes(bits);
	aux = malloc(bytes);
	memset(aux,0,bytes);
	return aux;
}

int bitsToBytes(int bits){
	int bytes;
	if(bits < 8)
		bytes = 1;
	else
	{
		double c = (double) bits;
		bytes = ceil(c/8.0);
	}

	return bytes;
}

//FIN


void eleminarSegmento(int id_segmento){
	tabla_segmento* Tablaseg = findSegmentoConId(get_tabla_segmentos(), id_segmento);
	liberarBitMap(bitMapSegment, Tablaseg->base, Tablaseg->limite);
	int id_seg_compare(tabla_segmento* tablaAux){
			return(id_segmento == tablaAux->id);
		}
	tabla_segmento* elementoAEliminar = list_find(get_tabla_segmentos(),(void*)id_seg_compare);
	list_remove_element(get_tabla_segmentos(), elementoAEliminar);// lo saco de la lista de segmentos
}

int contarEspaciosOcupadosDesde(t_bitarray*unBitmap, int i){
	log_info(logger_memoria,"contando espacios ocupados");
    int contador =0;
    while((bitarray_test_bit(unBitmap, i) == 1) && (i < memoria_configs->tam_memoria)){
        contador++;
        i++;
    }
    return contador;
}

int contarEspaciosLibresDesde(t_bitarray* bitmap, int i){
	log_info(logger_memoria,"contando espacios libres");
    int contador = 0;
    pthread_mutex_lock(&mutexBitMap);
    while((bitarray_test_bit(bitmap, i)==0 ) && (i < (memoria_configs->tam_memoria))) {
    	contador ++;
        i++;
    }
    pthread_mutex_unlock(&mutexBitMap);
    return contador;
}
