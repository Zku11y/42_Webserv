#ifndef SERVER_HPP
#define SERVER_HPP

#include <bits/types/struct_timeval.h>
#include <iostream>
#include <cstring>
#include <cerrno>

#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#define SK_IP INADDR_ANY
#define SK_PORT 8080
#define BACKLOG_MAX SOMAXCONN
#define T_OUT 500

enum states{
  READ,
  WRITE,
  WRITE_THEN_CLOSE,
  WAIT
};

enum typefd{
  SERVER,
  CLIENT
};

typedef struct client_info{

  std::string w_buff;
  std::string r_buff;

  states status;
  size_t tw_bytes;
  size_t tr_bytes; // REVIEW: parsed or already read bytes, so that the parser
  size_t w_bytes;   //         doesn't have to re-parse everything everytime
  size_t r_bytes;
  int client_fd;
  int server_fd;
  
  double t_out;
  
} client;

#endif
