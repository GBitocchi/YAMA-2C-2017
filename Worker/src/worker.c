#include "../../Biblioteca/src/Socket.c"
#include "../../Biblioteca/src/configParser.c"

#define PARAMETROS {"IP_FILESYSTEM","PUERTO_FILESYSTEM","NOMBRE_NODO","PUERTO_WORKER","RUTA_DATABIN"}
#define TRANSFORMACION 1
#define TRANSFORMACION_TERMINADA 2
#define ERROR_TRANSFORMACION 18
#define REDUCCION_LOCAL 8
#define REDUCCION_LOCAL_TERMINADA 4
#define ERROR_REDUCCION_LOCAL 6
#define REDUCCION_GLOBAL 9
#define REDUCCION_GLOBAL_TERMINADA 5
#define ERROR_REDUCCION_GLOBAL 7
#define ALMACENADO_FINAL 15
#define ALMACENADO_FINAL_TERMINADO 16
#define ERROR_ALMACENADO_FINAL 17
#define APAREO_GLOBAL 18

t_log* loggerWorker;
char* IP_FILESYSTEM;
char* RUTA_DATABIN;
char* NOMBRE_NODO;
int PUERTO_FILESYSTEM;
int PUERTO_WORKER;

typedef struct{
	uint32_t socketParaRecibir;
	char* bloqueLeido;
}infoApareoArchivo;

void sigchld_handler(int s){
	while(wait(NULL) > 0);
}

void eliminarProcesosMuertos(){
	struct sigaction sa;
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		log_error(loggerWorker,"Error de sigaction al eliminar procesos muertos.\n");
		exit(-1);
	}
}

void cargarWorker(t_config* configuracionWorker){
    if(!config_has_property(configuracionWorker, "IP_FILESYSTEM")){
        log_error(loggerWorker,"No se encuentra cargado IP_FILESYSTEM en el archivo.\n");
        exit(-1);
    }else{
        IP_FILESYSTEM = config_get_string_value(configuracionWorker, "IP_FILESYSTEM");
    }
    if(!config_has_property(configuracionWorker, "PUERTO_FILESYSTEM")){
    	log_error(loggerWorker,"No se encuentra cargado PUERTO_FILESYSTEM en el archivo.\n");
        exit(-1);
    }else{
        PUERTO_FILESYSTEM = config_get_int_value(configuracionWorker, "PUERTO_FILESYSTEM");
    }
    if(!config_has_property(configuracionWorker, "NOMBRE_NODO")){
    	log_error(loggerWorker,"No se encuentra cargado NOMBRE_NODO en el archivo.\n");
        exit(-1);
    }else{
        NOMBRE_NODO = string_new();
        string_append(&NOMBRE_NODO, config_get_string_value(configuracionWorker, "NOMBRE_NODO"));
    }
    if(!config_has_property(configuracionWorker, "PUERTO_WORKER")){
    	log_error(loggerWorker,"No se encuentra cargado PUERTO_WORKER en el archivo.\n");
        exit(-1);
    }else{
        PUERTO_WORKER = config_get_int_value(configuracionWorker, "PUERTO_WORKER");
    }
    if(!config_has_property(configuracionWorker, "RUTA_DATABIN")){
    	log_error(loggerWorker,"No se encuentra cargado RUTA_DATABIN en el archivo.\n");
        exit(-1);
    }else{
        RUTA_DATABIN = string_new();
        string_append(&RUTA_DATABIN, config_get_string_value(configuracionWorker, "RUTA_DATABIN"));
    }
    config_destroy(configuracionWorker);
}

void darPermisosAScripts(char* script){
	struct stat infoScript;

	if(chmod(script,S_IXUSR|S_IRUSR|S_IXGRP|S_IRGRP|S_IXOTH|S_IROTH|S_ISVTX)!=0){
		log_error(loggerWorker,"Error al otorgar permisos al script.\n");
	}
	else if(stat(script,&infoScript)!=0){
		log_error(loggerWorker,"No se pudo obtener informacion del script.\n");
	}
	else{
		log_info(loggerWorker,"Los permisos para el script son: %08x\n",infoScript.st_mode);
	}
}

