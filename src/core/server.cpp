#include "../../inc/core/server.hpp"
#include <cerrno>
#include <exception>
#include <fcntl.h>
#include <poll.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <map>
#include <utility>

// TODO: any push back and other functions that allocate need to be checked so that they
//         don't throw and end the whole program

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

void add_client(int listenFd, std::vector<pollfd> &pollfds, std::map<int, client> &clients, server_info &server){

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

  client new_client;
  new_client.client_fd = new_fd;
  new_client.r_bytes = 0;
  new_client.w_bytes = 0;
  new_client.tr_bytes = 0;
  new_client.tw_bytes = 0;
  new_client.server_fd = listenFd;
  new_client.t_out = get_time();
  new_client.status = READ;
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

int connection_handler(Config &cfg){

  std::map<int, client> clients;
  std::vector<pollfd> pollfds;
  std::vector<server_info> serverfds;

  std::vector<ServerConfig> servers = cfg.getServers();

  for(size_t i = 0; i < servers.size(); i++){
    add_listener(pollfds, serverfds, servers[i]);
  }
  
  while(true){
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
        j--;
        if(pollfds[i].fd != serverfds[j].fd)
          continue;

        type = SERVER;
        if(pollfds[i].revents & (POLLERR | POLLNVAL | POLLHUP)){
          close(pollfds[i].fd);
          pollfds.erase(pollfds.begin() + i);
          serverfds.erase(serverfds.begin() + j);
       }
        else if(pollfds[i].revents & POLLIN)
          add_client(pollfds[i].fd, pollfds, clients, serverfds[i]);

        break;
      }

      if(type == SERVER || clients.empty()) continue; // if not server, its client, but if client isn't added yet then unused fd in pollfds

      std::map<int, client>::iterator found = clients.find(pollfds[i].fd);
      if(found == clients.end()){ // IMPORTANT: add check for CGI pipe, this skips the moment its not a client fd
        close(pollfds[i].fd);
        pollfds.erase(pollfds.begin() + i);
        continue;
      }
          
      client &crnt_client = found->second;
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
          crnt_client.r_buff.append(buffer, res);
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
        }

        // an error was flagged, but since we can't check errno we can't diff between
          // a server breaking error and a simple pause flag sent by the client, so we close the socket
        else if (res < 0){
          close(pollfds[i].fd);
          pollfds.erase(pollfds.begin() + i);
          clients.erase(found);
          continue;
        }

        // [ ... ] call the parse and CGI functions, must communicate when client is removed
          // get info on request finished to switch event to POLLOUT + status to WRITE and vise versa with the response 

    

      }

      if(pollfds[i].revents & POLLOUT){
        ssize_t bytes_left = crnt_client.w_buff.size() - crnt_client.tw_bytes;
        ssize_t res = send(pollfds[i].fd, crnt_client.w_buff.c_str() + crnt_client.tw_bytes,
                                bytes_left >= W_BUFF_SIZE ? W_BUFF_SIZE : bytes_left, MSG_NOSIGNAL);

        if(res > 0){
      
          if(crnt_client.tw_bytes + res == crnt_client.w_buff.size()){
            switch(crnt_client.status){
              case WRITE:
                crnt_client.status = READ;
                pollfds[i].events = POLLIN;
                crnt_client.tw_bytes = 0;
                crnt_client.w_bytes = 0;
                crnt_client.w_buff.clear();
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
          else{
            crnt_client.w_bytes = res;
            crnt_client.tw_bytes += res;
          }

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
     
    } // for()
        
  } // while()
   return 0;  
}

int main(int ac, char **av){

  if(ac != 2){
  // TODO: add explicit message about program arguments
    return 1;
  }

  try{
    Config cfg(av[1]);
    connection_handler(cfg);

  }catch(std::exception &e){
    std::cerr << e.what();
    return 1;
  }

}
