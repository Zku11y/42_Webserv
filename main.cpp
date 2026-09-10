#include <cstring>
#include <getopt.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <vector>

#define BUFFER_SIZE 2000

void set_non_blocking(int &fd){
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(){

  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if(server_socket < 0)
    return (std::cerr << "error : server socket not created\n", 1);

  set_non_blocking(server_socket);

  int opt = 1;

  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8080);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if(bind(server_socket, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    return (std::cerr << "Error: couldn't bind socket to port and IP\n", close(server_socket), 1);

  listen(server_socket, 10);

  std::cout << "waiting for a client to connect...\n";

  std::vector<pollfd> fds;

  pollfd server_poll_fd;
  server_poll_fd.events = POLLIN;
  server_poll_fd.fd = server_socket;
  fds.push_back(server_poll_fd);

  while(true){
    if(poll(fds.data(), fds.size(), -1) < 0)
      break;

    for(int i = 0; i < fds.size(); i++){

      if((fds[i].revents & POLLIN) == 0) continue;

      if(fds[i].fd == server_socket){

        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_socket, (sockaddr *)&client_addr, &client_len);

        if(client_fd >= 0){
          std::cout << "new client connected successfully!\n";
          set_non_blocking(client_fd);

          pollfd client_poll_fd;
          client_poll_fd.fd = client_fd;
          client_poll_fd.events = POLLIN;
          fds.push_back(client_poll_fd);
        }
        
      }
      else{
        char buffer[BUFFER_SIZE] = {0};
        if(read(fds[i].fd, buffer, BUFFER_SIZE) <= 0){
          std::cerr << "Failed to read from client fd\n";
          close(fds[i].fd);
          fds.erase(fds.begin() + i);
          i--;
        }
        else{

          std::cout << "successfully read from client fd, sending response...\n";

          const char *response =
            "This is a response to the client\n"
            "if ts works im trying to send a png next\n";
          
          if(write(fds[i].fd, response, strlen(response)) < 0){
            std::cerr << "couldn't write to the fd :(\n";
            close(fds[i].fd);
            fds.erase(fds.begin() + i);
            i--;
          }
          std::cout << "response sent!\n";
        }
      }
      
    }

  }
  close(server_socket);
  return 0;
}
