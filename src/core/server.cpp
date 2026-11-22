#include "../../inc/main.hpp"
#include "../../inc/core/server.hpp"
#include "../../inc/Execution/Execution.hpp"
#include <sys/poll.h>

// TODO: any push back and other functions that allocate need to be checked so that they
//         don't throw and end the whole program


// ----------------------------------------------------------
// ----------------------------------------------------------

// hi

static void ft_log_request(const HttpRequest& req)
{
    ft_log("\n========================================");
    ft_log("          PARSED HTTP REQUEST");
    ft_log("========================================");

    // Connection information
    ft_log("\n[CONNECTION]");
    ft_log("Local Host    : [" << req.localHost << "]");
    ft_log("Local Port    : [" << req.localPort << "]");

    // Request line
    ft_log("\n[REQUEST LINE]");
    ft_log("Method        : [" << req.method << "]");
    ft_log("Target        : [" << req.target << "]");
    ft_log("Path          : [" << req.path << "]");
    ft_log("Query String  : [" << req.queryString << "]");
    ft_log("HTTP Version  : [" << req.version << "]");

    // Parsed Host header
    ft_log("\n[HOST]");
    ft_log("Host          : [" << req.host << "]");
    ft_log("Port          : [" << req.port << "]");

    // Headers
    // ft_log("\n[HEADERS] (" << req.headers.size() << ")");

    // for (std::map<std::string, std::string>::const_iterator it =
    //      req.headers.begin(); it != req.headers.end(); ++it)
    // {
    //     ft_log("  " << it->first << " : [" << it->second << "]");
    // }

    // Body
    ft_log("\n[BODY]");
    ft_log("Body Size     : " << req.body.size() << " bytes");
    ft_log("Body Content  : [" << req.body << "]");

    ft_log("========================================\n");
}
static void ft_log_response(const HttpResponse& rsp)
{
    ft_log("\n========================================");
    ft_log("          HTTP RESPONSE");
    ft_log("========================================");

    // Status
    ft_log("\n[STATUS]");
    ft_log("Status Code   : [" << rsp.status_code << "]");

    // Headers
    ft_log("\n[HEADERS] (" << rsp.headers.size() << ")");

    for (std::map<std::string, std::string>::const_iterator it =
         rsp.headers.begin(); it != rsp.headers.end(); ++it)
    {
        ft_log("  " << it->first << " : [" << it->second << "]");
    }

    // Body
    ft_log("\n[BODY]");
    ft_log("Body FD       : [" << rsp.body_fd << "]");
    ft_log("Body Size     : [" << rsp.body_size << "] bytes");
    ft_log("String Size   : [" << rsp.body.size() << "] bytes");

    if (rsp.body_fd == -1)
        ft_log("Body Content  : [" << rsp.body << "]");
    else
        ft_log("Body Content  : [Stored in file descriptor "
               << rsp.body_fd << "]");

    ft_log("========================================\n");
}

// ----------------------------------------------------------
// ----------------------------------------------------------




int set_non_blocking(int fd){
  if(fcntl(fd, F_SETFL, O_NONBLOCK) < 0)      
    return(std::cerr << "Error: fcntl(F_SETFL) failed", 0);
  return 1;
}

double get_time(){
  timeval tv;

  gettimeofday(&tv, NULL);

  return ((double)tv.tv_sec * 1000.0) + ((double)tv.tv_usec / 1000.0);
}

void add_client(int listenFd, std::vector<pollfd> &pollfds, std::map<int, Client> &clients, server_info &server){

  int new_fd = accept(listenFd, NULL, NULL);
  if (new_fd < 0){
    std::cerr << "Error: Couldn't accept new client\n";
    return ;
  }
  
  if(!set_non_blocking(new_fd)){
    std::cerr << "Error: Couldn't set client fd to non blocking\n";
    close(new_fd);
    return ;
  }

  Client new_client;
  new_client.fd = new_fd;
  new_client.r_bytes = 0;
  new_client.w_bytes = 0;
  new_client.tr_bytes = 0;
  new_client.tw_bytes = 0;
  new_client.localHost = server.ip;
  new_client.localPort = server.port;
  new_client.t_out = get_time();
  new_client.status = READ;
  new_client.rsp.body_fd = -1;
  new_client.parser.setMaxBodySize(server.max_size);
  if(!clients.insert(std::make_pair(new_fd, new_client)).second){
    std::cerr << "Error: Client fd already exists in map\n";
    close(new_fd);
    return ;
  }
  
  pollfd new_pclient = {};
  new_pclient.fd = new_fd;
  new_pclient.events = POLLIN;
  new_pclient.revents = 0;
  pollfds.push_back(new_pclient);
}