char* crearComandoScriptTransformador(char* nombreScript,char* pathDestino, uint32_t nroBloque, uint32_t bytesOcupados){
	char* command = string_new();
	string_append(&command, "head -n ");
	string_append(&command,string_itoa((nroBloque*1048576)+bytesOcupados));
	string_append(&command," ");
	string_append(&command,RUTA_DATABIN);
	string_append(&command," | tail -n ");
	string_append(&command,string_itoa(bytesOcupados));
	string_append(&command," | sh ");
	string_append(&command,nombreScript);
	string_append(&command," | sort > ");
	string_append(&command,pathDestino);
	free(pathDestino);
	log_info(loggerWorker, "Se creo correctamente el comando del script transformador\n");
	return command;
}

char* crearComandoScriptReductor(char* archivoApareado,char* nombreScript,char* pathDestino){
	char* command = string_new();
	string_append(&command,archivoApareado);
	string_append(&command," | sh ");
	string_append(&command,nombreScript);
	string_append(&command," > ");
	string_append(&command,pathDestino);
	free(pathDestino);
	free(archivoApareado);
	log_info(loggerWorker, "Se creo correctamente el comando del script reductor\n");
	return command;
}

void crearArchivoTemporal(char* nombreScript){
	string_append(&nombreScript,"XXXXXX");
	int resultado = mkstemp(nombreScript);

	if(resultado==-1){
		log_error(loggerWorker,"No se pudo crear un archivo temporal para guardar el script.\n");
		exit(-1);
	}
	log_info(loggerWorker, "Se creo correctamente un archivo temporal con nombre: %s.\n",nombreScript);
}

void guardarScript(char* script,char* nombreScript){
	crearArchivoTemporal(nombreScript);

	FILE* archivoScript = fopen(nombreScript,"w");

	if(archivoScript==NULL){
		log_error(loggerWorker,"No se pudo abrir el archivo donde se guardara el script.\n");
		exit(-1);
	}
	log_info(loggerWorker, "Se pudo abrir el archivo donde se guardara el script: %s.\n",nombreScript);

	if(fputs(script,archivoScript)==EOF){
		log_error(loggerWorker,"No se pudo escribir en el archivo del script.\n");
		exit(-1);
	}

	log_info(loggerWorker, "Se pudo escribir en el archivo donde se guarda el script: %s.\n",nombreScript);

	if(fclose(archivoScript)==EOF){
		log_error(loggerWorker,"No se pudo cerrar el archivo del script.\n");
	}

	log_info(loggerWorker, "Se pudo cerrar el archivo donde se guarda el script: %s.\n",nombreScript);

	free(script);
}

void eliminarArchivo(char* nombreScript){
	if(remove(nombreScript)!=0){
		log_error(loggerWorker,"No se pudo eliminar el script.\n");
	}
	else{
		log_info(loggerWorker,"El script se elimino correctamente.\n");
	}
	free(nombreScript);
}

