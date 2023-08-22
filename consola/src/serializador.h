#ifndef SERIALIZADOR_H_
#define SERIALIZADOR_H_

#include "utils/buffer.h"
#include "console.h"
#include "utils/flags.h"

void seralizar_zero_parametro(t_buffer* buffer, tipo_instruccion_consola instruccion);
void seralizar_un_parametro(t_buffer* buffer, tipo_instruccion_consola instruccion, char op1[50]);
void seralizar_dos_parametro(t_buffer* buffer, tipo_instruccion_consola instruccion, char op1[50], char op2[50]);
void seralizar_tres_parametro(t_buffer* buffer, tipo_instruccion_consola instruccion, char op1[50], char op2[50], char op3[50]);
void buffer_armar(t_buffer* self, void* streamToAdd, int size);

#endif

