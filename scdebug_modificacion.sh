#!/bin/bash

# Variables
OPTIONS=""
PROGRAM=""
FILE=""
TIME=""
NUMBER_FILES=
NUMBER_TACES=
AUX=

# Constantes
EXECUTION_ERROR="La ejecución de strace ha fallado. Terminando programa..."
ERROR="Opción $1 no válida. Usa -h para ver el uso del programa. Terminando programa..."Ç


# funciones

# Devuelve un mensaje de error por pantalla indicando el uso del programa
usage() {
    echo "scdebug [-h] [-sto arg] [-v | -vall] [-nattch progtoattach] [prog [arg1 …]]"
}

# Devuelve un mensaje de error por pantalla cuando se recibe una opción no valida
error() {
    echo "$ERROR"
    exit 1
}

# Devuelve un mensaje de error por pantalla y en el FILE de salida cuando strace no se ejecuta correctamente
comand_error() {
    echo "$EXECUTION_ERROR"
    echo "$EXECUTION_ERROR" >> $FILE
    exit 1
}

# Crea el directorio con el nombre del programa dentro del subdirectorio ~/.scdebug
make_directory() {
    mkdir -p ~/.scdebug/"$PROGRAM"  
}

# Mete en la variable PID el pid de un proceso
get_pid() {
   echo "Procesos con el nombre: $PROG_NAME"
        pgrep -a "$PROG_NAME" 


        echo "Ingrese el PID del proceso al que desea conectarse: " 
        read PID
}

# Ejecuta el comando strace de forma normal
command_strace() {
    make_directory
    FILE=~/.scdebug/$PROGRAM/trace_$(uuidgen).txt
    strace $OPTIONS -o "$FILE" $PROGRAM $@ &  # Al ejecutarse en 2º plano, el comando strace no da error aunque el comando no sea ejecutable
    strace $OPTIONS $PROGRAM $@ > /dev/null 2> $FILE # Para ello hacemos una segunda ejecución del comando strace, y redirigimos el error al fichero de salida y la salida la "borramos" enviandola a /dev/null
    if [ $? -ne 0 ]; then # Revisa que se haya ejcutado correctamente el comando strace
        comand_error
    fi
    exit 0
}

# Ejecuta el comando strace de forma attach
command_strace_attach() {
    make_directory
    FILE=~/.scdebug/$PROGRAM/trace_$(uuidgen).txt
    get_pid
    strace $OPTIONS -p $PID -o "$FILE" 2> >(tee "$FILE") # El comando tee dará la salida en el archivo de salida y por pantalla a su vez
    if [ $? -ne 0 ]; then # Revisa que se haya ejcutado correctamente el comando strace
        comand_error
    fi
    exit 0
}

# Programa principal

while [[ $# -gt 0 ]]; do  # Mientras que el número de parámetros sea mayor que 0, se hará:
    case $1 in 
        -h | --help ) # Opción para ver la ayuda del script
            usage 
            exit 0
            ;;
        -sto ) # Opcion sto, que envia las opciones para el comando strace
            shift 
            OPTIONS=$1
            ;;
        -nattch ) # Opcion nattch, que ejecuta el comando strace en modo attach
            shift
            PROGRAM=$1
            command_strace_attach
            ;;
        -* ) # Opción que detecta opciones no incluidas
            error
            exit 1
            ;; 
        * ) # Opción principal que lee el programa a ejecutar
            PROGRAM=$1
            shift
            command_strace
            ;; 
    esac
    shift
done

exit 0