#include "server.hpp"
#include <asm-generic/socket.h>
#include <fcntl.h>
#include <map>
#include <sys/poll.h>
#include <sys/socket.h>
#include <vector>

int set_non_blocking(int fd){
  int flags = fcntl(fd, F_GETFL, 0);
  if(flags < 0)
    return(std::cerr << "Error: fcntl(F_GETFL) failed", 0);

  if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)      
    return(std::cerr << "Error: fcntl(F_SETFL) failed", 0);

  return 1;
}

int add_client(int &listenFd, std::vector<pollfd> &pollfds, std::map<int, client> clients){

  int new_fd = accept(listenFd, NULL, NULL);
  if (new_fd >= 0){
    if(!set_non_blocking(new_fd))
      return (close(new_fd), 0);
      
    pollfd new_pclient = {};
    new_pclient.fd = new_fd;
    new_pclient.events = POLLIN;
    new_pclient.revents = 0;

    pollfds.push_back(new_pclient);

    client new_client = {};
    new_client.client_fd = new_fd;
    new_client.p_bytes = 0;
    new_client.w_bytes = 0;
    new_client.server_fd = listenFd;
    new_client.status = READ;
    clients[new_fd] = new_client; 
  }

  return 1;
}

int main(){

  int listenFd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd < 0)
    return (std::cerr << "Error: couldn't create listening socket\n", 1);

  sockaddr_in address = {};
  address.sin_addr.s_addr = htonl(SK_IP);
  address.sin_port = htons(SK_PORT);
  address.sin_family = AF_INET;

  int opt = 1;
  if(setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    return(std::cerr << "Error: setsockopt failed\n", close(listenFd), 1);

  if(!set_non_blocking(listenFd))
    return (1);

  if(bind(listenFd, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) < 0)
    return(std::cerr << "Error: couldn't bind socket to address and port\n", close(listenFd), 1);

  if(listen(listenFd, BACKLOG_MAX) < 0)
    return(std::cerr << "Error: couldn't set the socket to listen for connections\n", close(listenFd), 1);

 // std::vector<client> clients;
  std::map<int, client> clients; // IMPROV: using map instead of vector lets us iterate with the fd directly 
  std::vector<pollfd> pollfds;
  std::vector<pollfd> serverfds;

  pollfd listen_pfd = {};
    listen_pfd.fd = listenFd;
    listen_pfd.events = POLLIN;
    listen_pfd.revents = 0;
  serverfds.push_back(listen_pfd);
  
  pollfds.push_back(listen_pfd);

  while(true){
    if(poll(&serverfds[0], pollfds.size(), -1) < 0){
      std::cerr << "Error: poll failed\n";
      break;
    }
    
    for(int i = pollfds.size() - 1; i >= 0; i--){

      if(pollfds[i].revents == 0)
        continue;

      for(int j = 0; j < serverfds.size(); j++){
        if(pollfds[i].fd != serverfds[j].fd)
          continue;

        if(pollfds[i].revents & (POLLERR | POLLNVAL)){
          pollfds.erase(pollfds.begin() + i);
          serverfds.erase(serverfds.begin() + j);
          continue;
        }

        if(pollfds[i].revents & POLLIN){
          if(!add_client(listenFd, pollfds, clients))
            continue;
        }
      }

      
    }
        
  }
 return 0;
}
