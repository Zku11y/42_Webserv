#ifndef SERVER_HPP
#define SERVER_HPP

#include "../main.hpp"
#include "../http/http.hpp"
#include "../http/config.hpp"
#include "../http/configParser.hpp"
#include "../http/response.hpp"

#define SK_IP INADDR_ANY
#define SK_PORT 8080
#define BACKLOG_MAX SOMAXCONN
#define T_OUT 500
#define W_BUFF_SIZE 24096
#define R_BUFF_SIZE 24096

enum states{
  READ,
  WRITE,
  WRITE_THEN_CLOSE,
  WAIT
};

enum typefd{
  SERVER,
  CLIENT,
  CGI
};

struct CgiState
{
    pid_t pid;
    int stdin_write_fd;
    int stdout_read_fd;

    size_t body_offset;
    std::string raw_output;

    bool stdout_eof;
    bool child_reaped;
    int child_exit_status;

    CgiState()
        : pid(-1),
          stdin_write_fd(-1),
          stdout_read_fd(-1),
          body_offset(0),
          stdout_eof(false),
          child_reaped(false),
          child_exit_status(0)
    {}
};

typedef struct server_info{
  int fd;
  int port;
  std::string ip;
  size_t max_size;
} sk_server;

typedef struct client_info{
  
  int fd;

  int localPort;
  std::string localHost; // REVIEW: server socket could error, causing segfault

  
  size_t tw_bytes;
  size_t tr_bytes; // REVIEW: parsed or already read bytes, so that the parser
  size_t w_bytes;   //         doesn't have to re-parse everything everytime
  size_t r_bytes;
  
  size_t max_size;
  
  double t_out;
  
  
  std::string request_buffer;

    HttpParser parser;
    HttpRequest parsed_req;

    states status;
    CgiState cgi_rsp;
    
    HttpResponse rsp;

  std::string response_buffer;


  client_info() : parser(),cgi_rsp(){}
  
} Client;

enum pipe_type{
  P_STDIN,
  P_STDOUT
};

struct CgiPipe{
  int pipe_fd;
  pipe_type p_type;
  Client &client;  
};

typedef std::map<int, CgiPipe> CgiPipeMap;

#endif
