# 🐧 Sistemas Operativos

![C++](https://img.shields.io/badge/C++17-00599C?logo=cplusplus&logoColor=white)
![Bash](https://img.shields.io/badge/Bash-4EAA25?logo=gnubash&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black)

Programación de sistemas en Linux: scripting avanzado en Bash y uso directo de la API POSIX (sockets, señales, descriptores de fichero) desde C++.

## Proyectos

### `scdebug`: depurador de llamadas al sistema (Bash)
Script que envuelve a **`strace`** para registrar las llamadas al sistema de un programa y gestionar las trazas:
- Lanza un programa bajo `strace` o se engancha a procesos ya en marcha, por nombre (`-nattch`) o por PID (`-pattch`)
- Guarda las trazas en ficheros organizados por usuario y proceso
- Consulta las trazas (`-v`, `-vall`) y termina los procesos rastreados (`-k`)

```bash
./scdebug_final.sh -sto "-c -f" ls -l
```

Versiones: [`scdebug_strace_version.sh`](scdebug_strace_version.sh) → [`scdebug_final.sh`](scdebug_final.sh), más la [modificación](scdebug_modificacion.sh) de la evaluación.

### `netcp`: copia de ficheros por red (C++)
Envía y recibe ficheros con **sockets UDP**. Desarrollado de forma incremental:

| Versión | Novedades |
|---------|-----------|
| [v1](netcp_first_version.cc) | Lectura del fichero y envío por socket UDP (`sendto`), con errores gestionados mediante `std::error_code` |
| [v2](netcp_second_version.cc) | Modo receptor (`-l`, `recvfrom`) y configuración por variables de entorno (`NETCP_IP`, `NETCP_PORT`) |
| [v3](netcp_third_version.cc) | Cierre limpio con `sigaction` (`SIGINT`, `SIGTERM`) y una bandera `std::atomic` |

```bash
g++ -std=c++17 netcp_third_version.cc -o netcp
./netcp -l salida.txt        # receptor
./netcp fichero.txt          # emisor
```

---

Grado en Ingeniería Informática · Universidad de La Laguna · Antonio Martínez Alemán
