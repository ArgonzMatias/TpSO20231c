#ifndef MEMORIA_KERNEL_H_
#define MEMORIA_KERNEL_H_

#include "memoria.h"



void* peticiones_kernel(void* socketKernel);
void* peticiones_cpu(void* socketCpu);
void* peticiones_fileSystem(void* socketFile);
void m_read(int socket, char* fuente);
void m_write(void *socket, char* fuente);
void* stream_tabla_actualizada();

#endif
