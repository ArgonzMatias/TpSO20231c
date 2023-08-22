#ifndef SEGMENTACION_H_
#define SEGMENTACION_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include "utils/utils.h"
#include "pthread.h"
#include<commons/log.h>
#include<commons/collections/list.h>
#include <commons/bitarray.h>
#include "memoria.h"
#include <math.h>
#include <stdbool.h>



int funcion_hilo_memoria();
tabla_segmento* crear_nuevo_segmento(int id, int size, int pid);
segmento* create_segmento(int id, int size);
tabla_segmento* tablaSegmentoCrear(int pid, segmento* unSeg);
t_list* tablaConPid(int pid);
segmento* buscarSegmentoSegunTamanio(int size);
bool puedoGuardar(int tamanioPedido);
int tamanioTotalDispo();
void eleminarSegmento(int id_segmento);
t_list* puedenGuardar(t_list* segmentos, int size);
t_list* buscarSegmentoDisponible();
segmento* elegirSegunCriterio(t_list* listaDeSegmentos, int size);
segmento*  bestFit(t_list* listaDeSegmentos,int size);
segmento*  worstFit(t_list* listaDeSegmentos,int size);
void guardarEnMemory(void* data, segmento* unSeg, int size);
segmento* buscarLugarDisponibles(int* base);
int bitsToBytes(int bits);
void free_tabla_de_Segmento(tabla_segmento* tabla);

#endif /* MEMORIA_H_ */