char* realizarApareoGlobal(t_list* listaInfoApareo){
	int posicion;
	char* archivoApareado = string_new();
	string_append(&archivoApareado,"archivoApareoGlobal");
	crearArchivoTemporal(archivoApareado);

	FILE* archivoGlobalApareado = fopen(archivoApareado,"w");

	if(archivoGlobalApareado==NULL){
		log_error(loggerWorker,"No se pudo abrir el archivo global apareado.\n");
		exit(-1);
	}

	log_info(loggerWorker, "Se creo el archivo donde se guarda lo apareado globalmente.\n");

	while(!list_is_empty(listaInfoApareo)){
		int cantidad = list_size(listaInfoApareo);

		for(posicion=0;posicion<cantidad;posicion++){
			infoApareoArchivo* unaInfoArchivo = list_remove(listaInfoApareo, 0);
			if((unaInfoArchivo->bloqueLeido)==NULL){
				char* unPedacitoArchivo = recibirString(unaInfoArchivo->socketParaRecibir);

				if(strcmp(unPedacitoArchivo,"\0")!=0){
					string_append(&(unaInfoArchivo->bloqueLeido),unPedacitoArchivo);
					list_add(listaInfoApareo,unaInfoArchivo);
				}
				else{
					free(unPedacitoArchivo);
					free(unaInfoArchivo->bloqueLeido);
					close(unaInfoArchivo->socketParaRecibir);
					free(unaInfoArchivo);
				}
			}
			else{
				list_add(listaInfoApareo,unaInfoArchivo);
			}
		}

		log_info(loggerWorker, "Se recibio un set de stream de los workers.\n");

		cantidad = list_size(listaInfoApareo);
		char* menorString = string_new();
		menorString = NULL;

		for(posicion=0;posicion<cantidad;posicion++){
			infoApareoArchivo* unaInfoArchivo = list_remove(listaInfoApareo, 0);
			if(menorString!=NULL){
				if(strcmp(unaInfoArchivo->bloqueLeido,menorString)<=0){
					log_info(loggerWorker, "El string %s es menor alfabeticamente que %s.\n",unaInfoArchivo->bloqueLeido,menorString);
					free(menorString);
					char* menorString = string_new();
					menorString = NULL;
					string_append(&menorString,unaInfoArchivo->bloqueLeido);
					free(unaInfoArchivo->bloqueLeido);
					unaInfoArchivo->bloqueLeido = string_new();
					unaInfoArchivo->bloqueLeido = NULL;
					list_add(listaInfoApareo,unaInfoArchivo);
				}
				else{
					log_info(loggerWorker, "El string %s es menor alfabeticamente que %s.\n",menorString,unaInfoArchivo->bloqueLeido);
					list_add(listaInfoApareo,unaInfoArchivo);
				}
			}
			else{
				string_append(&menorString,unaInfoArchivo->bloqueLeido);
				free(unaInfoArchivo->bloqueLeido);
				unaInfoArchivo->bloqueLeido = string_new();
				unaInfoArchivo->bloqueLeido = NULL;
				list_add(listaInfoApareo,unaInfoArchivo);
			}
		}

		log_info(loggerWorker, "El string menor alfabeticamente es %s.\n",menorString);

		if(fputs(menorString,archivoGlobalApareado)==EOF){
			log_error(loggerWorker,"No se pudo escribir en el archivo global apareado.\n");
			exit(-1);
		}

		log_info(loggerWorker, "El string: %s se escribio correctamente en el archivo global apareado.\n",menorString);

		free(menorString);
	}


	if(fclose(archivoGlobalApareado)==EOF){
		log_error(loggerWorker,"No se pudo cerrar el archivo global apareado.\n");
	}

	log_info(loggerWorker, "Se cerro correctamente el archivo global apareado.\n");

	list_destroy(listaInfoApareo);

	return archivoApareado;
}

void ejecutarPrograma(char* command,int socketMaster,uint32_t casoError,uint32_t casoExito){
	uint32_t resultado = system(command);

	if(!WIFEXITED(resultado)){
		log_error(loggerWorker, "Error al ejecutar el script con system.\n");

		if(WIFSIGNALED(resultado)){
			log_error(loggerWorker, "La llamada al sistema termino con la senial %d\n",WTERMSIG(resultado));
		}

		sendDeNotificacion(socketMaster,casoError);
	}
	else{
		log_info(loggerWorker, "Script ejecutado correctamente con el valor de retorno: %d\n",WEXITSTATUS(resultado));
		sendDeNotificacion(socketMaster,casoExito);
	}

	free(command);
}

void realizarHandshakeWorker(char* unArchivoTemporal, uint32_t unSocketWorker){

	uint32_t tamanioArchivoTemporal = string_length(unArchivoTemporal);
	void* datosAEnviar = malloc(tamanioArchivoTemporal+sizeof(uint32_t));

	sendDeNotificacion(unSocketWorker, ES_WORKER);

	if(recvDeNotificacion(unSocketWorker) != ES_OTRO_WORKER){
		log_error(loggerWorker, "La conexion efectuada no es con otro worker.\n");
		close(unSocketWorker);
		exit(-1);
	}

	log_info(loggerWorker, "Se conecto con otro worker.\n");

	memcpy(datosAEnviar,&tamanioArchivoTemporal,sizeof(uint32_t));
	memcpy(datosAEnviar+sizeof(uint32_t),unArchivoTemporal,tamanioArchivoTemporal);

	log_info(loggerWorker, "Datos serializados para ser enviados al otro worker.\n");

	sendRemasterizado(unSocketWorker,APAREO_GLOBAL,tamanioArchivoTemporal+sizeof(uint32_t),datosAEnviar);

	free(unArchivoTemporal);
	free(datosAEnviar);
}

