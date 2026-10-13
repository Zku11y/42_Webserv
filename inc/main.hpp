#include <iostream>
#include <vector>
#include <map>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <fcntl.h>
#include <sstream>
#include <fstream>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include "./http/config.hpp"

#define PORT 8080
#define BACKLOG 10
#define READ_BUFFER_SIZE 4096
#define MAX_REQUEST_SIZE (25 * 1024 * 1024)

#define ft_log_(x) std::cout << x
#define ft_log(x) std::cout << x << std::endl
#define ft_log_r(x) std::cerr << x << std::endl

struct HttpRequest {
    std::string method;         
    std::string uri;            
    std::string query_string;   

    std::string physical_path;  
    bool is_directory;          
    bool autoindex;             
    std::string cgi_executable;  
    std::string upload_store;    

    int body_fd;                
};

struct Client {
    int         fd;
    std::string request_buffer;
    std::string response_buffer; // Where your fully formatted HTTP response goes
    HttpRequest parsed_req;      // The mocked Dev 2 data

    std::string raw_body;   // Holds pure HTML/File data or CGI output
    int         status_code;
};


int main(int ac, char **av);
int ft_core(Config&  config);
void ft_set_error(Client& client, int status);
void ft_handle_resource(Client& client,Config&  config);
