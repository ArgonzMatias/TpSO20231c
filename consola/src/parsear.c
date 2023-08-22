#include "parsear.h"

#include <commons/string.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int long_maxima_instruccion = 8;

void parsear_pseudocodigo(t_buffer *buffer,const char* path, t_log* logger_consola)// el path es la ruta del archivo y se pasa por ahi xq ahi va a buscar el archivo
{
	log_info(logger_consola,"Estoy en send_instruccion_consola_kernel");
	FILE* archivo = fopen (path,"r");
	instrucciones* instruc = malloc(sizeof(instruccion));
	char* opchar1 = malloc(20*sizeof(char));
	char* opchar2 = malloc(20*sizeof(char));
	char* opchar3 = malloc(20*sizeof(char));

	if(archivo == NULL)
	{
		log_info(logger_consola,"no se pudo abrir el archivo de instrucciones");
	}
	char *instruccion = malloc(long_maxima_instruccion * sizeof(char));// cuando lee una linea te devuelve esa linea
	int contadorInstruccion = 0;
	//instrucciones listaInstrucciones[100]; // armo una lista de tipo instrucciones, que es el struct que cree, para poner las intrucciones en listas
	for(;;){
		contadorInstruccion += 1;
		log_info(logger_consola,"parseando instruccion numero: %d", contadorInstruccion);

	if(fscanf(archivo, "%s", instruccion)){
		if(strcmp(instruccion, "F_READ") == 0){
					if(fscanf(archivo, "%s %s %s", opchar1, opchar2, opchar3)){ //este como le llega ARCHIVO 16 4 por ejemplo, va %s: string, d%:decimal, d%, para que lo entienda
						instruc->instruccion_nombre = INSTRUC_F_READ;
						instruc->parametro1 = opchar1;
						instruc->parametro2 = opchar2;
						instruc->parametro3 = opchar3;

					}
				}

		else if (strcmp(instruccion, "F_WRITE") == 0){

					if(fscanf(archivo, "%s %s %s", opchar1, opchar2, opchar3)){
					instruc->instruccion_nombre = INSTRUC_F_WRITE;
					instruc->parametro1 = opchar1;
					instruc->parametro2 = opchar2;
					instruc->parametro3 = opchar3;

					}
				}

		else if (strcmp(instruccion, "SET") == 0){
					if(fscanf(archivo, "%s %s", opchar1, opchar2)){
						instruc->instruccion_nombre = INSTRUC_SET;
						instruc->parametro1 = opchar1;
						instruc->parametro2 = opchar2;
						instruc->parametro3 = NULL;

						log_info(logger_consola,"se empaqueta instruccion set");
					}
				}

		else if (strcmp(instruccion, "MOV_IN") == 0){
					if(fscanf(archivo, "%s %s", opchar1, opchar2)){
						instruc->instruccion_nombre = INSTRUC_MOV_IN;
						instruc->parametro1 = opchar1;
																		instruc->parametro2 = opchar2;
																		instruc->parametro3 = NULL;

					}
				}
		else if (strcmp(instruccion, "MOV_OUT") == 0){
					if(fscanf(archivo, "%s %s", opchar1, opchar2)){
						instruc->instruccion_nombre = INSTRUC_MOV_OUT;
						instruc->parametro1 = opchar1;
																		instruc->parametro2 = opchar2;
																		instruc->parametro3 = NULL;
						log_info(logger_consola,"se empaqueta instruccion mov out");

					}
				}
		else if (strcmp(instruccion, "F_TRUNCATE") == 0){
					if(fscanf(archivo, "%s %s", opchar1, opchar2)){
						instruc->instruccion_nombre = INSTRUC_F_TRUNCATE;
						instruc->parametro1 = opchar1;
																		instruc->parametro2 = opchar2;
																		instruc->parametro3 = NULL;

					}
				}
		else if (strcmp(instruccion, "F_SEEK") == 0){
					if(fscanf(archivo, "%s %s", opchar1, opchar2)){
						instruc->instruccion_nombre = INSTRUC_F_SEEK;
						instruc->parametro1 = opchar1;
						instruc->parametro2 = opchar2;

					}
				}
		else if (strcmp(instruccion, "CREATE_SEGMENT") == 0){
					if(fscanf(archivo, "%s %s", opchar1, opchar2)){
						instruc->instruccion_nombre = INSTRUC_CREATE_SEGMENT;
						instruc->parametro1 = opchar1;
												instruc->parametro2 = opchar2;
												instruc->parametro3 = NULL;
					}
				}
		else if (strcmp(instruccion, "I/O") == 0){
					if(fscanf(archivo, "%s", opchar1)){
						instruc->instruccion_nombre = INSTRUC_IO;
						instruc->parametro1 = opchar1;
												instruc->parametro2 = NULL;
												instruc->parametro3 = NULL;

					}
				}
		else if (strcmp(instruccion, "WAIT") == 0){
					if(fscanf(archivo, "%s", opchar1)){
						instruc->instruccion_nombre = INSTRUC_WAIT;
						instruc->parametro1 = opchar1;
												instruc->parametro2 = NULL;
												instruc->parametro3 = NULL;

					}
				}
		else if (strcmp(instruccion, "SIGNAL") == 0){
					if(fscanf(archivo, "%s", opchar1)){
						instruc->instruccion_nombre = INSTRUC_SIGNAL;
						instruc->parametro1 = opchar1;
												instruc->parametro2 = NULL;
												instruc->parametro3 = NULL;

					}
				}
		else if (strcmp(instruccion, "F_OPEN") == 0){
					if(fscanf(archivo, "%s", opchar1)){
						instruc->instruccion_nombre = INSTRUC_F_OPEN;
						instruc->parametro1 = opchar1;
												instruc->parametro2 = NULL;
												instruc->parametro3 = NULL;

					}
				}
		else if (strcmp(instruccion, "F_CLOSE") == 0){
					if(fscanf(archivo, "%s", opchar1)){
						instruc->instruccion_nombre = INSTRUC_F_CLOSE;
						instruc->parametro1 = opchar1;
												instruc->parametro2 = NULL;
												instruc->parametro3 = NULL;

						log_info(logger_consola,"se empaqueta instruccion fclose");
					}
				}
		else if (strcmp(instruccion, "DELETE_SEGMENT") == 0){
					if(fscanf(archivo, "%s", opchar1)){
						instruc->instruccion_nombre = INSTRUC_DELETE_SEGMENT;
						instruc->parametro1 = opchar1;
						instruc->parametro2 = NULL;
						instruc->parametro3 = NULL;

						log_info(logger_consola,"se empaqueta instruccion delete segment");
					}
				}
		else if (strcmp(instruccion, "EXIT") == 0){
					instruc->instruccion_nombre = INSTRUC_EXIT;
					log_info(logger_consola,"se empaqueta instruccion salida");
					instruc->parametro1 = NULL;
				instruc->parametro2 = NULL;
				instruc->parametro3 = NULL;
					empaquetar_instruccion(instruc, buffer);
					//buffer_pack(buffer,instruc, sizeof(*instruc));
					break;
					}
		else if (strcmp(instruccion, "YIELD") == 0){
					instruc->instruccion_nombre = INSTRUC_YIELD;
					log_info(logger_consola,"se empaqueta instruccion yield");
					instruc->parametro1 = NULL;
									instruc->parametro2 = NULL;
									instruc->parametro3 = NULL;
				}
			}
	empaquetar_instruccion(instruc, buffer);
	}

	log_info(logger_consola,"Se parseo todas las instrucciones, cerrando el archivo");
	free(instruc);
	fclose(archivo);//llega hasta aca 3.6.2023
	// hay que liberar el espacio de la linea
	free(opchar1);
	free(opchar2);
	free(opchar3);
}



