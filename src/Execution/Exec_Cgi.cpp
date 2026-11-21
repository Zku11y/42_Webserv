#include "../../inc/Execution/Execution.hpp"


/* **********************************************/
/*                                              */
/*                  helpers :                   */
/*                                              */
/* **********************************************/


static std::string	ft_to_upper(const std::string& str){
	std::string	result;

	result = str;
    for (size_t i = 0; i < result.size(); i++)
    {
        if (result[i] == '-')
            result[i] = '_';
        else if (result[i] >= 'a' && result[i] <= 'z')
            result[i] = result[i] - ('a' - 'A');
    }
	return "HTTP_"+result;
}



/* **********************************************/
/*                                              */
/*                  Cgi main :                  */
/*                                              */
/* **********************************************/


void Execution::ft_handle_cgi(Client& client)
{
    if(client.status == WAIT)
        return;

    if(ft_cgi_init()==-1)
    {
        ft_handle_error(client, 500);
        return;
    }

    if(ft_cgi_handle_path()==-1)
    {
        ft_handle_error(client, 500);
        return;
    }

    if(ft_cgi_build_env()==-1)
    {
        ft_handle_error(client, 500);
        return;
    }

    {
        ft_cgi_build_argv();
    }

    if(ft_cgi_build_pipes()==-1)
    {
        ft_handle_error(client, 500);
        return;
    }


    fork_fd=fork();
    
    if(fork_fd==-1)
    {
        close(p_in[0]);
        close(p_in[1]);
        close(p_out[0]);
        close(p_out[1]);
        ft_handle_error(client, 500);
        return ;
    }
    else if(fork_fd==0)
    {
        ft_cgi_child();
    }

    close(p_in[0]);
    close(p_out[1]);

    client.cgi_rsp.stdin_write_fd=p_in[1];
    client.cgi_rsp.stdout_read_fd=p_out[0];
    client.cgi_rsp.pid=fork_fd;

    client.status = WAIT;

    return;
}


/* **********************************************/
/*                                              */
/*                  Cgi init :                  */
/*                                              */
/* **********************************************/

int Execution::ft_cgi_init()
{
    std::vector<std::string> tmp_1=ft_split(full_path,'/');
    
    cgi_req_dir.clear();
    cgi_req_file.clear();

    for(size_t i=0; i<tmp_1.size();i++)
    {
        if(i==tmp_1.size()-1)
            cgi_req_file=tmp_1[i];
        else
        {
            cgi_req_dir+=tmp_1[i];
            cgi_req_dir+="/";
        }
    }
    if(cgi_req_dir.empty())
        cgi_req_dir=".";
    if(cgi_req_file.empty())
        return -1;
    
    return 0;
}
int Execution::ft_cgi_handle_path()
{
    struct stat info;

    if(cgi_path.empty() || cgi_path[0]!='/')
        return -1;
    
    if(stat(cgi_path.c_str(), &info)==-1 || !S_ISREG(info.st_mode))
        return -1;
    
    if(access(cgi_path.c_str(), X_OK)==-1)
        return -1;
    
    return 0;
}

