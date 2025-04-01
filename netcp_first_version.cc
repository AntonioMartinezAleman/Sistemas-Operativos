// g++ -o netcp -Wall -std=c++17 netcp.cc
// netcat -lu 8080 > testfile2
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <optional>
#include <fcntl.h>
#include <vector>
#include <cstring>
#include <variant>
#include <system_error>
#include <cerrno>
#include <format>

// Función para leer un archivo y almacenar su contenido en un buffer
std::error_code read_file(int fd, std::vector<uint8_t>& buffer) {
    ssize_t bytesRead = read(fd, const_cast<void*>(static_cast<const void*>(buffer.data())), buffer.size());
    if (bytesRead == -1) {
        return std::error_code(errno, std::generic_category());
    }
    buffer.resize(bytesRead);
    return std::error_code();  // Éxito
}

// Función para escribir un buffer en un archivo
std::error_code write_file(int fd, const std::vector<uint8_t>& buffer) {
  // Implementación de la función de escritura del archivo
  ssize_t bytesWritten = write(fd, buffer.data(), buffer.size());
  if (bytesWritten == -1) {
    return std::error_code(errno, std::generic_category());
  }
  return std::error_code();  // Éxito
}

// Función para crear una dirección IP
std::optional<sockaddr_in> make_ip_address(const std::optional<std::string> ip_address, uint16_t port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (ip_address.has_value()) {
    if (inet_pton(AF_INET, ip_address.value().c_str(), &(addr.sin_addr)) <= 0) {
      return std::nullopt;  // Error al convertir la dirección IP
    }
  } else {
    addr.sin_addr.s_addr = INADDR_ANY;
  }
  return addr;
}

// Tipo de resultado al crear un socket
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

// Función para enviar datos a una dirección
std::error_code send_to(int fd, const std::vector<uint8_t>& message, const sockaddr_in& address) {
  ssize_t bytesSent = sendto(fd, message.data(), message.size(), 0,
                             reinterpret_cast<const struct sockaddr*>(&address), sizeof(address));
  if (bytesSent == -1) {
    return std::error_code(errno, std::generic_category());
  }
  return std::error_code();  // Éxito
}




int main(int argc, char* argv[]) {
  // Verificar si se proporciona el nombre del archivo o si se solicita ayuda
  if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
    std::cerr << "Modo de empleo: netcp [-h] ORIGEN\n";
    return (argc < 2) ? EXIT_FAILURE : EXIT_SUCCESS;
  }
  // Obtener el nombre del archivo de los argumentos de la línea de comandos
  const char* filename = argv[1];
  // Crear un buffer para almacenar el contenido del archivo
  std::vector<uint8_t> buffer(1024);  // Tamaño máximo de archivo
  // Abrir el archivo en modo lectura
  int filefd = open(filename, O_RDONLY);
  if (filefd == -1) {
    std::cerr << "netcp: no se puede abrir '" << filename << "': " << strerror(errno) << std::endl;
    return EXIT_FAILURE;
  }

  // Crear la dirección IP y el socket
  std::optional<sockaddr_in> address = make_ip_address(std::optional<std::string>("127.0.0.1"), 8080);
  make_socket_result socketResult = make_socket();
  if (std::holds_alternative<std::error_code>(socketResult)) {
    std::cerr << "netcp: error al crear el socket: " << std::get<std::error_code>(socketResult).message() << std::endl;
    return EXIT_FAILURE;
  }
  int sockfd = std::get<int>(socketResult);

  while(!buffer.empty()) {
    std::error_code error = read_file(filefd, buffer);
    if (error) {
      std::cerr << std::format("error ({}): {}\n", error.value(), error.message());
      return 1;
    }
    std::error_code sendError = send_to(sockfd, buffer, address.value());
    if (sendError) {
      std::cerr << "netcp: error al enviar datos: " << sendError.message() << std::endl;
      close(sockfd);
      return EXIT_FAILURE;
    }
  }

  // Cerrar el socket
  close(sockfd);
  return 0;
}