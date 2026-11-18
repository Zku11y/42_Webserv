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
#include "../core/server.hpp"
#include "../http/HttpResponse.hpp"


#define ft_log_(x) std::cout << x
#define ft_log(x) std::cout << x << std::endl
#define ft_log_r(x) std::cerr << x << std::endl


class Execution
{
    private:
        int status;

        HttpRequest req;

        ServerConfig s_conf;
        LocationConfig s_location;

        std::string full_path;

        bool cgi_exc;
        std::string cgi_path;


    private:
            //  Get :
        void ft_handle_Get(Client& client);
        void ft_handle_G_dir(Client& client, int& fd);
        void ft_handle_G_autoindex(Client& client);
        void ft_handle_G_files(Client& client, struct stat& info, int& fd);
        std::string ft_mime_types();

            //  Post :
        void ft_handle_Post(Client& client);

            //  Delet :
        void ft_handle_Delete(Client& client);

            //  Red :
        void ft_handle_Redirect(Client& client);
        void ft_handle_Redirect_2(Client& client);

            //  Cgi :
        bool ft_is_cgi();
        void ft_handle_cgi();

            //  Errors :
        int ft_error_1(Client& client);
        void ft_error_2(Client& client);
        void ft_handle_error(Client& client, int s);
        void ft_handle_error_2(Client& client, int s);

            //  Others :
        std::vector<std::string> ft_split(const std::string& str, char del) const;
        std::string ft_to_lower(const std::string& str) const;
        std::string ft_trim(const std::string& str);


            // init :
        int ft_select_server(Config& conf);
        int ft_validate_path();
        int ft_select_location();
        int ft_validate_methods();
        void ft_init_cgi_var();
        void ft_resolve_full_path_1();
        std::string ft_resolve_full_path_2();
        int ft_handle_path(std::string t, int f, struct stat &info,int &fd);


    public:
        Execution(Client& client);
        void ft_handle_resource(Client& client, Config& conf);
};

template <typename T>
std::string ft_to_string(T value)
{
    std::ostringstream ss;
    ss << value;
    return ss.str();
}

// later : remove .back()
// later : ft_resolve_full_path()
// later : encodeing space -> %20 ,ft_handle_G_autoindex()
// later : url encoding , ft_handle_Redirect_2()