void realizarHandshakeFS(uint32_t socketFS){
	sendDeNotificacion(socketFS, ES_WORKER);

	if(recibirUInt(socketFS) != ES_FS){
		log_error(loggerWorker, "La conexion efectuada no es con FileSystem.\n");
		close(socketFS);
		exit(-1);
	}
	log_info(loggerWorker, "Se conecto con el FileSystem.\n");
}

long int obtenerTamanioArchivo(FILE* unArchivo){
	int retornoSeek = fseek(unArchivo, 0, SEEK_END);

	if(retornoSeek==0){
		log_error(loggerWorker,"Error de fseek.\n");
		exit(-1);
	}

	long int tamanioArchivo = ftell(unArchivo);

	if(tamanioArchivo==-1){
		log_error(loggerWorker,"Error de ftell.\n");
		exit(-1);
	}

	return tamanioArchivo;
}

char* leerArchivo(FILE* unArchivo, long int tamanioArchivo)
{
	int retornoSeek = fseek(unArchivo, 0, SEEK_SET);

	if(retornoSeek==0){
		log_error(loggerWorker,"Error de fseek.\n");
		exit(-1);
	}

	char* contenidoArchivo = malloc(tamanioArchivo+1);
	fread(contenidoArchivo, tamanioArchivo, 1, unArchivo);
	contenidoArchivo[tamanioArchivo] = '\0';
	return contenidoArchivo;
}

char* obtenerContenido(char* unPath){
	FILE* archivoALeer = fopen(unPath, "r");

	if(archivoALeer==NULL){
		log_error(loggerWorker,"No se pudo abrir el archivo: %s.\n",unPath);
		exit(-1);
	}

	log_info(loggerWorker,"Se pudo abrir el archivo: %s.\n",unPath);

	long int tamanioArchivo = obtenerTamanioArchivo(archivoALeer);
	log_info(loggerWorker, "Se pudo obtener el tamanio del archivo: %s.\n",unPath);
	char* contenidoArchivo = leerArchivo(archivoALeer,tamanioArchivo);
	log_info(loggerWorker, "Se pudo obtener el contenido del archivo: %s.\n",unPath);
	return contenidoArchivo;
}

void enviarDatosAFS(uint32_t socketFS,char* nombreArchivoReduccionGlobal,char* nombreResultante,char* rutaResultante){

	char* contenidoArchivoReduccionGlobal = obtenerContenido(nombreArchivoReduccionGlobal);

	uint32_t tamanioContenidoArchivoReduccionGlobal= string_length(contenidoArchivoReduccionGlobal);
	uint32_t tamanionombreResultante= string_length(nombreResultante);
	uint32_t tamaniorutaResultante= string_length(rutaResultante);
	uint32_t tamanioTotalAEnviar = tamanioContenidoArchivoReduccionGlobal+tamaniorutaResultante+tamanionombreResultante+(sizeof(uint32_t)*3);

	void* datosAEnviar = malloc(tamanioTotalAEnviar);

	memcpy(datosAEnviar,&tamanioContenidoArchivoReduccionGlobal,sizeof(uint32_t));
	memcpy(datosAEnviar+sizeof(uint32_t),contenidoArchivoReduccionGlobal,tamanioContenidoArchivoReduccionGlobal);
	memcpy(datosAEnviar+tamanioContenidoArchivoReduccionGlobal+sizeof(uint32_t),&tamanionombreResultante,sizeof(uint32_t));
	memcpy(datosAEnviar+tamanioContenidoArchivoReduccionGlobal+sizeof(uint32_t)*2,nombreResultante,tamanionombreResultante);
	memcpy(datosAEnviar+tamanioContenidoArchivoReduccionGlobal+tamanionombreResultante+sizeof(uint32_t)*2,&tamaniorutaResultante,sizeof(uint32_t));
	memcpy(datosAEnviar+tamanioContenidoArchivoReduccionGlobal+tamanionombreResultante+sizeof(uint32_t)*3,rutaResultante,tamaniorutaResultante);

	log_info(loggerWorker, "Datos serializados correctamente para ser enviados al FileSystem\n");
	sendRemasterizado(socketFS,ALMACENADO_FINAL,tamanioTotalAEnviar,datosAEnviar);
}

