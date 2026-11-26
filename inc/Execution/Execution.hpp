#pragma once

#include "../main.hpp"
#include "../core/server.hpp"

#define MAX_CGI_HEADERS (16 * 1024)
#define MAX_CGI_OUTPUT  (20 * 1024 * 1024)
class Execution
{
    friend int connection_handler(Config &cfg);
    friend void ft_cgi_finish(Client& client);

    private:
        int status;

        HttpRequest req;

        ServerConfig s_conf;
        LocationConfig s_location;

        std::string full_path;

    private:
        bool cgi_exc;
        std::string cgi_path;

        std::string cgi_req_path;
        std::string cgi_req_dir;
        std::string cgi_req_file;

        std::vector<std::string> cgi_env_1;
        std::vector<char*> cgi_env_2;

        std::vector<std::string> argv_env_1;
        std::vector<char*> argv_env_2;
        
        int p_in[2];
        int p_out[2];

        int fork_fd;

    private:
            // init :
        int ft_select_server(Config& conf);
        int ft_validate_path();
        int ft_select_location();
        int ft_validate_methods();
        int ft_validate_root(std::string path);
        void ft_init_cgi_var();
        void ft_resolve_full_path_1();
        std::string ft_resolve_full_path_2();
        int ft_handle_path(std::string t, int f, struct stat &info,int &fd);

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

            //  Errors :
        int ft_error_1(Client& client);
        void ft_error_2(Client& client);
        void ft_handle_error(Client& client, int s);
        void ft_handle_error_2(Client& client, int s);

            //  Others :
        std::vector<std::string> ft_split(const std::string& str, char del) const;
        std::string ft_to_lower(const std::string& str) const;
        std::string ft_trim(const std::string& str);
        std::string ft_fix_slashs(std::string tmp);
        std::string ft_url_encode(std::string tmp);
        std::string ft_html_escape(std::string tmp);

            //  Cgi :
        bool ft_is_cgi();
        void ft_handle_cgi(Client& client);
        int ft_cgi_init();
        int ft_cgi_handle_path();
        int ft_cgi_build_env(Client& client);
        int ft_cgi_build_argv();
        int ft_cgi_build_pipes();
        void ft_cgi_child();
        int ft_cgi_parent(Client& client);

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

void ft_cgi_finish(Client& client);
void ft_handle_error_3(Client& client, int s);