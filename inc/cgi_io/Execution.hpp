#pragma once


#include <sys/stat.h>
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
#include <stdexcept>
#include <sys/types.h>
#include <dirent.h>

#include "../http/http.hpp"
#include "../http/config.hpp"
#include "../http/client.hpp"
#include "../http/HttpResponse.hpp"


#define ft_log_(x) std::cout << x
#define ft_log(x) std::cout << x << std::endl
#define ft_log_r(x) std::cerr << x << std::endl


class Execution
{
    private:
        int status;
        
        HttpRequest req;
        HttpResponse rsp;

        ServerConfig s_conf;
        LocationConfig s_info;

        std::string full_path;

        bool cgi_exc;
        std::string cgi_path;


        void ft_handle_Get(Client& client);
        void ft_handle_G_dir(Client& client);
        void ft_handle_G_autoindex(Client& client);
        void ft_handle_G_files(Client& client, struct stat& info);


        void ft_handle_Post(Client& client);


        void ft_handle_Delete(Client& client);


        void ft_handle_Redirect(Client& client);


        bool ft_is_cgi();
        void ft_handle_cgi();


        int ft_error_1(Client& client);
        void ft_error_2(Client& client);
        void ft_handle_error(Client& client);

    public:
        Execution(Client& client, Config& conf);
        void ft_handle_resource(Client& client);
        
};

template <typename T>
std::string ft_to_string(T value)
{
    std::ostringstream ss;
    ss << value;
    return ss.str();
}