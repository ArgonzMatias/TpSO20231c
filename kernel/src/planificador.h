#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_
#include "kernel.h"
#include "estado.h"
#include "utils/utils.h"
#include "utils/flags.h"




int64_t get_waiting_time_proceso(procesoNuevo* pcb); //??????? no entiendo, odio eclipse.
procesoNuevo* pcb_por_fifo(estado_proceso* e);
float get_estimado_rafaga_proceso(procesoNuevo* pcb, int alpha);
long int get_hrrn_proceso(procesoNuevo* pcb);
procesoNuevo* next_hrrn_proceso(estado_proceso* e);
void* nuevo_pcb_new(void* socket);
procesoNuevo* nuevoProceso(t_buffer* instrucciones);
void inicializar_planificador(void);
void enviar_instruccion_cpu (procesoNuevo* pcb, uint8_t header);
void instruccion_en_IO(procesoNuevo* pcb, int tiempo);
void sleepHilo(void* tiempo_ptr);
instruccion get_flag_cpuResponse(cpuResponse cpu);
int get_tiempo_cpuResponse(cpuResponse cpu);
void destroy_pcb(procesoNuevo* pcb);
int buscar_posicion_recurso(char* recurso);
int hay_elementos_wait(int posicion);
void recurso_inexistente(char*recursoPcb,procesoNuevo* pcb);
tabla_segmento* recive_segmento_nuevo();
void* liberar_segmentos_pcb(procesoNuevo* aux);
void* actualizarPorCompactacion(int sizeTabla);
void* actualizarSegmentosPorEstado(estado_proceso* e, tabla_segmento* segmentoAux);
bool mismoProceso(procesoNuevo* pcb1, procesoNuevo* pcb2);
bool mismoSegmento(segmento* seg1, segmento* seg2);
void* stream_archivo_fs_consulta(char* archivo);
void* enviar_archivo_fs(rtaKernelFs* dato, instruccion );
void * f_read_write(procesoNuevo* pcb, rtaKernelFs* , instruccion );
void creacionArchivoFileSystem(procesoNuevo* pcb, rtaKernelFs* archivo);
void logBlocked(int pid, char* motivoBloqueo);
void logReady();
void streamPcbFS(int pid);
//void pack_registros(t_buffer* bufferEjecutar,cpuResponse* cpu_enviar);

#endif
