#include <iostream>
#include <iomanip>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>
#include <optional>
#include <fcntl.h>
#include <vector>
#include <variant>
#include <system_error>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <atomic>

// Definición de constantes para la configuración del puerto e IP
constexpr char NETCP_PORT_ENV[] = "NETCP_PORT";
constexpr char NETCP_IP_ENV[] = "NETCP_IP";

// Bandera atómica para indicar si se ha solicitado salir
std::atomic<bool> quit_requested{false};

// Manejador de señales para gestionar la terminación del programa
void signal_handler(int signo) {
  std::cout << "netcp: terminando... (signal " << signo << ")\n";
  quit_requested.store(true);
}

// Función para recibir datos desde un socket UDP
std::error_code receive_from(int fd, std::vector<uint8_t>& buffer, sockaddr_in& address) {
  socklen_t addr_len = sizeof(address);
  ssize_t bytesReceived = recvfrom(fd, buffer.data(), buffer.size(), 0,
                                   reinterpret_cast<struct sockaddr*>(&address), &addr_len);
  if (bytesReceived == -1) {
    return std::error_code(errno, std::generic_category());
  }
  buffer.resize(bytesReceived);
  return {};
}

// Función para escribir datos en un archivo
std::error_code write_file(int fd, const std::vector<uint8_t>& buffer) {
  ssize_t bytesWritten = write(fd, buffer.data(), buffer.size());
  if (bytesWritten == -1) {
    return std::error_code(errno, std::generic_category());
  }
  return {};
}

// Función para enviar datos a través de un socket UDP
std::error_code send_to(int fd, const std::vector<uint8_t>& message, const sockaddr_in& address) {
  ssize_t bytesSent = sendto(fd, message.data(), message.size(), 0,
                             reinterpret_cast<const struct sockaddr*>(&address), sizeof(address));
  if (bytesSent == -1) {
    return std::error_code(errno, std::generic_category());
  }
  return {};
}

// Función para leer datos desde un archivo
std::error_code read_file(int fd, std::vector<uint8_t>& buffer) {
  ssize_t bytesRead = read(fd, buffer.data(), buffer.size());
  if (bytesRead == -1) {
    return std::error_code(errno, std::generic_category());
  }
  buffer.resize(bytesRead);
  return {};
}

// Función para crear una dirección IP
std::optional<sockaddr_in> make_ip_address(const std::optional<std::string> ip_address, uint16_t port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (ip_address.has_value()) {
    if (inet_pton(AF_INET, ip_address.value().c_str(), &(addr.sin_addr)) <= 0) {
      return std::nullopt;
    }
  } else {
    addr.sin_addr.s_addr = INADDR_ANY;
  }
  return addr;
}

// Tipo de resultado para la creación del socket
using make_socket_result = std::variant<int, std::error_code>;

// Función para crear un socket
make_socket_result make_socket(std::optional<sockaddr_in> address = std::nullopt) {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    return std::error_code(errno, std::generic_category());
  }
  if (address.has_value()) {
    if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&address.value()), sizeof(address.value())) == -1) {
      close(sockfd);
      return std::error_code(errno, std::generic_category());
    }
  }
  return sockfd;
}