int Execution::ft_cgi_build_env()
{
    cgi_env_1.clear();

    cgi_env_1.push_back("GATEWAY_INTERFACE=CGI/1.1");
    cgi_env_1.push_back("REQUEST_METHOD=" + req.method);
    cgi_env_1.push_back("QUERY_STRING=" + req.queryString);
    cgi_env_1.push_back("SCRIPT_NAME=" + cgi_req_path);

    std::map<std::string, std::string>::iterator it;

    it = req.headers.find("content-type");
    if(it != req.headers.end())
        cgi_env_1.push_back("CONTENT_TYPE=" + it->second);

    if(!req.body.empty() ||
        req.headers.find("content-length") != req.headers.end() ||
        req.headers.find("transfer-encoding") != req.headers.end())
    {
        cgi_env_1.push_back("CONTENT_LENGTH=" +
            ft_to_string(req.body.size()));
    }

    cgi_env_1.push_back("SERVER_NAME=" + req.host);
    cgi_env_1.push_back("SERVER_PORT=" + ft_to_string(req.localPort));
    cgi_env_1.push_back("SERVER_PROTOCOL=" + req.version);
    cgi_env_1.push_back("SERVER_SOFTWARE=webserv/1.0");
    cgi_env_1.push_back("REQUEST_URI=" + req.target);
    cgi_env_1.push_back("PATH_INFO=");
    cgi_env_1.push_back("PATH=/usr/bin:/bin");

    // TODO: REMOTE_ADDR (waiting for Dev 1)


    const char* headers_to_skip[] = {
        "content-length",
        "content-type",
        "transfer-encoding",
        "connection",
        "keep-alive",
        "upgrade",
        "te",
        "trailer",
        "proxy",
        "proxy-connection",
        "authorization",
        "proxy-authorization"
    };

    std::vector<std::string> skip(
        headers_to_skip,
        headers_to_skip +
        sizeof(headers_to_skip) / sizeof(headers_to_skip[0])
    );


    it = req.headers.find("connection");

    if(it != req.headers.end())
    {
        std::vector<std::string> tmp = ft_split(it->second, ',');

        for(size_t i = 0; i < tmp.size(); i++)
        {
            std::string name = ft_to_lower(ft_trim(tmp[i]));

            if(!name.empty())
                skip.push_back(name);
        }
    }


    std::map<std::string, std::string>::iterator itr;
    std::vector<std::string> used_names;

    itr = req.headers.begin();

    while(itr != req.headers.end())
    {
        if(std::find(skip.begin(), skip.end(), itr->first) != skip.end())
        {
            itr++;
            continue;
        }

        std::string name = ft_to_upper(itr->first);

        if(name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")
            != std::string::npos || name == "HTTP_PROXY")
        {
            itr++;
            continue;
        }

        if(std::find(used_names.begin(), used_names.end(), name)
            != used_names.end())
        {
            itr++;
            continue;
        }

        used_names.push_back(name);
        cgi_env_1.push_back(name + "=" + itr->second);

        itr++;
    }

    for(size_t i = 0; i < cgi_env_1.size(); i++)
    {
        if(cgi_env_1[i].find('\0') != std::string::npos)
            return -1;
    }

    cgi_env_2.clear();
    for(size_t i = 0; i < cgi_env_1.size(); i++)
    {
        cgi_env_2.push_back(const_cast<char*>(cgi_env_1[i].c_str()));
    }
    cgi_env_2.push_back(NULL);

    return 0;
}


int Execution::ft_cgi_build_argv()
{
    argv_env_1.clear();
    argv_env_1.push_back(cgi_path);
    argv_env_1.push_back("./"+cgi_req_file);

    argv_env_2.clear();
    for(size_t i = 0; i < argv_env_1.size(); i++)
    {
        argv_env_2.push_back(const_cast<char*>(argv_env_1[i].c_str()));
    }
    argv_env_2.push_back(NULL);

    return 0;
}


int Execution::ft_cgi_build_pipes()
{
    if(pipe(p_in)==-1 )
    {
        return -1;
    }
    if(pipe(p_out)==-1 )
    {
        close(p_in[0]);
        close(p_in[1]);
        return -1;
    }

    return 0;
}

/* **********************************************/
/*                                              */
/*               Child process :                */
/*                                              */
/* **********************************************/

void Execution::ft_cgi_child()
{
    close(p_in[1]);
    close(p_out[0]);

    if(dup2(p_in[0],STDIN_FILENO)==-1)
    {
        close(p_in[0]);
        close(p_out[1]);
        std::exit(127);
    }

    if(dup2(p_out[1],STDOUT_FILENO)==-1)
    {
        close(p_in[0]);
        close(p_out[1]);
        std::exit(127);
    }

    if(p_in[0] != STDIN_FILENO)
        close(p_in[0]);

    if(p_out[1] != STDOUT_FILENO)
        close(p_out[1]);

    if(chdir(cgi_req_dir.c_str())==-1)
        std::exit(127);

    execve(cgi_path.c_str(), &argv_env_2[0], &cgi_env_2[0]);
    std::exit(126);
}
