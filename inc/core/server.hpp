#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <cstring>
#include <cerrno>

#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#define SK_IP INADDR_ANY
#define SK_PORT 8080
#define BACKLOG_MAX SOMAXCONN

enum states{
  READ,
  WRITE,
  WAIT
};

typedef struct client_info{

  std::string w_buff;
  std::string r_buff;

  states status;
  size_t w_bytes;
  size_t p_bytes; // REVIEW: parsed or already read bytes, so that the parser
                  //          doesn't have to re-parse everything everytime
  int client_fd;
  int server_fd;  
} client;

#endif
