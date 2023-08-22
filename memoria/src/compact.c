#include "compact.h"



extern char* memoria_Principal;
extern t_log* logger_memoria;
extern memoria_config* memoria_configs;
extern t_bitarray* bitMapSegment;

extern pthread_mutex_t mutexBitMap;
extern pthread_mutex_t mutexMemory;

void compact(){
	//busco segmentos ocupados
	log_info(logger_memoria, "Solicitud de Compactación");
	t_list* segmentosNoCompactados = list_create();
	pthread_mutex_lock(&mutexMemory);
	for(int i = 0 ; i<list_size(get_tabla_segmentos()); i++){
		tabla_segmento* auxSegmento = list_get(get_tabla_segmentos(), i);
		list_add(segmentosNoCompactados, auxSegmento);
	}
	pthread_mutex_unlock(&mutexMemory);

	t_list* copySegmentos = list_map(segmentosNoCompactados, (void*)copiarSegmentos);

	liberarBitMap(bitMapSegment, 0, memoria_configs->tam_memoria);
	int sizeNoCompact = list_size(segmentosNoCompactados);

	t_list* segmentosCompact = list_create();

	for(int i = 0; i<sizeNoCompact; i++){
		tabla_segmento* seg1 = list_get(segmentosNoCompactados,i);
		segmento* segNew = guardarDato(list_get(copySegmentos,i), seg1->limite);
		tabla_segmento* segNuevo = tablaSegmentoCrear(seg1->pid, segNew);
		free_tabla_de_Segmento(seg1);
		list_add(segmentosCompact, segNew);
	}

	for(int i = 0; i < list_size(segmentosNoCompactados); i++ ){
		pthread_mutex_lock(&mutexMemory);
		tabla_segmento* segViejo = list_get(segmentosNoCompactados, i);
		tabla_segmento* segNuevo = list_get(segmentosCompact, i);
		log_info(logger_memoria, "Compactando: PID: %d - Segmento: %d - Base: %d - Tamaño %d", segNuevo->pid, segNuevo->id, segNuevo->base, segNuevo->limite);
		pthread_mutex_unlock(&mutexMemory);


	int segmentoBuscado(tabla_segmento* tablaAux){
		return (tablaAux->id ==  segViejo->id);
	}

	if(list_find(get_tabla_segmentos(), (void*) segmentoBuscado) != NULL ){
		tabla_segmento* tablaAux2 = list_find(get_tabla_segmentos(), (void*) segmentoBuscado);
		if(tablaAux2->id == segViejo->id){
			tablaAux2->base = segNuevo->base;
		}

	}
	free_tabla_de_Segmento(segViejo);
	free_tabla_de_Segmento(segNuevo);
	}

}

void* copiarSegmentos(segmento* unSeg){
	void* aux;
	aux = malloc(sizeof(unSeg->limite ));
	pthread_mutex_lock(&mutexMemory);
	memcpy(aux, memoria_Principal + unSeg->base, unSeg->limite);
	pthread_mutex_unlock(&mutexMemory);
	return aux;
}
