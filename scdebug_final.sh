#!/bin/bash

#  ./scdebug_final.sh -sto "-c -f -F -tt" ls -l


# VARIABLES
OPTIONS=""
PROGRAM=""
FILE=""
TIME=""
NUMBER_FILES=""
NUMBER_TACES=""
AUX=""
SHOW_INFO=false
SHOW_ALL_INFO=false
KILL_PROCESSES=false
NATTCH_PROCESSES=()
PATTCH_PROCESSES=()

# CONSTANTES

EXECUTION_ERROR="La ejecución de strace ha fallado. Terminando programa..."
ERROR="Opción $1 no válida. Usa -h para ver el uso del programa. Terminando programa..."

# FUNCIONES

# Función para mostrar la información de uso del script
usage() {
    echo "scdebug [-h] [-sto arg] [-v | -vall] [-k] [prog [arg1 …]] [-nattch progtoattach ...] [-pattch pid1 ...]"
}

# Función para mostrar mensajes de error y finalizar el script
error() {
    echo "Error: $1"
    exit 1
}

# Función para mostrar información de procesos del usuario
show_processes_info() {
    # Utiliza 'ps' con opciones adecuadas para mostrar información de los procesos del usuario
    ps -e -o pid,ppid,comm,stime,tname | grep "$USER" | sort -k4,4
}

# Función para mostrar información de procesos trazadores y trazados, incluyendo stime
show_tracing_info() {
    # Utiliza 'ps' y /proc/PID/status para obtener información de procesos trazadores y trazados
    echo "Información de procesos trazadores y trazados:"
    for pid in $(pgrep -u "$USER"); do
        tracer_pid=$(grep -s "TracerPid" /proc/$pid/status | awk '{print $2}')
        if [ "$tracer_pid" -ne 0 ]; then
            tracee_name=$(ps -o comm= -p $pid)
            tracer_name=$(ps -o comm= -p $tracer_pid)
            stime=$(awk '{print $22}' /proc/$pid/stat)
            #modificacion
            echo "PID: $pid, Nombre: $tracee_name, Trazador: $tracer_pid ($tracer_name), Stime: $stime"
        fi
    done
}


# Función para terminar todos los procesos trazadores del usuario
kill_tracing_processes() {
    # Utiliza 'pkill' para enviar la señal KILL a todos los procesos trazadores
    pkill -9 -U "$USER" -f 'strace '
}

# Función para rastrear un programa especificado con strace
command_strace() {
    make_directory
    FILE=~/.scdebug/$PROGRAM/trace_$(uuidgen).txt

    # Agregar una declaración de depuración
    echo "Ejecutando command_strace para $PROGRAM con opciones $OPTIONS"

    # Ejecuta strace con las opciones proporcionadas y redirige la salida al archivo de registro
    strace $OPTIONS -o "$FILE" "$PROGRAM" "$@" &

    # No esperamos a la terminación de strace, lanzamos en segundo plano y gestionamos errores
    launch_strace "$!" "$FILE"
}

# Función para adjuntarse a un programa en ejecución y rastrearlo con strace
command_strace_attach() {
    make_directory
    FILE=~/.scdebug/$PROGRAM/trace_$(uuidgen).txt
    get_pid

    # Agregar una declaración de depuración
    echo "Ejecutando command_strace_attach para $PROGRAM con PID $PID"

    # Adjunta strace al programa en ejecución y redirige la salida al archivo de registro
    strace $OPTIONS -p "$PID" -o "$FILE" &

    # No esperamos a la terminación de strace, lanzamos en segundo plano y gestionamos errores
    launch_strace "$!" "$FILE"
}

# Función para lanzar strace en segundo plano y gestionar errores
launch_strace() {
    strace -p "$1" -o "$2" || {
        local error_message="Error en strace para el proceso PID: $1"
        echo "$error_message" 1>&2
        echo "$error_message" >> "$2"
    }
}

# PROGRAMA

while [[ $# -gt 0 ]]; do
    case $1 in
        -h | --help )
            usage
            exit 0
            ;;
        -sto )
            shift
            OPTIONS="$1"
            ;;
        -v )
            SHOW_INFO=true
            ;;
        -vall )
            SHOW_ALL_INFO=true
            ;;
        -k )
            KILL_PROCESSES=true
            ;;
        -nattch )
            shift
            while [[ $# -gt 0 && $1 != -* ]]; do
                NATTCH_PROCESSES+=("$1")
                shift
            done
            ;;
        -pattch )
            shift
            while [[ $# -gt 0 && $1 != -* ]]; do
                PATTCH_PROCESSES+=("$1")
                shift
            done
            ;;
        -* )
            error "Opción no válida: $1. Usa -h para ver el uso del programa."
            ;;
        * )
            PROGRAM="$1"
            shift
            if [ "$SHOW_INFO" = true ] || [ "$SHOW_ALL_INFO" = true ]; then
                show_processes_info
                show_tracing_info
            fi
            if [ -n "$PROGRAM" ]; then
                if [ "$KILL_PROCESSES" = true ]; then
                    kill_tracing_processes
                fi
                if [ "${#NATTCH_PROCESSES[@]}" -gt 0 ]; then
                    for prog in "${NATTCH_PROCESSES[@]}"; do
                        command_strace_attach
                    done
                fi
                if [ "${#PATTCH_PROCESSES[@]}" -gt 0 ]; then
                    for pid in "${PATTCH_PROCESSES[@]}"; do
                        command_strace
                    done
                fi
            fi
            ;;
    esac
    shift
done

exit 0