uint32_t asignarStreamADatosParaEnviar(uint32_t tamanioPrevio, char* streamAEnviar, void* datosAEnviar){
	uint32_t tamanioStream = string_length(streamAEnviar);
	datosAEnviar = realloc(datosAEnviar,tamanioStream+sizeof(uint32_t)+tamanioPrevio);
	tamanioPrevio = tamanioStream+sizeof(uint32_t)+tamanioPrevio;

	memcpy(datosAEnviar,&tamanioStream,sizeof(uint32_t));
	memcpy(datosAEnviar+sizeof(uint32_t),streamAEnviar,tamanioStream);

	free(streamAEnviar);

	return tamanioPrevio;
}

char* leerLinea(FILE* unArchivo){
	char* lineaLeida = string_new();

	while(!feof(unArchivo))
	{
		int cadenaLeida = fgetc(unArchivo);

		if (cadenaLeida != '\n')
		{
			string_append(&lineaLeida,&cadenaLeida);
		}
		else{
			break;
		}
	}

	log_info(loggerWorker, "Se obtuvo la siguiente linea del archivo.\n");

	return lineaLeida;
}

void enviarDatosAWorkerDesignado(int socketAceptado,char* nombreArchivoTemporal){
	FILE* archivoTemporal = fopen(nombreArchivoTemporal,"r");
	void* datosAEnviar;
	char* streamAEnviar;
	uint32_t tamanioPrevio = 0;

	while(!feof(archivoTemporal)){
		streamAEnviar = leerLinea(archivoTemporal);
		tamanioPrevio = asignarStreamADatosParaEnviar(tamanioPrevio,streamAEnviar,datosAEnviar);
	}

	streamAEnviar = string_new();
	string_append(&streamAEnviar,"\0");
	tamanioPrevio = asignarStreamADatosParaEnviar(tamanioPrevio,streamAEnviar,datosAEnviar);
	log_info(loggerWorker, "Todos los datos del archivo temporal reducido del worker fueron serializados\n");
	sendRemasterizado(socketAceptado,APAREO_GLOBAL,tamanioPrevio,datosAEnviar);
	free(datosAEnviar);

	if(fclose(archivoTemporal)==EOF){
		log_error(loggerWorker,"No se pudo cerrar el archivo global apareado.\n");
	}
}

char* aparearArchivos(t_list* archivosTemporales){
	char* nombreArchivoApareado = string_new();
	string_append(&nombreArchivoApareado,"archivoApareadoTemporal");
	char* comandoOrdenacionArchivos = string_new();
	string_append(&comandoOrdenacionArchivos,"sort -m ");
	int posicion;
	int cantidad = list_size(archivosTemporales);

	for(posicion=0;posicion<cantidad;posicion++){
		char* unArchivoTemporal = list_remove(archivosTemporales,0);
		string_append(&comandoOrdenacionArchivos,unArchivoTemporal);
		string_append(&comandoOrdenacionArchivos," ");
		free(unArchivoTemporal);
	}

	string_append(&comandoOrdenacionArchivos,"> ");
	string_append(&comandoOrdenacionArchivos,nombreArchivoApareado);

	log_info(loggerWorker,"Comando para realizar apareo local de archivos fue correctamente creado.\n");

	uint32_t resultado = system(comandoOrdenacionArchivos);

	if(!WIFEXITED(resultado)){
		log_error(loggerWorker, "Error al ejecutar el comando para aparear archivos con system.\n");

		if(WIFSIGNALED(resultado)){
			log_error(loggerWorker, "La llamada al sistema termino con la senial %d\n",WTERMSIG(resultado));
		}
	}
	else{
		log_info(loggerWorker, "System para aparear archivos ejecutado correctamente con el valor de retorno: %d\n",WEXITSTATUS(resultado));
	}

	free(comandoOrdenacionArchivos);

	list_destroy(archivosTemporales);

	return nombreArchivoApareado;
}

