#ifndef OPERACIONES_H_
#define OPERACIONES_H_

#include <stdio.h>
#include "utils/utils.h"
#include "utils/flags.h"
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include<commons/bitarray.h>

typedef struct{
	int block_size;
	int block_count;
}super_bloque;

void truncar_archivo(char*, int, super_bloque*, void*, t_bitarray*,filesystem_config*);
uint32_t buscar_proximo_bloque_libre(t_bitarray*);
void* leer_archivo(rtaKernelFs , filesystem_config*,super_bloque*, void*);
uint32_t encontrar_bloque_n(rtaKernelFs,t_config*, int,void*,super_bloque*);
void escribir_archivo(rtaKernelFs, filesystem_config*,super_bloque*,void*, void*);

#endif /* OPERACIONES_H_ */
