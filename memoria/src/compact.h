#ifndef COMPACT_H_
#define COMPACT_H_

#include <stdlib.h>
#include <commons/collections/list.h>
#include "utils/utils.h"
#include <memoria.h>



void compact();
void* copiarSegmentos(segmento* unSeg);
void liberarBitMap(t_bitarray* bitmap, int base, int limite);
#endif