void crearProcesoHijo(int socketMaster){
	log_info(loggerWorker, "Se recibio un job del socket de master %d.\n",socketMaster);
	int pipe_padreAHijo[2];
	int pipe_hijoAPadre[2];

	pipe(pipe_padreAHijo);
	pipe(pipe_hijoAPadre);
	log_info(loggerWorker, "Pipes creados\n");

	pid_t pid = fork();

	switch(pid){
	case -1:{
		log_error(loggerWorker, "No se pudo crear el proceso hijo\n");
		close(socketMaster);
		exit(-1);
	}
	break;
	case 0:{
		log_info(loggerWorker,"Soy el hijo con el pid %d y mi padre tiene el pid: %d \n",getpid(),getppid());

		dup2(pipe_padreAHijo[0],STDIN_FILENO);
		dup2(pipe_hijoAPadre[1],STDOUT_FILENO);

		close(pipe_padreAHijo[1]);
		close(pipe_hijoAPadre[0]);
		close(pipe_padreAHijo[0]);
		close(pipe_hijoAPadre[1]);

		uint32_t tipoEtapa = recibirUInt(socketMaster);

		switch(tipoEtapa){
		case TRANSFORMACION:{
			char* script = recibirString(socketMaster);
			char* nombreScript = recibirString(socketMaster);
			uint32_t nroBloque = recibirUInt(socketMaster);
			uint32_t bytesOcupados = recibirUInt(socketMaster);
			char* pathDestino = recibirString(socketMaster);

			log_info(loggerWorker, "Todos los datos fueron recibidos de master para realizar la transformacion");

			guardarScript(script,nombreScript);

			darPermisosAScripts(nombreScript);

			char* command = crearComandoScriptTransformador(nombreScript,pathDestino,nroBloque,bytesOcupados);

			ejecutarPrograma(command,socketMaster,ERROR_TRANSFORMACION,TRANSFORMACION_TERMINADA);

			eliminarArchivo(nombreScript);

		}
		break;
		case REDUCCION_LOCAL:{
			char* script = recibirString(socketMaster);
			char* pathDestino = recibirString(socketMaster);
			char* nombreScript = recibirString(socketMaster);
			uint32_t cantidadTemporales = recibirUInt(socketMaster);
			uint32_t posicion;
			t_list* archivosTemporales = list_create();
			for(posicion = 0; posicion < cantidadTemporales; posicion++){
				char* unArchivoTemporal = recibirString(socketMaster);
				list_add(archivosTemporales,unArchivoTemporal);
			}

			log_info(loggerWorker, "Todos los datos fueron recibidos de master para realizar la reduccion local");

			char* archivoApareado = aparearArchivos(archivosTemporales);

			guardarScript(script,nombreScript);

			darPermisosAScripts(nombreScript);

			char* command = crearComandoScriptReductor(archivoApareado,nombreScript,pathDestino);

			ejecutarPrograma(command,socketMaster,ERROR_REDUCCION_LOCAL,REDUCCION_LOCAL_TERMINADA);

			eliminarArchivo(nombreScript);
			eliminarArchivo(archivoApareado);

		}
		break;
		case REDUCCION_GLOBAL:{
			char* script = recibirString(socketMaster);
			char* pathDestino = recibirString(socketMaster);
			char* nombreScript = recibirString(socketMaster);
			uint32_t cantidadWorkers = recibirUInt(socketMaster);
			uint32_t posicionWorker;
			t_list* listaInfoApareo = list_create();
			for(posicionWorker = 0; posicionWorker < cantidadWorkers; posicionWorker++){
				char* archivoTemporal = recibirString(socketMaster);
				char* ipWorker = recibirString(socketMaster);
				uint32_t puertoWorker = recibirUInt(socketMaster);
				uint32_t unSocketWorker = conectarAServer(ipWorker, puertoWorker);
				realizarHandshakeWorker(archivoTemporal,unSocketWorker);
				infoApareoArchivo* unaInfoArchivo = malloc(sizeof(infoApareoArchivo));
				unaInfoArchivo->socketParaRecibir = unSocketWorker;
				unaInfoArchivo->bloqueLeido = string_new();
				unaInfoArchivo->bloqueLeido = NULL;
				list_add(listaInfoApareo,unaInfoArchivo);
			}

			log_info(loggerWorker, "Todos los datos fueron recibidos de master para realizar la reduccion global");

			char* archivoApareado = realizarApareoGlobal(listaInfoApareo);

			guardarScript(script,nombreScript);

			darPermisosAScripts(nombreScript);

			char* command = crearComandoScriptReductor(archivoApareado,nombreScript,pathDestino);

			ejecutarPrograma(command,socketMaster,ERROR_REDUCCION_GLOBAL,REDUCCION_GLOBAL_TERMINADA);

			eliminarArchivo(nombreScript);
			eliminarArchivo(archivoApareado);
		}
		break;
		case ALMACENADO_FINAL:{
			char* nombreArchivoReduccionGlobal = recibirString(socketMaster);
			char* nombreResultante = recibirString(socketMaster);
			char* rutaResultante = recibirString(socketMaster);

			log_info(loggerWorker, "Todos los datos fueron recibidos de master para realizar el almacenado final");

			uint32_t socketFS = conectarAServer(IP_FILESYSTEM, PUERTO_FILESYSTEM);
			realizarHandshakeFS(socketFS);

			enviarDatosAFS(socketFS,nombreArchivoReduccionGlobal,nombreResultante,rutaResultante);

			int notificacion = recvDeNotificacion(socketFS);

			if(notificacion==ALMACENADO_FINAL_TERMINADO){
				sendDeNotificacion(socketMaster,ALMACENADO_FINAL_TERMINADO);
				close(socketFS);
			} else if(notificacion==ERROR_ALMACENADO_FINAL){
				sendDeNotificacion(socketMaster,ERROR_ALMACENADO_FINAL);
				close(socketFS);
			} else{
				log_error(loggerWorker, "La conexion recibida es erronea.\n");
				sendDeNotificacion(socketMaster,ERROR_ALMACENADO_FINAL);
				close(socketFS);
			}
		}
		break;
		default:
			log_error(loggerWorker, "Error al recibir paquete de Master\n");
			exit(-1);
		}

		close(socketMaster);

		exit(1);
	}
	break;
	default:
		close(socketMaster);

		log_info(loggerWorker,"Soy el proceso padre con pid: %d y mi hijo tiene el pid %d \n ",getpid(),pid);

		close(pipe_padreAHijo[0]);
		close(pipe_hijoAPadre[1]);
		close(pipe_padreAHijo[1]);
		close(pipe_hijoAPadre[0]);
	}
}

