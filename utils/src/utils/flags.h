#ifndef FLAGS_H
#define FLAGS_H

typedef enum
{
	hs_consola,
	hs_aux,
	hs_cpu,
	hs_memoria,
	hs_filesystem,
	hs_fs_blocked,
	hs_kernel,
}handsakes;


typedef enum
{
	header_lista_instrucciones,
	header_pcb_execute,
	header_pcb_pid,
	header_resultado,
	header_respuesta_cpu,
	header_general,
	header_compactacion,
	header_out_of_memory,
	header_nuevo_archivo_filesystem,
	header_error,
	header_archivo,
}headers;

typedef enum{
	I_EXIT,
	I_YIELD,
	I_IO,
	I_WAIT,
	I_SIGNAL,
	I_READ,
	I_WRITE,
	I_DELETE_SEGMENT,
	I_CREATE_SEGMENT,
	I_OPEN,
	I_CLOSE,
	I_SEEK,
	I_TRUNCATE,
	I_CREATE_ARCHIVO,
	SEG_FAULT,
	CREAR_PROCESO,
}instruccion;

typedef enum{
	INSTRUC_F_READ,
	INSTRUC_F_WRITE,
	INSTRUC_SET,
	INSTRUC_MOV_IN,
	INSTRUC_MOV_OUT,
	INSTRUC_F_TRUNCATE,
	INSTRUC_F_SEEK,
	INSTRUC_CREATE_SEGMENT,
	INSTRUC_IO,
	INSTRUC_WAIT,
	INSTRUC_SIGNAL,
	INSTRUC_F_OPEN,
	INSTRUC_F_CLOSE,
	INSTRUC_DELETE_SEGMENT,
	INSTRUC_EXIT,
	INSTRUC_YIELD,
} tipo_instruccion_consola;

#endif
