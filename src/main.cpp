#include <arpa/inet.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <ostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  // Disable output buffering
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket: " << std::endl;
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    close(server_fd);
    std::cerr << "setsockopt failed: " << std::endl;
    return 1;
  }

  struct sockaddr_in server_addr {};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(9092);

  if (bind(server_fd, reinterpret_cast<struct sockaddr *>(&server_addr),
           sizeof(server_addr)) != 0) {
    close(server_fd);
    std::cerr << "Failed to bind to port 9092" << std::endl;
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    close(server_fd);
    std::cerr << "listen failed" << std::endl;
    return 1;
  }

  std::cout << "Waiting for a client to connect...\n";

  struct sockaddr_in client_addr {};
  socklen_t client_addr_len = sizeof(client_addr);

  // You can use print statements as follows for debugging, they'll be visible
  // when running tests.
  std::cerr << "Logs from your program will appear here!\n";

  int client_fd =
      accept(server_fd, reinterpret_cast<struct sockaddr *>(&client_addr),
             &client_addr_len);
  std::cout << "Client connected\n";

  char buffer[256];
  int r = recv(client_fd, buffer, sizeof(buffer), 0);
  if (r < 0) {
    std::cout << "Error: No message recieved" << std::endl;
  }

  std::uint32_t size;
  std::uint32_t correlation_id;
  std::uint16_t api_ver;
  std::uint16_t api_key;

  std::memcpy(&api_key, buffer + 4, sizeof(std::uint16_t));
  api_key = ntohs(api_key);
  std::memcpy(&api_ver, buffer + 6, sizeof(std::uint16_t));
  api_ver = ntohs(api_ver);

  std::memcpy(&correlation_id, buffer + 8, sizeof(std::uint32_t));
  size = sizeof(correlation_id);

  std::uint16_t error_code;
  if (api_ver < 0 || api_ver > 4) {
    error_code = htons(35);
  } else {
    error_code = htons(0);
  }

  std::uint8_t length = 0x02;
  std::uint16_t key = htons(18);
  std::uint16_t min = htons(0);
  std::uint16_t max = htons(4);
  std::uint8_t api_tag = 0x00;
  std::uint32_t throttle_time = htonl(0);
  std::uint8_t tag = 0x00;

  size += sizeof(error_code) + sizeof(length) + sizeof(key) + sizeof(min) +
          sizeof(max) + sizeof(api_tag) + sizeof(throttle_time) + sizeof(tag);

  size = htonl(size);
  send(client_fd, &size, sizeof(size), 0);
  send(client_fd, &correlation_id, sizeof(correlation_id), 0);
  send(client_fd, &error_code, sizeof(error_code), 0);
  send(client_fd, &length, sizeof(length), 0);
  send(client_fd, &key, sizeof(key), 0);
  send(client_fd, &min, sizeof(min), 0);
  send(client_fd, &max, sizeof(max), 0);
  send(client_fd, &api_tag, sizeof(api_tag), 0);
  send(client_fd, &throttle_time, sizeof(throttle_time), 0);
  send(client_fd, &tag, sizeof(tag), 0);

  close(client_fd);
  close(server_fd);
  return 0;
}
