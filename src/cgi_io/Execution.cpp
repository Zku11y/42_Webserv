#include "../../inc/core/server.hpp"
#include "../../inc/cgi_io/Execution.hpp"

/* **********************************************/
/*                                              */
/*               Constructor :                  */
/*                                              */
/* **********************************************/

Execution::Execution(Client& client):
        status(0),
        req(client.parsed_req),
        cgi_exc(false),
        cgi_path("")
{    

    if(client.rsp.body_fd!=-1)
        close(client.rsp.body_fd);
    client.rsp.body_fd=-1;

    client.rsp.status_code=status;
    client.rsp.body.clear();
    client.rsp.headers.clear();
    client.rsp.body_size=0;
}


/* **********************************************/
/*                                              */
/*                    Main :                    */
/*                                              */
/* **********************************************/

void Execution::ft_handle_resource(Client& client, Config& conf)
{
    if(ft_select_server(conf)==-1)
    {
        ft_handle_error_2(client, 500);
        return;
    }

    if(ft_validate_path()==-1)
    {
        ft_handle_error(client,status);
        return;
    }

    if(ft_select_location()==-1)
    {
        ft_handle_error(client,status);
        return;
    }

    if(ft_validate_methods()==-1)
    {
        ft_handle_error(client,status);
        return;
    }

    {
        ft_resolve_full_path_1();
        ft_init_cgi_var();
    }
    

    if(s_location._returnCode!=0)
    {
        ft_handle_Redirect(client);
    }
    else if(req.method=="GET")
    {
        ft_handle_Get(client);
    }
    else if(req.method=="POST")
    {
        ft_handle_Post(client);
    }
    else if(req.method=="DELETE")
    {
        ft_handle_Delete(client);
    }
    else
    {
        ft_handle_error(client,500);
    }

    return;
}



/* **********************************************/
/*                                              */
/*                    Init :                    */
/*                                              */
/* **********************************************/

int Execution::ft_select_server(Config& conf)
{
    int f=-1;
    size_t i=0;

    while(i<conf._servers.size())
    { 
        if(conf._servers[i]._port==req.localPort )
        {
            if(conf._servers[i]._host==req.localHost)
            {
                s_conf=conf._servers[i];
                return 0;
            }
            if(conf._servers[i]._host=="0.0.0.0" && f==-1)
                f = i;         
        }
        i++;
    }

    if(f != -1)
    {
        s_conf=conf._servers[f];
        return 0;
    }

    return -1;
}

int Execution::ft_validate_path()
{
    if(req.path.empty()||req.path[0]!='/')
    {
        status=400;
        return -1;
    }
    for(size_t i=0; i<req.path.size() ; i++)
    {
        if(req.path[i]=='\0')
        {
            status=400;
            return -1;
        }
    }

    std::vector<std::string> tmp_1=ft_split(req.path, '/');
    std::vector<std::string> tmp_2 ;

    bool ff=(tmp_1.back().empty() || tmp_1.back()=="." || tmp_1.back()=="..");

    for(size_t i=0;i<tmp_1.size();i++)
    {
        if(tmp_1[i].empty() || tmp_1[i]==".")
            continue;
        else if(tmp_1[i]=="..")
        {
            if(tmp_2.empty())
            {
                status=403;
                return -1;
            }
            else 
                tmp_2.pop_back();
        }
        else
            tmp_2.push_back(tmp_1[i]);
    }

    req.path.clear();
    for(size_t i=0;i<tmp_2.size();i++)
    {
        req.path += "/";
        req.path += tmp_2[i];
    }

    if(tmp_2.empty() || ff)
    {
        req.path+="/";
    }

    return 0;
}

int Execution::ft_select_location()
{
    std::string tmp;
    std::string tmp_path;

    for(size_t i=0;i<s_conf._locations.size(); i++)
    {
        std::string P_location = s_conf._locations[i]._path;
        size_t x=0;
        size_t y=0;

        while( x < P_location.size() 
            && y < req.path.size() 
            && P_location[x] == req.path[y])
        {
            x++;y++;
        }

        if(x==P_location.size())
        {
            if( y==req.path.size()
                || P_location[P_location.size()-1]=='/'
                || req.path[y]=='/')
            {
                tmp_path=P_location;
            }

            if (tmp_path.size() > tmp.size())// fix the edge case whre it dont update if it match / first that it match /xxxxx caus they have same numberof slashs
            {
                tmp = tmp_path;
            }
        }
        else
            continue;

        tmp_path="";
    }
    if(s_conf._locations.empty() || tmp=="")
    {
        status=404;
        return -1;
    }

    for(size_t i=0;i<s_conf._locations.size(); i++)
    {
        if(tmp==s_conf._locations[i]._path)
            s_location=s_conf._locations[i];
    }

    return 0;
}

