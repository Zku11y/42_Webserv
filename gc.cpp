#include <asm-generic/socket.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>
#include <fcntl.h>
#include <poll.h>


//     x     x     x     x
//     ^     ^     ^     ^
//   turn /  X  /  Y  / state (win / lose / ongoing)


void set_non_blocking(int &socket){
  int flags = fcntl(socket, F_GETFL, 0);
  fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}

int main(){
  int server_socket;

  server_socket = socket(AF_INET, SOCK_STREAM, 0);

  set_non_blocking(server_socket);
  int opt = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in server_addr;
  server_addr.sin_port = htons(6767);
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_family = AF_INET;

  if(bind(server_socket, (sockaddr *)&server_addr, sizeof(server_addr)) < 0){
    std::cerr << "couldn't bind socket to port and IP\n";
    close(server_socket);
    return 1;
  }

  listen(server_socket, 1);

  std::cout << "listening for the 2nd player's connection...\n";

  int client_socket;
  sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  while(true){
    client_socket = accept(server_socket, (sockaddr *)&client_addr, &client_len);
    if(client_socket < 0){
      //std::cerr << "couldn't connect player 2 to server\n";
      continue;
    }
      break;
  }

  std::cout << "Client connected! chat to display messages\n";

  const char *prompt = "\033[1;32mServer : \033[0m";

  while(true){

  std::cout << "\033[2;5Htesting\033[0m";

    pollfd fds[2];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = client_socket;
    fds[1].events = POLLIN;

    char buffer[2000] = {0};
    char wr_buff[2000] = {0};

    if(poll(fds, 2, -1) < 0){
      std::cerr << "Error: poll couldn't poll events\n";
      break;
    }


    if(fds[1].revents & POLLIN){

      int result = read(client_socket, buffer, sizeof(buffer));

      if(result < 0){
        std::cerr << "Read error\n";
        break;
      }
      if(result == 0){
        std::cout << "Client disconnected\n";
        break;
      }  
      std::cout << buffer;

    }
    if(fds[0].revents & POLLIN){
      std::cin.getline(wr_buff, sizeof(wr_buff));
      int len = strlen(wr_buff);
      if(len > 0){
        write(client_socket, prompt, strlen(prompt));
        write(client_socket, wr_buff, len);
        write(client_socket, "\n", 1);
        std::cout << prompt << wr_buff << std::endl;
      }
    }
  }  

  close(server_socket);
  return 0;
}