uint32_t convert_ip(std::string &ip){
    unsigned int parts[4] = {0};
    int part = 0;
    int digits = 0;

    for (size_t i = 0; i < ip.size(); i++)
    {
        if (ip[i] == '.')
        {
            if (digits == 0 || part >= 3)
                return -1;
            part++;
            digits = 0;
        }
        else if (ip[i] >= '0' && ip[i] <= '9')
        {
            if (++digits > 3)
                return -1;
            parts[part] = parts[part] * 10 + (ip[i] - '0');
            if (parts[part] > 255)
                return -1;
        }
        else
            return -1;
    }

    if (part != 3 || digits == 0)
        return -1;

    uint32_t result = (parts[0] << 24) |
                      (parts[1] << 16) |
                      (parts[2] << 8)  |
                       parts[3];

    return result;
}


int add_listener(std::vector<pollfd> &pollfds, std::vector<server_info> &serverfds, ServerConfig &server){
  int listenFd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd < 0)
    return (std::cerr << "Error: couldn't create listening socket\n", -1);

  sockaddr_in address = {};

  std::string ip = server.getHost();
  int port = server.getPort();

  int host = convert_ip(ip);

 if (host < 0) {
      close(listenFd);
      std::cerr << "Invalid IP address format.\n";
      return 1;
 }

  address.sin_addr.s_addr = htonl(host);
  address.sin_port = htons(port);
  address.sin_family = AF_INET;

  int opt = 1;
  if(setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    return(std::cerr << "Error: setsockopt failed\n", close(listenFd), -1);

  if(!set_non_blocking(listenFd))
    return (close(listenFd), -1);

  if(bind(listenFd, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) < 0)
    return(std::cerr << "Error: couldn't bind socket to address and port\n", close(listenFd), -1);

  if(listen(listenFd, BACKLOG_MAX) < 0)
    return(std::cerr << "Error: couldn't set the socket to listen for connections\n", close(listenFd), -1);

  server_info listen_info = {};
  listen_info.fd = listenFd;
  listen_info.ip = ip;
  listen_info.port = port;
  listen_info.max_size = server.getClientMaxBodySize();

  pollfd listen_pfd = {};
    listen_pfd.fd = listenFd;
    listen_pfd.events = POLLIN;
    listen_pfd.revents = 0;

  pollfds.push_back(listen_pfd);
  serverfds.push_back(listen_info);

  return listenFd;
}


bool sk_setup_fd(int fd){
  set_non_blocking(fd);
  if(fcntl(fd, F_SETFD, FD_CLOEXEC) > 0)
    return 1;
  return 0;
}

void sk_add_pipe(CgiPipeMap &pipe_map, std::vector<pollfd> &pollfds, int pipe_fd, pipe_type type, int event, Client &client){
  CgiPipe pipe = {
    pipe_fd,
    type,
    client
  };

  pollfd pfd;
  pfd.events = event;
  pfd.fd = pipe_fd;
  pfd.revents = 0;

  pipe_map.insert(std::make_pair(pipe_fd, pipe));
  pollfds.push_back(pfd);
}

//void sk_close_pipe();

bool sk_add_pipes(CgiPipeMap &pipe_map, std::vector<pollfd> &pollfds, Client &client){
    if(!sk_setup_fd(client.cgi_rsp.stdout_read_fd) || !sk_setup_fd(client.cgi_rsp.stdin_write_fd))
      return false;

    sk_add_pipe(pipe_map, pollfds, client.cgi_rsp.stdout_read_fd, P_STDOUT, POLLIN, client);
    if(client.parsed_req.body.empty()){
      close(client.cgi_rsp.stdin_write_fd);
      client.cgi_rsp.stdin_write_fd = -1;
    }
        
    sk_add_pipe(pipe_map, pollfds, client.cgi_rsp.stdin_write_fd, P_STDIN, POLLOUT, client);
    return true;
}

bool sk_handle_pipes(CgiPipe pipe, pollfd &pfd){
  if(pipe.p_type == P_STDIN){
    
    if(!(pfd.revents & (POLLERR | POLLHUP))){
       if(!(pfd.revents & POLLOUT))
         return false;
    }
    
  }
}

