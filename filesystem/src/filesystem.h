#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include "utils/utils.h"
#include "utils/flags.h"
#include "utils/buffer.h"
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include<pthread.h>
#include "operaciones.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include<commons/bitarray.h>
#include <sys/types.h>
#include <dirent.h>


typedef struct {
	char* nombreArchivo;
	int size;
	uint32_t puntero_directo;
	uint32_t puntero_indirecto;
}fcb;

void liberarPcb(int rta);
void abrir_archivo_bloques ();
void abrir_superBloque();
void abrir_Bitmap();
void recibo_instrucciones(instruccion instruccion);
void terminar_programa(int conexion, t_log* logger, t_config* config);
void atenderKernel();
rtaKernelFs recv_peticion_fs_kernel();
void liberarPcb(int rta);
static void fs_config_iniciar(void*, t_config*);
static void super_bloque_iniciar(void*, t_config*);
void crearBitMap();
bool buscarArchivo(char*);
void levantar_archivo_bloques();
void marcarBloques(fcb*);
void crearFCB(char*);
void leerFCBs();
int recibirPidBLoqueado();

#endif /* FILESYSTEM_H_ */
