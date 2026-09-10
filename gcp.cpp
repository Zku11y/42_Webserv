#include <asm-generic/socket.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <poll.h>

void set_non_blocking(int &socket){
  int flags = fcntl(socket, F_GETFL, 0);
  fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}


int main(int ac, char **av){

  int player_socket = socket(AF_INET, SOCK_STREAM, 0);


  int opt = 1;
  setsockopt(player_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in client_addr;
  client_addr.sin_port = htons(6767);
  client_addr.sin_family = AF_INET;

  const char *addr = "127.0.0.1";

  if(inet_pton(AF_INET, addr, &client_addr.sin_addr) <= 0){
    close(player_socket); 
    std::cerr << "invalid address\n";
    return 1;
  }

  if(connect(player_socket, (sockaddr *)&client_addr, sizeof(client_addr)) < 0){
    close(player_socket);
    std::cerr << "Couldn't connect to server\n";
    return 1;
  }

  set_non_blocking(player_socket);

  char buffer[2000] = {0};
  char rd_buff[2000] = {0};

  pollfd fds[2];
  fds[0].fd = player_socket;
  fds[0].events = POLLIN;
  fds[1].fd = STDIN_FILENO;
  fds[1].events = POLLIN;

  const char *prompt = "\033[1;31mClient : \033[0m";

  while(true){

    if(poll(fds, 2, -1) < 0){
      std::cerr << "Error: couldn't poll events\n";
      break;
    }

    if(fds[0].revents & POLLIN){
      int result = read(player_socket, rd_buff, sizeof(rd_buff));
      if(result < 0){
        std::cerr << "Read error\n";
        break;
      }
      if(result == 0){
        std::cout << "Server Stopped\n";
        break;
      }
    
      std::cout << rd_buff;
    }

    
    if(fds[1].revents & POLLIN){
      std::cin.getline(buffer, sizeof(buffer));
      int len = strlen(buffer);
      if(len > 0){
        write(player_socket, prompt, strlen(prompt));
        write(player_socket, buffer, len);
        write(player_socket, "\n", 1);
        std::cout << prompt << buffer << std::endl;
      }
    }
  }  
  close(player_socket);
  return 0;
}
