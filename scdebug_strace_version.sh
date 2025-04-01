#!/bin/bash

# VARIABLES
OPTIONS=""
PROGRAM=""
FILE=""
TIME=""
NUMBER_FILES=""
NUMBER_TACES=""
AUX=""
natt=""
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
    dir=.scdebug/$prog
    commando=$(ls -t $dir | head -1)
    time=$(date -r $dir | cut -d " " -f1-4)
    echo "=============== COMMAND: "$dir" ======================="
    echo "=============== TRACE FILE: $commando ================="
    echo "=============== TIME: $time =============="
    echo
}

# Función para mostrar información de procesos trazadores y trazados
vall() {
    dir=".scdebug/$prog"
    num=0
    for commando in $(ls -t "$dir"); do
        time=$(date -r "$dir/$commando" | cut -d " " -f1-4)
        echo "=============== COMMAND: $dir/$commando ======================="
        echo "=============== TRACE FILE: $commando ================="
        echo "=============== TIME: $time =============="
        echo
        ((num++))
    done
    if [ $num -eq 0 ]; then
        echo "No se encontraron archivos de traza en $dir"
    fi
}

# Función para terminar todos los procesos trazadores del usuario
kill_tracing_processes() {
    # Utiliza 'pkill' para enviar la señal KILL a todos los procesos trazadores
    echo "$pid"
    kill -9 $pid
}

# Función para rastrear un programa especificado con strace
command_strace() {
    #make_directory
    mkdir -p .scdebug/"$prog" 
    prog=$(ps -o comm -p "$pid" --no-headers)

    FILE=.scdebug/$prog/trace_$(uuidgen).txt

    # Agregar una declaración de depuración
    echo "Ejecutando command_strace para $prog con opciones $OPTIONS"

    # No esperamos a la terminación de strace, lanzamos en segundo plano y gestionamos errores
    launch_strace "$pid" "$FILE" &
}

# Función para adjuntarse a un programa en ejecución y rastrearlo con strace
command_strace_attach() {

    mkdir -p .scdebug/"$prog" 

    FILE=.scdebug/$prog/trace_$(uuidgen).txt
    
    PID=$(ps -e -opid,comm | grep $prog | sort -r | head -n1 | awk '{print $1}')

    # Agregar una declaración de depuración
    echo "Ejecutando command_strace_attach para $prog con PID $PID situado en $FILE"

    # No esperamos a la terminación de strace, lanzamos en segundo plano y gestionamos errores
    launch_strace "$PID" "$FILE" &
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

# Control de opciones
if [ -z $1 ]; then
    echo "No hay argumentos. Prueba -h"
fi

while [ $# -gt 0 ]; do
    case $1 in
        -h | --help )
            usage
            exit 0
            ;;
        -sto )
            shift
            OPTIONS="$1"
            break;
            ;;
        -v )
            shift
            SHOW_INFO=true
            break;
            ;;
        -vall )
            shift
            SHOW_ALL_INFO=true
            break;
            ;;
        -k )
            KILL_PROCESSES=true
            shift
            break;
            ;;
        -S ) 
            commName="$2"
            echo -n "traced_$commName" > /proc/$$/comm
            kill -SIGSTOP $$
            exec "$@"
            ;;


        -nattch )
            shift
            natt=1
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
            echo "strace -o .scdebug/$PROGRAM/trace_$(uuidgen).txt para "$PROGRAM" "
            strace -o .scdebug/$PROGRAM/trace_$(uuidgen).txt $OPTIONS "$PROGRAM" "$@" &
            shift
            ;;
    esac
    shift
done

# Comprobación de variables y ejecución de funciones
if [ "$SHOW_INFO" = true ]; then
    prog=$1
    show_processes_info
fi
if [ "$SHOW_ALL_INFO" = true ]; then
    prog=$1
    vall
fi

    if [ "$KILL_PROCESSES" = true ]; then
        pid="$1"
        kill_tracing_processes
    fi

    if [ "$natt" == 1 ]; then
        for prog in "${NATTCH_PROCESSES[@]}"; do
            command_strace_attach
        done
    fi

    if [ "${#PATTCH_PROCESSES[@]}" -gt 0 ]; then
        for pid in "${PATTCH_PROCESSES[@]}"; do
            command_strace 
        done
    fi


exit 0