int main(int argc, char **argv) {
	loggerWorker = log_create("Worker.log", "Worker", 1, 0);
	chequearParametros(argc,2);
	t_config* configuracionWorker = generarTConfig(argv[1], 5);
	//t_config* configuracionWorker = generarTConfig("Debug/worker.ini", 5);
	cargarWorker(configuracionWorker);
	log_info(loggerWorker, "Se cargo correctamente Worker.\n");
	int socketAceptado, socketEscuchaWorker;
	socketEscuchaWorker = ponerseAEscucharClientes(PUERTO_WORKER, 0);
	eliminarProcesosMuertos();
	log_info(loggerWorker, "Procesos muertos eliminados del sistema.\n");
	while(1){
		socketAceptado = aceptarConexionDeCliente(socketEscuchaWorker);
		log_info(loggerWorker, "Se ha recibido una nueva conexion.\n");
		int notificacion = recvDeNotificacion(socketAceptado);
		switch(notificacion){
		case ES_MASTER:{
			log_info(loggerWorker, "Se recibio una conexion de master.\n");
			sendDeNotificacion(socketAceptado, ES_WORKER);
			crearProcesoHijo(socketAceptado);
		}
		break;
		case ES_WORKER:{
			log_info(loggerWorker, "Se recibio una conexion de otro worker.\n");
			sendDeNotificacion(socketAceptado, ES_OTRO_WORKER);
			char* nombreArchivoTemporal = recibirString(socketAceptado);
			enviarDatosAWorkerDesignado(socketAceptado,nombreArchivoTemporal);
			close(socketAceptado);
		}
		break;
		default:
			log_error(loggerWorker, "La conexion recibida es erronea.\n");
			close(socketAceptado);
		}
	}
	close(socketEscuchaWorker);
	return EXIT_SUCCESS;
}
