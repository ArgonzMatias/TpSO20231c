#ifndef CONSOLE_H_
#define CONSOLE_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include "serializador.h"
#include "utils/utils.h"
#include <errno.h>
#include "utils/flags.h"
#include "utils/buffer.h"
#include "parsear.h"


typedef struct consola_config consola_config;

t_log* iniciar_logger(void);
consola_config* iniciar_config(t_log* logger);
void leer_consola(t_log*);
void paquete(int);
void terminar_programa(int, t_log*, t_config*);
char* get_ip_kernel_consola(consola_config* aux);
char* get_puerto_kernel_consola(consola_config* aux);
void terminar_programa(int conexion, t_log* logger, t_config* config);
void send_instruccion_consola_kernel(int socket);

#endif /* CONSOLE_H_ */

