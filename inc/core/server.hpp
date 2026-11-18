#ifndef SERVER_HPP
#define SERVER_HPP

#include "../http/http.hpp"
#include "../http/config.hpp"
#include "../http/configParser.hpp"
#include "../http/response.hpp"


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
#define W_BUFF_SIZE 4096
#define R_BUFF_SIZE 4096

#define ft_log(x) std::cout << x << std::endl

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

typedef struct server_info{
  int fd;
  int port;
  std::string ip;
  size_t max_size;
} sk_server;

typedef struct client_info{

  std::string request_buffer;
  std::string response_buffer;

  int fd;
  states status;
  size_t tw_bytes;
  size_t tr_bytes; // REVIEW: parsed or already read bytes, so that the parser
  size_t w_bytes;   //         doesn't have to re-parse everything everytime
  size_t r_bytes;
  int localPort;
  std::string localHost; // REVIEW: server socket could error, causing segfault
  size_t max_size;
  
  double t_out;

  // anwar files

  HttpParser parser;
  HttpRequest parsed_req;
  HttpResponse rsp;

  client_info() : parser(){}
  
  // 
  
} Client;

#endif
