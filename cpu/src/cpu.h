#ifndef CPU_H_
#define CPU_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include "utils/utils.h"
#include "pthread.h"
#include "math.h"

void iterator(char* value);
t_config* iniciar_config(void);
cpu_config* cpu_config_crear(char*);
void fetch(cpu_atributos*);
void decode(cpuResponse*, instrucciones*);
void escribir_valor_en_registro_con_N_caracteres(char*,char*,int);
void escribir_valor_en_registro(char*,char*,registros_cpu* registros);
bool hayQueMandarContexto(instrucciones*);
bool instruccionNoDesaloja(instrucciones*);
bool direccion_entra_en_segmento(t_list* ,int,int);
tabla_segmento* encontrar_segmento(t_list* tabla_segmentos, int num_segmento);
char* encontrar_valor_registro(char* registro,registros_cpu* registros);
int encontrar_tamanio_registro(char*);
void desempaquetar_registros_cpu(t_buffer* buffer, registros_cpu* registros);
void enviar_instruccion_kernel (cpuResponse* cpu_enviar);

#endif /* CPU_H_ */
