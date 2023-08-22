#include "serializador.h"

extern t_log* logger_consola;

void seralizar_zero_parametro(t_buffer* buffer, tipo_instruccion_consola instruccion){
	log_info(logger_consola,"instruccion recibida, armando buffer");
	buffer_armar(buffer, &instruccion, sizeof(&instruccion));
	log_info(logger_consola,"buffer listo");
}

void seralizar_un_parametro(t_buffer* buffer, tipo_instruccion_consola instruccion, char op1[50]){
	seralizar_zero_parametro(buffer, instruccion);
	buffer_armar(buffer, &op1, sizeof(&op1));
}

void seralizar_dos_parametro(t_buffer* buffer, tipo_instruccion_consola instruccion, char op1[50], char op2[50]){
	seralizar_un_parametro(buffer,instruccion, op1);
	buffer_armar(buffer, &op2, sizeof(&op2));
}

void seralizar_tres_parametro(t_buffer* buffer, tipo_instruccion_consola instruccion, char op1[50], char op2[50], char op3[50]){
	seralizar_dos_parametro(buffer,instruccion, op1, op2);
	buffer_armar(buffer, &op3, sizeof(&op3));
}

void buffer_armar(t_buffer* self, void* streamToAdd, int size) {
    self->stream = realloc(self->stream, self->size + size);
    memcpy(self->stream + self->size, streamToAdd, size);
    self->size += size;
}