int connection_handler(Config &cfg){

  std::map<int, Client> clients;
  std::map<int, CgiPipe> pipes;
  std::vector<pollfd> pollfds;
  std::vector<server_info> serverfds;

  std::vector<ServerConfig> servers = cfg.getServers();

  for(size_t i = 0; i < servers.size(); i++){
    add_listener(pollfds, serverfds, servers[i]);
  }

  // ft_log("Prototype listening on http://localhost:"<< client.port);

  while(true){
    // std::cout << "hi\n";
    if(pollfds.empty()){
      std::cerr << "All sockets (server and client) were closed\n";
      break;
    }
    int res = poll(&pollfds[0], pollfds.size(), 100); // TODO: may need to change timeout to something like 50 or 100
    if(res < 0){
      if(errno == EINTR) // REASON: in the case of interruptions such as CGI signals
        continue;
      std::cerr << "Error: poll failed\n";
      break;
    }
    
    for(size_t i = pollfds.size(); i > 0;){
      i--;
      
      //if(pollfds[i].revents == 0)
      //  continue;

      typefd type = CLIENT;

      for(size_t j = serverfds.size(); j > 0;){
      // ft_log("bye");
      j--;
        
      
      // std::cout << serverfds[j].ip << ":" << serverfds[j].port << "\n";
        if(pollfds[i].fd != serverfds[j].fd)
          continue;

        type = SERVER;
        if(pollfds[i].revents & (POLLERR | POLLNVAL | POLLHUP)){
          close(pollfds[i].fd);
          pollfds.erase(pollfds.begin() + i);
          serverfds.erase(serverfds.begin() + j);
       }
        else if(pollfds[i].revents & POLLIN)
          add_client(pollfds[i].fd, pollfds, clients, serverfds[j]);

        break;
      }

      if(type == SERVER)
        continue;

      std::map<int, CgiPipe>::iterator f = pipes.find(pollfds[i].fd);
      if(f == pipes.end())
        continue;

      if(type != CLIENT || clients.empty()) continue; // if not server, its client, but if client isn't added yet then unused fd in pollfds

      // ft_log("Hi im a new client! :DDDD");
      std::map<int, Client>::iterator found = clients.find(pollfds[i].fd);
      if(found == clients.end()){ // IMPORTANT: add check for CGI pipe, this skips the moment its not a client fd
        close(pollfds[i].fd);
        pollfds.erase(pollfds.begin() + i);
        continue;
      }
          
      Client &crnt_client = found->second;
      if(pollfds[i].revents & (POLLERR | POLLNVAL)
        || ((pollfds[i].revents & POLLHUP) && !(pollfds[i].revents & (POLLIN | POLLOUT)))
        || (get_time() - crnt_client.t_out) > T_OUT){

        close(pollfds[i].fd);
        pollfds.erase(pollfds.begin() + i);
        clients.erase(found);
        continue;
      }

      if(pollfds[i].revents & POLLIN){
        char buffer[R_BUFF_SIZE];
        ssize_t res = recv(pollfds[i].fd, buffer, sizeof(buffer), 0);

        // client sent data, write it in r_buff, parser reads it and continues creating request
        if (res > 0){ 
          crnt_client.r_bytes = res;
          crnt_client.tr_bytes += res;
          crnt_client.t_out = get_time();
          crnt_client.request_buffer.append(buffer, res);
          crnt_client.parser.parse(crnt_client.request_buffer);
        }

        // client closes its end of write, switch to POLLOUT and send response
          // (got a full request and generated a proper response /
          //  got an unfinished request and generated an error response)
          // client socket auto closes when timeout is reached
        else if(res == 0){      
          crnt_client.r_bytes = 0; // parser must stop expecting more reads and finish the response
          crnt_client.status = WRITE_THEN_CLOSE; // write code should check for CLOSED before switching to READ and POLLIN, in that case close and cleanup
          pollfds[i].events = POLLOUT;
          crnt_client.t_out = get_time();

          // TODO: generate a response
          ft_log("here");
          
          continue;
        }

        // an error was flagged, but since we can't check errno we can't diff between
          // a server breaking error and a simple pause flag sent by the client, so we close the socket
        else if (res < 0){
          close(pollfds[i].fd);
          pollfds.erase(pollfds.begin() + i);
          clients.erase(found);
          continue;
        }

        if (crnt_client.parser.has_error()) {
            ft_log("its a fucking errorrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
    
            const int error_code = crnt_client.parser.get_error_code();
            

            crnt_client.parsed_req = HttpRequest();
            ft_log_request(crnt_client.parsed_req);
            Execution exec_client(crnt_client);
            exec_client.ft_handle_error_2(crnt_client, error_code);
            ft_log_response(crnt_client.rsp);
            crnt_client.rsp.headers["Connection"] = "close";
            HttpResponseBuilder builder;
            builder.build(crnt_client.rsp);

            crnt_client.r_bytes = 0;
            crnt_client.status = WRITE_THEN_CLOSE;
            crnt_client.t_out = get_time();
            pollfds[i].events = POLLOUT;
            continue;
        }

        else if(crnt_client.parser.is_complete()){
        
          crnt_client.parsed_req = crnt_client.parser.get_request();
          crnt_client.parsed_req.localHost = crnt_client.localHost; 
          crnt_client.parsed_req.localPort = crnt_client.localPort; 
          ft_log_request(crnt_client.parsed_req);
          Execution exec_client(crnt_client);
          exec_client.ft_handle_resource(crnt_client, cfg);
          
          if(crnt_client.status == WAIT){
            pollfds[i].events = 0; // pause the client's events until cgi finishes

            if(!sk_add_pipes(pipes, pollfds, crnt_client)){
              close(pollfds[i].fd);
              pollfds.erase(pollfds.begin() + i);
              clients.erase(found);
            }

            continue;
          }
          
          crnt_client.r_bytes = 0;
          crnt_client.status = WRITE; 
          pollfds[i].events = POLLOUT;
          crnt_client.t_out = get_time();
          ft_log_response(crnt_client.rsp);

          HttpResponseBuilder builder;
          builder.build(crnt_client.rsp);

        }

        // [ ... ] call the parse and CGI functions, must communicate when client is removed
          // get info on request finished to switch event to POLLOUT + status to WRITE and vise versa with the response 

    

      }

      if(pollfds[i].revents & POLLOUT){
 
      //	else if (crnt_client.rsp.body_size <= MAX_INLINE_BODY)
      // 

        if(crnt_client.rsp.body_fd != -1 && crnt_client.tw_bytes == crnt_client.rsp.responc.size()){

//          std::cout << "ITS USING THE BODY_FD FINALLYYYYYYYYY\n";
    
          char buff[W_BUFF_SIZE];
          if(crnt_client.response_buffer.empty()){
            ssize_t red = read(crnt_client.rsp.body_fd, buff, sizeof(buff));
  
            if(red > 0){
              crnt_client.response_buffer.append(buff, red);
            }
            if(red == 0){
              close(crnt_client.rsp.body_fd); // should i close this fd or do the other's code close it?
              crnt_client.rsp.body_fd = -1;
            }

          }
          if(!crnt_client.response_buffer.empty()){
             ssize_t res = send(pollfds[i].fd, crnt_client.response_buffer.c_str(), crnt_client.response_buffer.size(), MSG_NOSIGNAL);

            if(res > 0){

             crnt_client.response_buffer = crnt_client.response_buffer.substr(res, crnt_client.response_buffer.size());
             crnt_client.t_out = get_time();
            }

            if(res < 0){
              close(pollfds[i].fd);
              close(crnt_client.rsp.body_fd);
              clients.erase(found);
              pollfds.erase(pollfds.begin() + i);
              continue;
            } 
          }
          
          continue;
        }

        if(crnt_client.tw_bytes < crnt_client.rsp.responc.size()){
          ssize_t bytes_left = crnt_client.rsp.responc.size() - crnt_client.tw_bytes;
          ssize_t res = send(pollfds[i].fd, crnt_client.rsp.responc.c_str() + crnt_client.tw_bytes,
                                bytes_left >= W_BUFF_SIZE ? W_BUFF_SIZE : bytes_left, MSG_NOSIGNAL);

          if(res > 0){
            crnt_client.w_bytes = res;
            crnt_client.tw_bytes += res;
            crnt_client.t_out = get_time();
          }

          // if res == 0 : ignore and move on. if we keep getting 0 timeout will eventually stop it

          if(res < 0){
            close(pollfds[i].fd);
            clients.erase(found);
            pollfds.erase(pollfds.begin() + i);
            continue;
          }
        }

        if(crnt_client.response_buffer.empty() && crnt_client.rsp.body_fd == -1 && crnt_client.tw_bytes == crnt_client.rsp.responc.size()){
          switch(crnt_client.status){
            case WRITE:
              crnt_client.status = READ;
              pollfds[i].events = POLLIN;
              crnt_client.tw_bytes = 0;
              crnt_client.w_bytes = 0;
              crnt_client.rsp.responc.clear();
              crnt_client.parser.reset();
              crnt_client.response_buffer.clear();
              break;
            case WRITE_THEN_CLOSE:
              close(pollfds[i].fd);
              clients.erase(found);
              pollfds.erase(pollfds.begin() + i);
              continue;
            default:
              break;
          }
        }
      }
     
      
    
    } // for()
        
  } // while()
   return 0;  
}


int main(int ac, char **av){

  std::string configFile;
  configFile = (ac != 2) ? "default.conf" : av[1];

  try{
    Config cfg(configFile);
    connection_handler(cfg);

  }catch(std::exception &e){
    std::cerr << e.what();
    return 1;
  }
}