int Execution::ft_validate_methods()
{
    if(req.method!="GET" && req.method!="POST" && req.method!="DELETE")
    {
        status = 501;
        return -1;
    }

    size_t i = 0;
    while(i<s_location._methods.size())
    {
        if(req.method==s_location._methods[i])
            break;
        i++;
    }
    if(i==s_location._methods.size())
    {
        status = 405;
        return -1;
    }

    return 0;
}

std::string Execution::ft_resolve_full_path_2()
{
    return s_location._uploadStore + req.path;
}

void Execution::ft_resolve_full_path_1()
{
    full_path=s_location._root + req.path;  

    return;
}

void Execution::ft_init_cgi_var()
{
    cgi_exc = false;
    cgi_path.clear();

    if(!s_location._cgi.empty())
    {
        std::vector<std::string> tmp=ft_split(full_path, '.');

        if (tmp.size() < 2)
            return;

        std::map<std::string, std::string>::iterator itr= s_location._cgi.find("."+tmp.back());

        if(itr!=s_location._cgi.end())
        {
            cgi_exc = true;
            cgi_path = itr->second;
        }
    }

    return;
}

int Execution::ft_handle_path(std::string t, int f, struct stat &info,int &fd)
{
    std::vector<std::string> tmp_1=ft_split(t, '/');
    std::string tmp_2;

    if(t.empty())
    {
        status=500;
        return -1;
    }

    for(size_t i=0; i<tmp_1.size();i++)
    {
        if(i==0 && tmp_1[i].empty())
        {
            tmp_2="/";
        }
        else if(i==0 && !tmp_1[i].empty())
        {
            tmp_2=tmp_1[i];
        }
        else
        {
            if (tmp_2[tmp_2.size() - 1] != '/')
                tmp_2 += "/";
            tmp_2 += tmp_1[i];
        }
            

        if(i==tmp_1.size()-1) // last file
        {  
            if(f==1) //post case , we dont care about last file exsistance
                return 0;

            if (stat(tmp_2.c_str(), &info) == -1)
            {
                if (errno == ENOENT || errno == ENOTDIR)
                    status = 404;
                else if (errno == EACCES || errno == EPERM || errno == ELOOP)
                    status = 403;
                else
                    status = 500;

                return -1;
            }
            if (!S_ISREG(info.st_mode) && !S_ISDIR(info.st_mode))
            {
                status=403;
                return -1;
            }
            
            if(f==2) //delet case , we dont care about last file permision
                return 0;

            fd=open(tmp_2.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
        }
        else
        {
            fd=open(tmp_2.c_str(), O_RDONLY | O_NOFOLLOW |  O_NONBLOCK | O_DIRECTORY);
        }

        if(fd==-1)
        {
            if (errno == ENOENT || errno == ENOTDIR)
                status = 404;
            else if (errno == EACCES || errno == EPERM || errno == ELOOP)
                status = 403;
            else
                status = 500;

            return -1;
        }
        else if(i!=tmp_1.size()-1)
        {
            close(fd);
            fd=-1;
        }
    }

    return 0;
}

/* **********************************************/
/*                                              */
/*             ft_handle_Get :                  */
/*                                              */
/* **********************************************/

void Execution::ft_handle_G_autoindex(Client& client)
{
    DIR *t=opendir(full_path.c_str());
    if(!t)
    {
        if (errno == ENOENT || errno == ENOTDIR || errno == ELOOP)
            status = 404;
        else if (errno == EACCES || errno == EPERM)
            status = 403;
        else
            status = 500;

        ft_handle_error(client, status);
        return;
    }


    struct dirent *t2;
    std::ostringstream ss;

    ss << "<html> <body>"    
        << "<h1> Index of " << req.path << " </h1>"
        << "<ul>";

    while (1)
    {
        errno=0;
        t2 = readdir(t);
        if(!t2)
        {
            if(errno!=0)
            {
                closedir(t);
                ft_handle_error(client, 500);
                return;
            }
            break;
        }

        std::string name = t2->d_name;

        if (name == "." || name == "..")
            continue;
            
        ss << "<li>"   //later : encodeing space -> %20
            << "<a href=\"" << req.path + name << "\">"
            << name << "</a>"
            << "</li>";
    }
    
    ss <<"</ul>"
        <<"</body> </html>";
    
    closedir(t);

    client.rsp.body=ss.str();

    if (client.rsp.body_fd != -1)
        close(client.rsp.body_fd);
    client.rsp.body_fd=-1;

    client.rsp.status_code=200;

    client.rsp.body_size=client.rsp.body.size();

    client.rsp.headers["Content-Type"] = "text/html";
    client.rsp.headers["Content-Length"]=ft_to_string(client.rsp.body_size);

    return;
}

void Execution::ft_handle_G_files(Client& client, struct stat& info, int& fd)
{
    if(client.rsp.body_fd!=-1)
        close(client.rsp.body_fd);
    client.rsp.body_fd=fd;
    fd=-1;
    
    client.rsp.status_code=200;
    
    client.rsp.body="";
    client.rsp.body_size=info.st_size;

    client.rsp.headers["Content-Type"] = ft_mime_types();
    client.rsp.headers["Content-Length"] = ft_to_string(client.rsp.body_size);
}


void Execution::ft_handle_G_dir(Client& client, int& fd)
{    
    if(req.path[req.path.size()-1]!='/')
    {
        close(fd); fd = -1;
        ft_handle_Redirect_2(client);
        return;
    }
    
    if(!s_location._index.empty())
    {
        int fd_2=-1;
        struct stat info_2;

        std::string tmp_index_path = full_path;
        
        if(full_path[full_path.size()-1]!='/')
            tmp_index_path+="/";
        tmp_index_path+= s_location._index;//fix the bug of missing index file

        if(ft_handle_path(tmp_index_path, 0 ,info_2, fd_2)==-1)
        {
            if(status != 404)
            {
                close(fd); fd = -1;
                ft_handle_error(client, status);
                return;
            }
            status=0;
        }
        else
        {
            if(S_ISREG(info_2.st_mode))
            {
                full_path=tmp_index_path;
                ft_init_cgi_var();

                if(cgi_exc)
                {
                    close(fd); fd = -1;
                    close(fd_2); fd_2 = -1;
                    // ft_handle_cgi()
                    return;
                }
                else
                {
                    close(fd); fd = -1;
                    ft_handle_G_files(client, info_2 ,fd_2);
                    return ;
                }
            }
            else
            {
                close(fd_2); fd_2 = -1;
            }
        }
    }
    
    close(fd); fd = -1;
    if(s_location._autoindex)
    {
        ft_handle_G_autoindex(client);
    }
    else
    {
        ft_handle_error(client, 403);
    }
    return;
}


void Execution::ft_handle_Get(Client& client)
{
    int fd = -1;
    struct stat info;

    if(ft_handle_path(full_path, 0, info, fd)==-1)
    {
        ft_handle_error(client,status);
        return;
    }
    
    if(S_ISREG(info.st_mode))
    {
        if(cgi_exc)
        {
            close(fd); fd = -1;
            // ft_handle_cgi(client, info);
        }
        else
        {
            ft_handle_G_files(client, info, fd);
        }
    }
    else if(S_ISDIR(info.st_mode))
    {
        ft_handle_G_dir(client, fd);
    }
    else
    {
        close(fd); fd = -1;
        ft_handle_error(client, 403);
    }

    return ;
}


/* **********************************************/
/*                                              */
/*             ft_handle_Post :                 */
/*                                              */
/* **********************************************/

void Execution::ft_handle_Post(Client& client)
{
    int fd = -1;
    struct stat info;

    if(cgi_exc)
    {
        if(ft_handle_path(full_path, 0, info, fd)==-1)
        {
            ft_handle_error(client, status);
            return;
        }
        if(!S_ISREG(info.st_mode))
        {
            close(fd); fd=-1;
            ft_handle_error(client, 403);
            return;
        }
        close(fd); fd=-1;
        ft_handle_cgi();
        return;
    }

    if(!s_location._uploadEnable)
    {
        ft_handle_error(client, 403);
        return;
    }

    std::map<std::string, std::string>::iterator itr=req.headers.find("content-type");
    if(itr!=req.headers.end())
    {   
        std::vector<std::string> tmp_1 = ft_split(itr->second, ';');
        std::string tmp_2 = ft_to_lower(ft_trim(tmp_1[0]));

        if(tmp_2 == "multipart/form-data")
        {
            ft_handle_error(client, 415);
            return;
        }
    }

    if(req.path[req.path.size()-1]=='/'  || req.path == s_location._path )
    {
        ft_handle_error(client, 400);
        return;
    }

    std::string full_uploade_path=ft_resolve_full_path_2();

    if(ft_handle_path(full_uploade_path, 1, info, fd)==-1)
    {
        ft_handle_error(client, status);
        return;
    }

    fd=open(full_uploade_path.c_str(),O_WRONLY | O_CREAT | O_EXCL, 0644);
    if(fd==-1)
    {
        if (errno == EEXIST)
            status = 409;
        else if(errno==ENOENT || errno==ENOTDIR)
            status = 404;
        else if (errno == EACCES || errno == EPERM 
                || errno == EROFS || errno == ELOOP)
            status = 403;
        else
            status = 500;

        ft_handle_error(client, status);
        return ;
    }

    size_t count=0;
    while(count<req.body.size())
    {
        ssize_t n=write(fd, req.body.data()+count,req.body.size()-count);
        if(n<=0)
        {
            close(fd);
            fd = -1;
            if(std::remove(full_uploade_path.c_str()) != 0)
                std::cerr << "Upload cleanup failed" << std::endl;
            ft_handle_error(client, 500);
            return;
        }
        count+=n;
    }

    if (close(fd) == -1)
    {
        fd=-1;
        if(std::remove(full_uploade_path.c_str()) != 0)
            std::cerr << "Upload cleanup failed" << std::endl;
        ft_handle_error(client, 500);
        return;
    }

    client.rsp.status_code=201;

    if (client.rsp.body_fd != -1)
        close(client.rsp.body_fd);
    client.rsp.body_fd=-1;
    fd=-1;

    client.rsp.body.clear();
    client.rsp.body_size=0;

    client.rsp.headers.clear();
    client.rsp.headers["Location"] = req.path;
    client.rsp.headers["Content-Length"] = "0";
}


// /* **********************************************/
// /*                                              */
// /*           ft_handle_Delete :                 */
// /*                                              */
// /* **********************************************/

void Execution::ft_handle_Delete(Client& client)
{
    int fd = -1;
    struct stat info;

    if(ft_handle_path(full_path, 2, info, fd)==-1)
    {
        ft_handle_error(client, status);
        return;
    }

    if (!S_ISREG(info.st_mode))
    {
        ft_handle_error(client, 403);
        return;
    }

    if (std::remove(full_path.c_str()) != 0)
    {
        if (errno == ENOENT || errno == ENOTDIR)
            status = 404;
        else if (errno == EACCES || errno == EPERM 
                || errno == EROFS || errno == ELOOP)
            status = 403;
        else
            status = 500;

    
        ft_handle_error(client, status);
        return;
    }

    client.rsp.status_code = 204;

    if (client.rsp.body_fd != -1)
        close(client.rsp.body_fd);
    client.rsp.body_fd = -1;

    client.rsp.body.clear();
    client.rsp.headers.clear();
    
    client.rsp.body_size = 0;

    return;
}


/* **********************************************/
/*                                              */
/*                   Errors :                   */
/*                                              */
/* **********************************************/

int Execution::ft_error_1(Client& client)
{
    int fd = -1;
    struct stat info;

    int tmp_status=status; //bug of status change if custome erroe page path is not woeking

    std::map<int, std::string>::iterator itr=s_conf._errorPages.find(status);

    if(itr!=s_conf._errorPages.end())
    {
        if(ft_handle_path(itr->second, 0, info, fd) ==-1)
        {
            status=tmp_status;
            return 0;
        }
        status=tmp_status;
        if(S_ISREG(info.st_mode))
        {
            if(client.rsp.body_fd!=-1)
                close(client.rsp.body_fd);
            client.rsp.body_fd=fd;
            fd = -1;
            client.rsp.body.clear();
            client.rsp.body_size=info.st_size;
            client.rsp.headers["Content-Length"]=ft_to_string(info.st_size);

            return 1;
        }
        close(fd);fd=-1;
    }
    return 0;
}

void Execution::ft_error_2(Client& client)
{
    std::stringstream ss;
    ss << "<html><body><h1>"
       << client.rsp.status_code
       << "Error" // change it later to custome msg
       << "</h1></body></html>";

    if(client.rsp.body_fd!=-1)
        close(client.rsp.body_fd);
    client.rsp.body_fd=-1;

    client.rsp.body=ss.str();
    client.rsp.body_size=client.rsp.body.size();

    client.rsp.headers["Content-Length"]=ft_to_string(client.rsp.body_size);

    return;
}

void Execution::ft_handle_error(Client& client, int s)
{
    status=s;
    client.rsp.status_code=s;

    client.rsp.headers.clear();
    client.rsp.headers["Content-Type"] = "text/html";
    if(status==405)
    {
        std::string tmp;
        for(size_t i=0; i< s_location._methods.size();i++)
        {
            tmp+=s_location._methods[i];
            if(i<s_location._methods.size()-1)
                tmp+=", ";
        }
        client.rsp.headers["Allow"] = tmp;
    }

    if(ft_error_1(client))
        return ;
    ft_error_2(client);
}

void Execution::ft_handle_error_2(Client& client, int s)
{
    status=s;
    client.rsp.status_code=s;
    client.rsp.headers.clear();
    client.rsp.headers["Content-Type"] = "text/html";

    ft_error_2(client);
}


/* **********************************************/
/*                                              */
/*                 Redirect :                   */
/*                                              */
/* **********************************************/

void Execution::ft_handle_Redirect_2(Client& client)
{
    std::string new_target = req.path+"/"; // later : url encoding

    if(!req.queryString.empty())
    {
        new_target += "?" ;
        new_target += req.queryString;
    }

    client.rsp.headers["Location"] = new_target;
    client.rsp.headers["Content-Length"]="0";
    
    client.rsp.status_code=301;

    if(client.rsp.body_fd!=-1)
        close(client.rsp.body_fd);
    client.rsp.body_fd=-1;

    client.rsp.body="";
    client.rsp.body_size=0;
    
    return;
}

void Execution::ft_handle_Redirect(Client& client)
{
    client.rsp.status_code=s_location._returnCode;

    client.rsp.headers["Location"] = s_location._returnTarget;
    client.rsp.headers["Content-Length"]="0";

    if(client.rsp.body_fd!=-1)
        close(client.rsp.body_fd);
    client.rsp.body_fd=-1;

    client.rsp.body="";
    client.rsp.body_size=0;
    
    return;
}



/* **********************************************/
/*                                              */
/*                  Cgi part :                  */
/*                                              */
/* **********************************************/

void Execution::ft_handle_cgi()
{


    
    return;
}


/* **********************************************/
/*                                              */
/*                  helpers :                   */
/*                                              */
/* **********************************************/

std::vector<std::string>	Execution::ft_split(const std::string& str, char del) const{
	std::vector<std::string>	res;
	size_t						start;
	size_t						pos;
	
	start = 0;
	while (true){
		pos = str.find(del, start);
		if (pos == std::string::npos){
			res.push_back(str.substr(start));
			break;
		}
		res.push_back(str.substr(start, pos - start));
		start = pos + 1;
	}
	return res;
}

std::string Execution::ft_trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos)
            return "" ;

    size_t last = str.find_last_not_of(" \t\n\r\f\v");

    return str.substr(first, (last - first + 1));
}


