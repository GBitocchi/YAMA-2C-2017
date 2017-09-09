# ElTPEstaBien

### [Protocolo](https://docs.google.com/document/d/1HTPwwbBRAI6GmL6H6vFxna70PN-Eu-OCSxXIjhXF5k4/edit?usp=sharing)

### Integrantes:
- 155.900-0 / Christian Dario Nievas
- 156.607-6 / Lautaro Gomez Odriozola
- 155.467-0 / Lucas Ezequiel Romano
- 155.468-2 / Gustavo Ayrton Bitocchi
- 155.553-4 / German Caceres


### Primer checkpoint (16/09/2017):

- [ ] Consola de FS sin funcionalidades.
- [ ] Propagar archivo abierto por Master entre los procesos.
- [X] Desarrollo servidor con hilos en Master.
- [X] Desarrollo servidor con forks en Worker.


### Segundo checkpoint (07/10/2017):

- [X] Aplicar nmap().
- [ ] Estructuras FS (tabla de directorios, archivos, nodos, bitmap).
- [ ] YAMA planifica tareas con Round Robin.
- [ ] Master y YAMA efectuan todo un recorrido respetando el RR (sin comunicacion contra el Worker ni FS).
- [ ] FS debe permitir la copia de un archivo al yamafs y su almacentamiento partiendolo en bloques.
