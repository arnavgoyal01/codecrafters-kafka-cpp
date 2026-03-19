#include <arpa/inet.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <netdb.h>
#include <netinet/in.h>
#include <ostream>
#include <string>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

void describeTopicPartitions(const char *buffer, int client_fd);
void apiVersions(const char *buffer, int client_fd);

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

  fd_set masterfds;
  std::vector<int> clientfds;
  int maxfd;
  char buffer[256];
  int p = 1;

  while (p != clientfds.size()) {
    p = clientfds.size();
    FD_ZERO(&masterfds);
    FD_SET(server_fd, &masterfds);
    maxfd = server_fd;
    for (auto cfd : clientfds) {
      FD_SET(cfd, &masterfds);
      if (cfd > maxfd)
        maxfd = cfd;
    }
    int activity = select(maxfd + 1, &masterfds, NULL, NULL, NULL);

    if (activity < 0) {
      std::cerr << "select error\n";
      std::cout << "Error code: " << errno << " Num of client fds "
                << clientfds.size() << "\n";
    }

    if (FD_ISSET(server_fd, &masterfds)) {
      struct sockaddr_in client_addr;
      int client_addr_len = sizeof(client_addr);
      int client_fd1 = accept(server_fd, (struct sockaddr *)&client_addr,
                              (socklen_t *)&client_addr_len);
      if (client_fd1 < 0) {
        std::cerr << "client 1 accept failed\n";
      }
      clientfds.push_back(client_fd1);
    }
  }

  for (int client_fd : clientfds) {
    bzero(buffer, 256);
    int r = recv(client_fd, buffer, sizeof(buffer), 0);
    if (r < 0) {
      std::cout << "Error: No message recieved" << std::endl;
    }

    while (r > 0) {
      int p = 0;
      std::uint32_t size;
      std::memcpy(&size, buffer, sizeof(size));
      size = ntohl(size);

      p += sizeof(size);
      std::uint16_t api_key;
      std::memcpy(&api_key, buffer + p, sizeof(api_key));
      api_key = ntohs(api_key);

      if (api_key == 75) {
        describeTopicPartitions(buffer, client_fd);
      } else if (api_key == 18) {
        apiVersions(buffer, client_fd);
      }

      bzero(buffer, 256);
      r = 0;

      struct timeval tv;
      tv.tv_sec = 1;
      tv.tv_usec = 0;
      int activity = select(maxfd + 1, &masterfds, NULL, NULL, &tv);
      if (activity < 0) {
        std::cout << "Select error\n";
      }
      if (FD_ISSET(client_fd, &masterfds)) {
        r = recv(client_fd, buffer, sizeof(buffer), 0);
      }
    }
  }

  for (int client_fd : clientfds) {
    close(client_fd);
  }

  close(server_fd);
  return 0;
}