std::string	Execution::ft_to_lower(const std::string& str) const{
	std::string	result;

	result = str;
    for (size_t i = 0; i < result.size(); i++)
    {
        if (result[i] >= 'A' && result[i] <= 'Z')
            result[i] = result[i] + ('a' - 'A');
    }
	return result;
}

std::string Execution::ft_mime_types()
{
    std::vector<std::string> tmp_1=ft_split(full_path,'/');
    std::vector<std::string> tmp_2=ft_split(tmp_1.back(), '.');
    
    if(tmp_2.size()<2)
        return "application/octet-stream";

    std::string ext=ft_to_lower(tmp_2.back());

    if (ext == "html" || ext == "htm")
        return "text/html";
    else if (ext == "css")
        return "text/css";
    else if (ext == "js")
        return "text/javascript";
    else if (ext == "txt")
        return "text/plain";
    else if (ext == "json")
        return "application/json";
    else if (ext == "gltf")
        return "model/gltf+json";
    else if (ext == "ttf")
        return "font/ttf";
    else if (ext == "png")
        return "image/png";
    else if (ext == "jpg" || ext == "jpeg")
        return "image/jpeg";
    else if (ext == "gif")
        return "image/gif";
    else if (ext == "pdf")
        return "application/pdf";
    else if (ext == "mp4")
        return "video/mp4";
    else if (ext == "mp3")
        return "audio/mpeg";

    return "application/octet-stream";
}
