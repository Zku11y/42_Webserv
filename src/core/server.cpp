#include "../../inc/main.hpp"
#include "../../inc/core/server.hpp"
#include "../../inc/Execution/Execution.hpp"
#include <netinet/in.h>

// TODO: any push back and other functions that allocate need to be checked so that they
//         don't throw and end the whole program


// ----------------------------------------------------------
// ----------------------------------------------------------

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
        ft_log("Body Content  : [" << rsp.body.substr(0, 200)  << "]");
    else
        ft_log("Body Content  : [Stored in file descriptor "
               << rsp.body_fd << "]");

    ft_log("========================================\n");
}

// ----------------------------------------------------------
// ----------------------------------------------------------


void sk_remove_pollfd(std::vector<pollfd> &pollfds, int fd){
  for(size_t i = 0; i < pollfds.size(); ++i){
    if(pollfds[i].fd == fd){
      pollfds.erase(pollfds.begin() + i);
      return;
    }
  }
}

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

std::string bytes_to_string(const sockaddr_in &address)
{
    const unsigned char *octets =
        reinterpret_cast<const unsigned char *>(&address.sin_addr.s_addr);

    std::ostringstream ip;
    ip << static_cast<unsigned int>(octets[0]) << '.'
       << static_cast<unsigned int>(octets[1]) << '.'
       << static_cast<unsigned int>(octets[2]) << '.'
       << static_cast<unsigned int>(octets[3]);

    return ip.str();
}

void add_client(int listenFd, std::vector<pollfd> &pollfds, std::map<int, Client> &clients, server_info &server){

  sockaddr_in client_addr = {};
  socklen_t client_len = sizeof(client_addr);
  

  int new_fd = accept(listenFd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
  if (new_fd < 0){
    std::cerr << "Error: Couldn't accept new client\n";
    return ;
  }
  
  if(!set_non_blocking(new_fd) || fcntl(new_fd, F_SETFD, FD_CLOEXEC) < 0){
    std::cerr << "Error: Couldn't set client fd to non blocking\n";
    close(new_fd);
    return ;
  }

  Client new_client = {};
  new_client.client_ip = bytes_to_string(client_addr); 
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

  if(!set_non_blocking(listenFd) || fcntl(listenFd, F_SETFD, FD_CLOEXEC) < 0)
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
  if(set_non_blocking(fd) && fcntl(fd, F_SETFD, FD_CLOEXEC) >= 0)
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
    return true;
  }

  sk_add_pipe(pipe_map, pollfds, client.cgi_rsp.stdin_write_fd, P_STDIN, POLLOUT, client);

  return true;
}

void sk_close_pipe(int &fd, CgiPipeMap &pipes, std::vector<pollfd> &pollfds){
  if(fd < 0) return;


  pipes.erase(fd);
  sk_remove_pollfd(pollfds, fd);
  close(fd);
  fd = -1;
}

bool sk_handle_pipes(CgiPipe pipe, CgiPipeMap &pipes, int i, std::vector<pollfd> &pollfds){
  if(pollfds[i].revents & POLLNVAL)
    return false;

  if(pipe.p_type == P_STDIN){
    
    if(!(pollfds[i].revents & (POLLERR | POLLHUP))){
       if(!(pollfds[i].revents & POLLOUT))
         return true;
      
        std::string &body = pipe.client.parsed_req.body;
        size_t &offset = pipe.client.cgi_rsp.body_offset;
        size_t size = body.size() - offset;
        ssize_t count = write(pollfds[i].fd, body.c_str() + offset, size < W_BUFF_SIZE ? size : W_BUFF_SIZE);

        if(count < 0)
          return false;

        offset += count;
        if(offset < body.size())
          return true;       
    }
    pipe.client.cgi_rsp.stdin_write_fd = -1;
  }
  else if(pipe.p_type == P_STDOUT){
    if((pollfds[i].revents & (POLLERR)))
      return false;
    if(!(pollfds[i].revents & (POLLIN | POLLHUP)))
      return true;

    char buffer[R_BUFF_SIZE];
    ssize_t r = read(pollfds[i].fd, buffer, R_BUFF_SIZE);

    if(r < 0)
      return false;
    if(r > 0){
      pipe.client.cgi_rsp.raw_output.append(buffer, r);
      if(pipe.client.cgi_rsp.raw_output.size() > MAX_CGI_OUTPUT)
        return false;
      return true;
    }
    pipe.client.cgi_rsp.stdout_eof = true;
    pipe.client.cgi_rsp.stdout_read_fd = -1;
  }

  close(pollfds[i].fd);
  pipes.erase(pollfds[i].fd);
  pollfds.erase(pollfds.begin() + i);
  return true;
}


