#include "../../inc/Execution/Execution.hpp"


/* **********************************************/
/*                                              */
/*                  helpers :                   */
/*                                              */
/* **********************************************/

static std::string	ft_to_upper(const std::string& str)
{
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

static int	ft_strtoi(const std::string& value)
{
	std::stringstream	s;
	int					nm;
	char				extra;

	s << value;
	if (!(s >> nm))
        return -1;
    if (s >> extra)
        return -1;
	
    return nm;
}

static std::string	ft_to_lower_2(const std::string& str)
{
	std::string	result;

	result = str;
    for (size_t i = 0; i < result.size(); i++)
    {
        if (result[i] >= 'A' && result[i] <= 'Z')
            result[i] = result[i] + ('a' - 'A');
    }
	return result;
}

static std::string ft_trim_2(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t");

    if (first == std::string::npos)
        return "";

    size_t last = str.find_last_not_of(" \t");

    return str.substr(first, last - first + 1);
}

static std::vector<std::string> ft_split_2(const std::string& str, const std::string& del)
{
	std::vector<std::string>	res;
	size_t						start;
	size_t						pos;

	if (del.empty()){
		res.push_back(str);
		return res;
	}
	start = 0;
	while (true){
		pos = str.find(del, start);
		if (pos == std::string::npos){
			res.push_back(str.substr(start));
			break;
		}
		res.push_back(str.substr(start, pos - start));
		start = pos + del.length();
	}
	return res;
}

static bool    is_token_char_2(char c)
{
    if ((c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9'))
        return true;

    if (c == '!' || c == '#' || c == '$' || c == '%'
		||c == '&' || c == '\'' || c == '*' || c == '+'
		|| c == '-' || c == '.' || c == '^' || c == '_'
		|| c == '`' || c == '|' || c == '~')
	    return true;

    return false;
}

static bool	valid_header_name_2(const std::string& name)
{
	if (name.empty())
		return false;
	for (size_t a = 0; a < name.size(); a++){
		if (!is_token_char_2(name[a]))
			return false;
	}
	return true;
}

static bool	valid_header_value_2(const std::string& value)
{
	unsigned char	c;
	for (size_t a = 0; a < value.size(); a++){
		c = static_cast<unsigned char>(value[a]);
		if (c < 32 && c != '\t')
			return false;
		if (c == 127)
			return false;
	}
	return true;
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

    if(ft_cgi_build_env(client)==-1)
    {
        ft_handle_error(client, 500);
        return;
    }

    if(ft_cgi_build_argv()==-1)
    {
        ft_handle_error(client, 500);
        return;
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
        std::exit(127);
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
    if (full_path.find('\0') != std::string::npos)
        return -1;

    std::vector<std::string> tmp_head=ft_split(full_path,'/');
    
    cgi_req_dir.clear();
    cgi_req_file.clear();

    for(size_t i=0; i<tmp_head.size();i++)
    {
        if(i==tmp_head.size()-1)
            cgi_req_file=tmp_head[i];
        else
        {
            cgi_req_dir+=tmp_head[i];
            cgi_req_dir+="/";
        }
    }
    if(cgi_req_dir.empty())
        cgi_req_dir=".";

    if(cgi_req_file.empty() || cgi_req_file=="." || cgi_req_file=="..")
        return -1;
    
    return 0;
}
int Execution::ft_cgi_handle_path()
{
    struct stat info;

    if(cgi_path.empty() || cgi_path[0]!='/' || cgi_path.find('\0') != std::string::npos)
        return -1;
    
    if(stat(cgi_path.c_str(), &info)==-1 || !S_ISREG(info.st_mode))
        return -1;
    
    if(access(cgi_path.c_str(), X_OK)==-1)
        return -1;
    
    return 0;
}

int Execution::ft_cgi_build_env(Client& client)
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

    if (req.host.empty())
        cgi_env_1.push_back("SERVER_NAME=" + s_conf._host);
    else
        cgi_env_1.push_back("SERVER_NAME=" + req.host);

    cgi_env_1.push_back("SERVER_PORT=" + ft_to_string(req.localPort));
    cgi_env_1.push_back("SERVER_PROTOCOL=" + req.version);
    cgi_env_1.push_back("SERVER_SOFTWARE=webserv/1.0");
    cgi_env_1.push_back("REQUEST_URI=" + req.target);
    cgi_env_1.push_back("PATH_INFO="+req.path);
    cgi_env_1.push_back("REMOTE_ADDR=" + client.client_ip);
    cgi_env_1.push_back("PATH=/usr/bin:/bin");


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
            std::string name = ft_to_lower_2(ft_trim(tmp[i]));

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
    if (cgi_path.empty() || cgi_req_file.empty() || cgi_req_file.find('\0') != std::string::npos)
        return -1;

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

        p_in[0] = -1;
        p_in[1] = -1;

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

    if(dup2(p_in[0], STDIN_FILENO) == -1)
    {
        close(p_in[0]);
        close(p_out[1]);
        std::exit(127);
    }

    if(p_in[0] != STDIN_FILENO)
        close(p_in[0]);

    if(dup2(p_out[1], STDOUT_FILENO) == -1)
    {
        close(p_out[1]);
        std::exit(127);
    }

    if(p_out[1] != STDOUT_FILENO)
        close(p_out[1]);

    if(chdir(cgi_req_dir.c_str()) == -1)
        std::exit(127);

    execve(cgi_path.c_str(), &argv_env_2[0], &cgi_env_2[0]);

    std::exit(126);
}


/* **********************************************/
/*                                              */
/*                Cgi result :                  */
/*                                              */
/* **********************************************/

static std::string ft_msg(int s)
{
    switch (s)
    {
        case 400: return " Bad Request";
        case 401: return " Unauthorized";
        case 403: return " Forbidden";
        case 404: return " Not Found";
        case 405: return " Method Not Allowed";
        case 408: return " Request Timeout";
        case 409: return " Conflict";
        case 411: return " Length Required";
        case 413: return " Payload Too Large";
        case 414: return " URI Too Long";
        case 415: return " Unsupported Media Type";
        case 417: return " Expectation Failed";
        case 431: return " Request Header Fields Too Large";

        case 500: return " Internal Server Error";
        case 501: return " Not Implemented";
        case 502: return " Bad Gateway";
        case 503: return " Service Unavailable";
        case 504: return " Gateway Timeout";
        case 505: return " HTTP Version Not Supported";

        default:  return " Unknown Error";
    }
}

void ft_handle_error_3(Client& client, int s)
{
    if(s < 400 || s > 599)
        s = 500;

    client.rsp.status_code=s;
    client.rsp.headers.clear();
    client.rsp.set_cookies.clear();
    std::string().swap(client.cgi_rsp.raw_output); //new

    client.rsp.headers["Content-Type"] = "text/html";

    std::stringstream ss;
    ss << "<html><body><h1>"
        << client.rsp.status_code
        << ft_msg(client.rsp.status_code)
        << "</h1></body></html>";

    if(client.rsp.body_fd!=-1)
        close(client.rsp.body_fd);
    client.rsp.body_fd=-1;

    client.rsp.body=ss.str();
    client.rsp.body_size=client.rsp.body.size();

    client.rsp.headers["Content-Length"]=ft_to_string(client.rsp.body_size);

    client.rsp.responc.clear();
    return;
}

void ft_cgi_finish(Client& client)
{
    if (!client.cgi_rsp.stdout_eof || !client.cgi_rsp.child_reaped)
        return;

    if (client.cgi_rsp.child_exit_status != 0 ||
        client.cgi_rsp.raw_output.empty() || 
        client.cgi_rsp.raw_output.size() > MAX_CGI_OUTPUT)
    {
        ft_handle_error_3(client, 502);
        return;
    }

    std::string tmp_heads;
    std::string tmp_body;
    std::string tmp_del;

    size_t ps;
    size_t crlf = client.cgi_rsp.raw_output.find("\r\n\r\n");
    size_t lf = client.cgi_rsp.raw_output.find("\n\n");

    if (crlf == std::string::npos &&
        lf == std::string::npos)
    {
        ft_handle_error_3(client, 502);
        return;
    }

    if (crlf < lf)
    {
        ps = crlf;
        tmp_del = "\r\n"; //select the delimeter
    }
    else
    {
        ps = lf;
        tmp_del = "\n";
    }


    if (ps > MAX_CGI_HEADERS)
    {
        ft_handle_error_3(client, 502);
        return;
    }
    tmp_heads = client.cgi_rsp.raw_output.substr(0, ps); //split output
    tmp_body = client.cgi_rsp.raw_output.substr(ps + tmp_del.size() * 2);


    std::map<std::string, std::string> tmp_headers;
    int tmp_status = 200;
    
    bool f_has_status = false;
    bool f_has_location = false;
    bool f_has_content_type = false;


    std::vector<std::string> tmp_1 = ft_split_2(tmp_heads, tmp_del);
    for (size_t i = 0; i < tmp_1.size(); i++)
    {
        ps = tmp_1[i].find(':');

        if (ps == std::string::npos)
        {
            ft_handle_error_3(client, 502);
            return;
        }

        std::string tmp_2_key = tmp_1[i].substr(0, ps);
        std::string tmp_3_val = tmp_1[i].substr(ps + 1);
        
        if (!valid_header_name_2(tmp_2_key) ||
            !valid_header_value_2(tmp_3_val))
        {
            ft_handle_error_3(client, 502);
            return;
        }

        std::string key = ft_to_lower_2(tmp_2_key);
        tmp_3_val = ft_trim_2(tmp_3_val);


        if (key == "status")
        {
            if (f_has_status)
            {
                ft_handle_error_3(client, 502);
                return;
            }

            f_has_status = true;

            if (tmp_3_val.size() < 3 ||
                    tmp_3_val.substr(0, 3).find_first_not_of("0123456789") != std::string::npos ||
                    (tmp_3_val.size() > 3 && tmp_3_val[3] != ' '))
                {
                    ft_handle_error_3(client, 502);
                    return;
                }

            int code = ft_strtoi(tmp_3_val.substr(0, 3));

            if (code < 200 || code > 599)
            {
                ft_handle_error_3(client, 502);
                return;
            }

            tmp_status = code;
        }
        else if (key == "location")
        {
            if (f_has_location || tmp_3_val.empty() ||
                tmp_3_val.find_first_of(" \t") != std::string::npos)
            {
                ft_handle_error_3(client, 502);
                return;
            }

            f_has_location = true;
            tmp_headers["location"] = tmp_3_val;
        }
        else if (key == "content-type")
        {
            if (f_has_content_type || tmp_3_val.empty())
            {
                ft_handle_error_3(client, 502);
                return;
            }

            f_has_content_type = true;
            tmp_headers["content-type"] = tmp_3_val;
        }
        else if(key=="set-cookie")
        {
            client.rsp.set_cookies.push_back(tmp_3_val);
        }
        else if (key == "transfer-encoding")
        {
            ft_handle_error_3(client, 502);
            return;
        }
        else if (
            key == "content-length" ||
            key == "connection" || //later
            key == "keep-alive" ||
            key == "te" ||
            key == "trailer" ||
            key == "upgrade" ||
            key == "proxy-connection"
        )
        {
            continue;
        }
        else
        {
            tmp_headers[key] = tmp_3_val;
        }
    }

    // 4. Handle Location without explicit Status
    if (f_has_location && !f_has_status)
    {
        std::string location = ft_to_lower_2(tmp_headers["location"]);

        if (location.compare(0, 7, "http://") == 0 ||
            location.compare(0, 8, "https://") == 0)
        {
            if (!tmp_body.empty())
            {
                ft_handle_error_3(client, 502);
                return;
            }

            tmp_status = 302;
        }
        else
        {
            ft_handle_error_3(client, 502);
            return;
        }
    }

    bool bodyless_status = (tmp_status == 204 || tmp_status == 205 || tmp_status == 304);

    bool is_redirect = (f_has_location && tmp_status >= 300 && tmp_status < 400);

    if ((!tmp_body.empty() && (bodyless_status || !f_has_content_type)) ||
        (!f_has_content_type && !bodyless_status && !is_redirect))
    {
        ft_handle_error_3(client, 502);
        return;
    }

    if (client.rsp.body_fd != -1)
        close(client.rsp.body_fd);
    client.rsp.body_fd = -1;

    client.rsp.status_code = tmp_status;

    client.rsp.headers.swap(tmp_headers);
    client.rsp.body.swap(tmp_body);
    std::string().swap(client.cgi_rsp.raw_output);

    client.rsp.body_size = client.rsp.body.size();

    client.rsp.responc.clear();
}