// Función para enviar un archivo a través de netcp
std::error_code netcp_send_file(const std::string& filename) {
  // Configuración de la dirección IP y el puerto
  std::optional<sockaddr_in> address = make_ip_address(std::optional<std::string>("127.0.0.1"), 8080);

  // Creación del socket
  make_socket_result socketResult = make_socket();
  if (std::holds_alternative<std::error_code>(socketResult)) {
    std::cout << "netcp: error al crear el socket: " << std::get<std::error_code>(socketResult).message() << '\n';
    return std::get<std::error_code>(socketResult);
  }
  int sockfd = std::get<int>(socketResult);

  // Apertura del archivo a enviar
  int filefd = open(filename.c_str(), O_RDONLY);
  if (filefd == -1) {
    std::cout << "netcp: no se puede abrir '" << filename << "': " << std::strerror(errno) << '\n';
    close(sockfd);
    return std::error_code(errno, std::generic_category());
  }

  // Buffer para almacenar los datos leídos del archivo
  std::vector<uint8_t> buffer(1024);

  // Bucle de lectura, envío y comprobación de finalización
  while (true) {
    // Lectura de datos desde el archivo
    std::error_code readError = read_file(filefd, buffer);
    if (readError) {
      std::cout << "netcp: error al leer el archivo: " << readError.message() << '\n';
      close(filefd);
      close(sockfd);
      return readError;
    }

    // Envío de datos a través del socket
    std::error_code sendError = send_to(sockfd, buffer, address.value());
    if (sendError) {
      std::cout << "netcp: error al enviar datos: " << sendError.message() << '\n';
      close(filefd);
      close(sockfd);
      return sendError;
    }

    // Comprobación de finalización
    if (buffer.empty() || quit_requested.load()) {
      break;
    }
  }

  // Cierre de archivos y sockets
  close(filefd);
  close(sockfd);

  return {};
}

// Función para recibir un archivo a través de netcp
std::error_code netcp_receive_file(const std::string& filename) {
  // Obtención del puerto desde la variable de entorno
  uint16_t port = std::getenv(NETCP_PORT_ENV) ? std::stoi(std::getenv(NETCP_PORT_ENV)) : 8080;

  // Configuración de la dirección IP y el puerto
  std::optional<sockaddr_in> address = make_ip_address(std::nullopt, port);

  // Creación del socket
  make_socket_result socketResult = make_socket(address);
  if (std::holds_alternative<std::error_code>(socketResult)) {
    std::cout << "netcp: error al crear el socket: " << std::get<std::error_code>(socketResult).message() << '\n';
    return std::get<std::error_code>(socketResult);
  }
  int sockfd = std::get<int>(socketResult);

  // Configuración del manejador de señales
  struct sigaction sa {};
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;  // No usar SA_RESTART para permitir interrupciones en llamadas al sistema
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGHUP, &sa, nullptr);
  sigaction(SIGQUIT, &sa, nullptr);

  // Apertura del archivo para escritura
  int filefd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (filefd == -1) {
    std::cout << "netcp: no se puede abrir '" << filename << "': " << std::strerror(errno) << '\n';
    close(sockfd);
    return std::error_code(errno, std::generic_category());
  }

  // Buffer para almacenar los datos recibidos del socket
  std::vector<uint8_t> buffer(1024);

  // Bucle de recepción, escritura y comprobación de finalización
  while (true) {
    // Recepción de datos desde el socket
    std::error_code receiveError = receive_from(sockfd, buffer, address.value());
    if (receiveError) {
      std::cout << "netcp: error al recibir datos: " << receiveError.message() << '\n';
      close(filefd);
      close(sockfd);
      return receiveError;
    }

    // Comprobación de finalización
    if (buffer.empty() || quit_requested.load()) {
      break;
    }

    // Escritura de datos en el archivo
    std::error_code writeError = write_file(filefd, buffer);
    if (writeError) {
      std::cout << "netcp: error al escribir en el archivo: " << writeError.message() << '\n';
      close(filefd);
      close(sockfd);
      return writeError;
    }
  }

  // Cierre de archivos y sockets
  close(filefd);
  close(sockfd);
  return {};
}

int main(int argc, char* argv[]) {
  // Verificación de los argumentos de línea de comandos
  if (argc < 2 || (std::strcmp(argv[1], "-l") == 0 && argc < 3)) {
    std::cout << "Modo de empleo:\n";
    std::cout << "  ./netcp ARCHIVO_DESTINO\n";
    std::cout << "  ./netcp -l ARCHIVO_ORIGEN\n";
    return EXIT_FAILURE;
  }

  // Determina el modo del programa
  bool is_listener_mode = (std::strcmp(argv[1], "-l") == 0);
  const char* filename = is_listener_mode ? argv[2] : argv[1];

  // Llama a la función correspondiente según el modo
  if (is_listener_mode) {
    return netcp_receive_file(filename).value();
  } else {
    return netcp_send_file(filename).value();
  }
}