void describeTopicPartitions(const char *buffer, int client_fd) {
  int p = 0;
  std::uint32_t size;
  std::memcpy(&size, buffer, sizeof(size));
  size = ntohl(size);

  p += sizeof(size);
  std::uint16_t api_key;
  std::memcpy(&api_key, buffer + p, sizeof(api_key));
  api_key = ntohs(api_key);
  p += sizeof(api_key);
  std::uint16_t api_ver;
  std::memcpy(&api_ver, buffer + p, sizeof(api_ver));
  api_ver = ntohs(api_ver);

  p += sizeof(api_ver);
  std::uint32_t correlation_id;
  std::memcpy(&correlation_id, buffer + p, sizeof(correlation_id));

  p += sizeof(correlation_id);
  std::uint16_t length;
  std::memcpy(&length, buffer + p, sizeof(length));
  length = ntohs(length);
  std::cout << "LENGTH " << length << "\n";

  p += sizeof(length) + (int)length + 1;
  std::uint8_t array_length;
  std::memcpy(&array_length, buffer + p, sizeof(array_length));
  std::cout << "ARRAY LENGTH " << (int)array_length << "\n";

  p += sizeof(array_length);
  std::uint8_t name_length;
  std::memcpy(&name_length, buffer + p, sizeof(name_length));
  std::cout << "NAME LENGTH " << (int)name_length << "\n";

  p += 1;
  std::vector<std::uint8_t> topic_name;
  topic_name.insert(topic_name.end(), &buffer[p],
                    &buffer[p + (int)name_length - 1]);
  // p + (int)name_length - 1

  std::uint32_t throttle_time = htonl(0);
  std::uint8_t tag = 0x00;
  std::uint8_t is_internal = 0x00;
  std::uint8_t partitions_array = static_cast<std::uint8_t>(0);
  std::uint32_t topic_authorized_operations = htonl(0);
  std::uint8_t cursor = static_cast<std::uint8_t>(-1);
  std::vector<std::uint8_t> topic_id;
  std::uint8_t zeroes = static_cast<std::uint8_t>(0);
  topic_id.insert(topic_id.end(), 16, zeroes);

  std::cout << "TOPIC ID " << topic_id.size() << "\n";
  std::uint16_t error_code = htons(3);
  size = sizeof(correlation_id) + sizeof(tag) + sizeof(throttle_time) +
         sizeof(array_length) + sizeof(error_code) + sizeof(name_length) +
         topic_name.size() + topic_id.size() + sizeof(is_internal) +
         sizeof(partitions_array) + sizeof(topic_authorized_operations) +
         sizeof(tag) + sizeof(cursor) + sizeof(tag);
  size = htonl(size);

  send(client_fd, &size, sizeof(size), 0);
  send(client_fd, &correlation_id, sizeof(correlation_id), 0);
  send(client_fd, &tag, sizeof(tag), 0);
  send(client_fd, &throttle_time, sizeof(throttle_time), 0);
  send(client_fd, &array_length, sizeof(array_length), 0);
  send(client_fd, &error_code, sizeof(error_code), 0);
  send(client_fd, &name_length, sizeof(name_length), 0);
  send(client_fd, topic_name.data(), topic_name.size(), 0);
  send(client_fd, topic_id.data(), topic_id.size(), 0);
  send(client_fd, &is_internal, sizeof(is_internal), 0);
  send(client_fd, &partitions_array, sizeof(partitions_array), 0);
  send(client_fd, &topic_authorized_operations,
       sizeof(topic_authorized_operations), 0);
  send(client_fd, &tag, sizeof(tag), 0);
  send(client_fd, &cursor, sizeof(cursor), 0);
  send(client_fd, &tag, sizeof(tag), 0);
}

void apiVersions(const char *buffer, int client_fd) {
  int p = 0;
  std::uint32_t size;
  std::memcpy(&size, buffer, sizeof(size));
  size = ntohl(size);

  p += sizeof(size);
  std::uint16_t api_key;
  std::memcpy(&api_key, buffer + p, sizeof(api_key));
  api_key = ntohs(api_key);

  p += sizeof(api_key);
  std::uint16_t api_ver;
  std::memcpy(&api_ver, buffer + p, sizeof(api_ver));
  api_ver = ntohs(api_ver);

  p += sizeof(api_ver);
  std::uint32_t correlation_id;
  std::memcpy(&correlation_id, buffer + p, sizeof(correlation_id));

  std::uint16_t error_code;
  if (api_ver < 0 || api_ver > 4) {
    error_code = htons(35);
  } else {
    error_code = htons(0);
  }

  std::uint8_t length = 0x03;
  std::uint16_t key = htons(18);
  std::uint16_t min = htons(0);
  std::uint16_t max = htons(4);
  std::uint8_t api_tag = 0x00;
  std::uint16_t key2 = htons(75);
  std::uint16_t min2 = htons(0);
  std::uint16_t max2 = htons(0);
  std::uint32_t throttle_time = htonl(0);
  std::uint8_t tag = 0x00;

  size = sizeof(correlation_id) + sizeof(error_code) + sizeof(length) +
         sizeof(key) + sizeof(min) + sizeof(max) + sizeof(api_tag) +
         sizeof(key) + sizeof(min) + sizeof(max) + sizeof(api_tag) +
         sizeof(throttle_time) + sizeof(tag);
  size = htonl(size);

  send(client_fd, &size, sizeof(size), 0);
  send(client_fd, &correlation_id, sizeof(correlation_id), 0);
  send(client_fd, &error_code, sizeof(error_code), 0);
  send(client_fd, &length, sizeof(length), 0);
  send(client_fd, &key, sizeof(key), 0);
  send(client_fd, &min, sizeof(min), 0);
  send(client_fd, &max, sizeof(max), 0);
  send(client_fd, &api_tag, sizeof(api_tag), 0);
  send(client_fd, &key2, sizeof(key2), 0);
  send(client_fd, &min2, sizeof(min2), 0);
  send(client_fd, &max2, sizeof(max2), 0);
  send(client_fd, &api_tag, sizeof(api_tag), 0);
  send(client_fd, &throttle_time, sizeof(throttle_time), 0);
  send(client_fd, &tag, sizeof(tag), 0);
}