void sk_generate_error(Client &crnt_client, std::vector<pollfd> &pollfds, int i, int error_code){

  crnt_client.parsed_req = HttpRequest();
  ft_log_request(crnt_client.parsed_req);
  ft_handle_error_3(crnt_client, error_code);
  ft_log_response(crnt_client.rsp);
  crnt_client.rsp.headers["Connection"] = "close";
  HttpResponseBuilder builder;
  builder.build(crnt_client.rsp);

  crnt_client.r_bytes = 0;
  crnt_client.status = WRITE_THEN_CLOSE;
  crnt_client.t_out = get_time();
  pollfds[i].events = POLLOUT;
}

int connection_handler(Config &cfg){

  std::map<int, Client> clients;
  std::map<int, CgiPipe> pipes;
  std::vector<pollfd> pollfds;
  std::vector<server_info> serverfds;
  std::vector<pid_t> retired_pids;
  
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
    
    std::set<int> clients_to_close;
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

      CgiPipeMap::iterator f = pipes.find(pollfds[i].fd);
      if(f != pipes.end()){
        int client_fd = f->second.client.fd;

        if(clients_to_close.count(client_fd))
          continue;

        if(!sk_handle_pipes(f->second, pipes, i, pollfds))
          clients_to_close.insert(client_fd);

        continue;
      }

      if(type != CLIENT || clients.empty()) continue; // if not server, its client, but if client isn't added yet then unused fd in pollfds

      // ft_log("Hi im a new client! :DDDD");
      std::map<int, Client>::iterator found = clients.find(pollfds[i].fd);
      if(found == clients.end()){ // IMPORTANT: add check for CGI pipe, this skips the moment its not a client fd
        close(pollfds[i].fd);
        pollfds.erase(pollfds.begin() + i);
        continue;
      }
          
      Client &crnt_client = found->second;
      double timeout = crnt_client.status == WAIT ? CGI_T_OUT : T_OUT;

      if(clients_to_close.count(crnt_client.fd))
        continue;

      if(pollfds[i].revents & (POLLERR | POLLNVAL)
        || ((pollfds[i].revents & POLLHUP) && !(pollfds[i].revents & (POLLIN | POLLOUT)))
        || (get_time() - crnt_client.t_out) > timeout){

        clients_to_close.insert(crnt_client.fd);
        continue;
      }
      
      if(crnt_client.status == READ){

        bool reached_eof = false;

        if(pollfds[i].revents & POLLIN){
          char buffer[R_BUFF_SIZE];
          ssize_t res = recv(pollfds[i].fd, buffer, sizeof(buffer), 0);

          // client sent data, write it in r_buff, parser reads it and continues creating request
          if (res > 0){ 
            crnt_client.r_bytes = res;
            crnt_client.tr_bytes += res;
            crnt_client.t_out = get_time();
            crnt_client.request_buffer.append(buffer, res);
            // crnt_client.parser.parse(crnt_client.request_buffer);
          }

          // client closes its end of write, switch to POLLOUT and send response
            // (got a full request and generated a proper response /
            //  got an unfinished request and generated an error response)
            // client socket auto closes when timeout is reached
          if(res == 0)
            reached_eof = true;
          //   crnt_client.r_bytes = 0; // parser must stop expecting more reads and finish the response
          //   crnt_client.status = WRITE_THEN_CLOSE; // write code should check for CLOSED before switching to READ and POLLIN, in that case close and cleanup
          //   pollfds[i].events = POLLOUT;
          //   crnt_client.t_out = get_time();

          //   // TODO: generate a response
          //   ft_log("here");
          // }

          // an error was flagged, but since we can't check errno we can't diff between
            // a server breaking error and a simple pause flag sent by the client, so we close the socket
          else if (res < 0){
            clients_to_close.insert(crnt_client.fd);
            continue;
          }
        }

        crnt_client.parser.parse(crnt_client.request_buffer);
      
        if (crnt_client.parser.has_error()) {
            sk_generate_error(crnt_client, pollfds, i, crnt_client.parser.get_error_code());
            continue;
        }

        else if(crnt_client.parser.is_complete()){
      
          crnt_client.parsed_req = crnt_client.parser.get_request();
          crnt_client.parsed_req.localHost = crnt_client.localHost; 
          crnt_client.parsed_req.localPort = crnt_client.localPort; 
          ft_log_request(crnt_client.parsed_req);
          crnt_client.cgi_rsp = CgiState();
          Execution exec_client(crnt_client);
          exec_client.ft_handle_resource(crnt_client, cfg);
        
          if(crnt_client.status == WAIT){
            crnt_client.t_out = get_time();
            pollfds[i].events = 0; // pause the client's events until cgi finishes

            if(!sk_add_pipes(pipes, pollfds, crnt_client))
              clients_to_close.insert(crnt_client.fd);

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

        else if(reached_eof){
          if(crnt_client.parser.get_state() == HttpParser::REQUEST_LINE && crnt_client.request_buffer.empty())
            clients_to_close.insert(crnt_client.fd);
          else{
            sk_generate_error(crnt_client, pollfds, i, HttpParser::BAD_REQUEST);
            continue;
          }     
        }

      }  // [ ... ] call the parse and CGI functions, must communicate when client is removed
          // get info on request finished to switch event to POLLOUT + status to WRITE and vise versa with the response 

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
            if(red < 0){
              clients_to_close.insert(crnt_client.fd);
              continue;
            }

          }
          
          if(!crnt_client.response_buffer.empty()){
             ssize_t res = send(pollfds[i].fd, crnt_client.response_buffer.c_str(), crnt_client.response_buffer.size(), MSG_NOSIGNAL);

            if(res > 0){

             crnt_client.response_buffer = crnt_client.response_buffer.substr(res, crnt_client.response_buffer.size());
             crnt_client.t_out = get_time();
            }

            if(res < 0){
              clients_to_close.insert(crnt_client.fd);
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
            clients_to_close.insert(crnt_client.fd);
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
              clients_to_close.insert(crnt_client.fd);
              continue;
            default:
              break;
          }
        }
      }
      
    
    } // for()

    for (std::map<int, Client>::iterator it = clients.begin();
         it != clients.end(); ++it)
    {
        Client &client = it->second;

        if (client.status != WAIT
            || clients_to_close.count(client.fd))
            continue;

        CgiState &cgi = client.cgi_rsp;

        if (!cgi.child_reaped) {
            int status = 0;
            pid_t result = waitpid(cgi.pid, &status, WNOHANG);

            if (result == 0)
                continue;

            if (result < 0) {
                if (errno == EINTR)
                    continue;

                if (errno == ECHILD)
                    cgi.pid = -1;

                clients_to_close.insert(client.fd);
                continue;
            }

            cgi.child_reaped = true;
            cgi.child_exit_status = status;
        }

        if (!cgi.stdout_eof)
            continue;

        sk_close_pipe(cgi.stdin_write_fd, pipes, pollfds);
        sk_close_pipe(cgi.stdout_read_fd, pipes, pollfds);

        ft_cgi_finish(client);

        ft_log_response(client.rsp);

        HttpResponseBuilder builder;
        builder.build(client.rsp);

        client.status = WRITE;
        client.t_out = get_time();

        for (size_t k = 0; k < pollfds.size(); ++k)
        {
            if (pollfds[k].fd == client.fd)
            {
                pollfds[k].events = POLLOUT;
                break;
            }
        } 
      
    
    }
  
    for(std::set<int>::iterator it = clients_to_close.begin();
      it != clients_to_close.end(); ++it){
      std::map<int, Client>::iterator found = clients.find(*it);
      if(found == clients.end())
        continue;

      Client &client = found->second;
      sk_close_pipe(client.cgi_rsp.stdin_write_fd, pipes, pollfds);
      sk_close_pipe(client.cgi_rsp.stdout_read_fd, pipes, pollfds);
      if(client.rsp.body_fd >= 0){
        close(client.rsp.body_fd);
        client.rsp.body_fd = -1;       
      }
      if(client.cgi_rsp.pid > 0 && !client.cgi_rsp.child_reaped){
        kill(client.cgi_rsp.pid, SIGKILL);
        retired_pids.push_back(client.cgi_rsp.pid);
      }

      sk_remove_pollfd(pollfds, client.fd);
      close(client.fd);

      clients.erase(found);
    }

    for(size_t i = 0; i < retired_pids.size();){
      pid_t result = waitpid(retired_pids[i], NULL, WNOHANG);

      if(result == retired_pids[i] || (result < 0 && errno == ECHILD))
        retired_pids.erase(retired_pids.begin() + i);
      else
        i++;
    }
    
  } // while()
   return 0;  
}


int main(int ac, char **av){

  std::string configFile;

  if(ac != 2) return 1;
  
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
      std::cerr << "Error: couldn't ignore SIGPIPE\n";
      return 1;
  }

  try{
    Config cfg(configFile);
    ft_log("Starting in 3, 2, 1......");
    connection_handler(cfg);

  }catch(std::exception &e){
    std::cerr << e.what();
    return 1;
  }
}
