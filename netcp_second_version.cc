#include <iostream>
#include <iomanip>
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
#include <csignal>

#define NETCP_PORT_ENV "NETCP_PORT"
#define NETCP_IP_ENV "NETCP_IP"

void signal_handler(int signo) {
  std::cerr << "netcp: terminando... (signal " << signo << ")\n";
  exit(EXIT_FAILURE);
}

std::error_code receive_from(int fd, std::vector<uint8_t>& buffer, sockaddr_in& address) {
  socklen_t addr_len = sizeof(address);
  ssize_t bytesReceived = recvfrom(fd, buffer.data(), buffer.size(), 0,
                                   reinterpret_cast<struct sockaddr*>(&address), &addr_len);
  if (bytesReceived == -1) {
    return std::error_code(errno, std::generic_category());
  }
  buffer.resize(bytesReceived);
  return std::error_code();
}

std::error_code write_file(int fd, const std::vector<uint8_t>& buffer) {
  ssize_t bytesWritten = write(fd, buffer.data(), buffer.size());
  if (bytesWritten == -1) {
    return std::error_code(errno, std::generic_category());
  }
  return std::error_code();
}

std::error_code send_to(int fd, const std::vector<uint8_t>& message, const sockaddr_in& address) {
  ssize_t bytesSent = sendto(fd, message.data(), message.size(), 0,
                             reinterpret_cast<const struct sockaddr*>(&address), sizeof(address));
  if (bytesSent == -1) {
    return std::error_code(errno, std::generic_category());
  }
  return std::error_code();
}

std::error_code read_file(int fd, std::vector<uint8_t>& buffer) {
  ssize_t bytesRead = read(fd, buffer.data(), buffer.size());
  if (bytesRead == -1) {
    return std::error_code(errno, std::generic_category());
  }
  buffer.resize(bytesRead);
  return std::error_code();
}

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

using make_socket_result = std::variant<int, std::error_code>;

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

std::error_code netcp_send_file(const std::string& filename) {
  // Crear la dirección IP y el socket
  std::optional<sockaddr_in> address = make_ip_address(std::optional<std::string>("127.0.0.1"), 8080);
  make_socket_result socketResult = make_socket();
  if (std::holds_alternative<std::error_code>(socketResult)) {
    std::cerr << "netcp: error al crear el socket: " << std::get<std::error_code>(socketResult).message() << std::endl;
    return std::get<std::error_code>(socketResult);
  }
  int sockfd = std::get<int>(socketResult);

  // Abrir el archivo en modo lectura
  int filefd = open(filename.c_str(), O_RDONLY);
  if (filefd == -1) {
    std::cerr << "netcp: no se puede abrir '" << filename << "': " << strerror(errno) << std::endl;
    close(sockfd);
    return std::error_code(errno, std::generic_category());
  }

  // Crear un buffer para almacenar el contenido del archivo
  std::vector<uint8_t> buffer(1024);  // Tamaño máximo de archivo

  // Leer el archivo y enviar datos por el socket
  while (true) {
    std::error_code readError = read_file(filefd, buffer);
    if (readError) {
      std::cerr << "netcp: error al leer el archivo: " << readError.message() << std::endl;
      close(filefd);
      close(sockfd);
      return readError;
    }

    std::error_code sendError = send_to(sockfd, buffer, address.value());
    if (sendError) {
      std::cerr << "netcp: error al enviar datos: " << sendError.message() << std::endl;
      close(filefd);
      close(sockfd);
      return sendError;
    }

    if (buffer.empty()) {
      // Fin de la transmisión
      break;
    }
  }

  // Cerrar el archivo y el socket
  close(filefd);
  close(sockfd);

  return std::error_code();
}

std::error_code netcp_receive_file(const std::string& filename) {
  uint16_t port = std::getenv(NETCP_PORT_ENV) ? std::stoi(std::getenv(NETCP_PORT_ENV)) : 8080;
  std::optional<sockaddr_in> address = make_ip_address(std::nullopt, port);
  make_socket_result socketResult = make_socket(address);
  if (std::holds_alternative<std::error_code>(socketResult)) {
    std::cerr << "netcp: error al crear el socket: " << std::get<std::error_code>(socketResult).message() << std::endl;
    return std::get<std::error_code>(socketResult);
  }
  int sockfd = std::get<int>(socketResult);

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);
  signal(SIGQUIT, signal_handler);

  int filefd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (filefd == -1) {
    std::cerr << "netcp: no se puede abrir '" << filename << "': " << strerror(errno) << std::endl;
    close(sockfd);
    return std::error_code(errno, std::generic_category());
  }

  std::vector<uint8_t> buffer(1024);
  while (true) {
    std::error_code receiveError = receive_from(sockfd, buffer, address.value());
    if (receiveError) {
      std::cerr << "netcp: error al recibir datos: " << receiveError.message() << std::endl;
      close(filefd);
      close(sockfd);
      return receiveError;
    }

    if (buffer.empty()) {
      break;
    }

    std::error_code writeError = write_file(filefd, buffer);
    if (writeError) {
      std::cerr << "netcp: error al escribir en el archivo: " << writeError.message() << std::endl;
      close(filefd);
      close(sockfd);
      return writeError;
    }
  }

  close(filefd);
  close(sockfd);
  return std::error_code();
}

int main(int argc, char* argv[]) {
  // Verificar el modo de funcionamiento
  if (argc < 2 || (strcmp(argv[1], "-l") != 0 && argc < 3)) {
    std::cerr << "Modo de empleo:\n";
    std::cerr << "  ./netcp ARCHIVO_DESTINO\n";
    std::cerr << "  ./netcp -l ARCHIVO_ORIGEN\n";
    return EXIT_FAILURE;
  }

  // Verificar si se está en el modo de escucha (-l)
  bool is_listener_mode = (strcmp(argv[1], "-l") == 0);

  // Obtener el nombre del archivo de destino u origen
  const char* filename = is_listener_mode ? argv[2] : argv[1];

  if (is_listener_mode) {
    return netcp_receive_file(filename).value();
  } else {
    return netcp_send_file(filename).value();
  }
}
