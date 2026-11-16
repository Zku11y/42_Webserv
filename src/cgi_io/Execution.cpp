#include "../../inc/main.hpp"



/* **********************************************/
/*                                              */
/*               Constructor :                  */
/*                                              */
/* **********************************************/



Execution::Execution(Client& client, Config& conf): status(0),req(client.parsed_req),cgi_exc(false),cgi_path("")
{
        // selecting the server :
    int f=0;
    for(size_t i=0; i<conf._servers.size();i++)
    { 
        // check host name later too, not jsut ports
        if(conf._servers[i]._port==req.localPort)
        {
            f=1;
            s_conf=conf._servers[i];
            break;
        }
    }
    if(f==0) //default or an error, that for later dx 
        s_conf=conf._servers[0];
    // if(f==0)
    // {
    //     status=400;
    //     return;
    // }


            // validation of the req path :
    if(req.path.empty()||req.path[0]!='/')
    {
        status=400;
        return;
    }
    for(size_t i=0; i<req.path.size() ; i++)
    {
        if(req.path[i]=='\0')
        {
            status=400;
            return;
        }
    }



            //handle later , req path :

// No unsafe .. traversal	/../../etc/passwd	403
// Valid percent encoding, if not already checked by Dev 2	/test%ZZ	400



            // selecting the match location :
    std::string tmp;
    std::string tmp_path;

    // ft_log("--------- " << s_conf._locations.size() << " ---------\n");
    // ft_log("req_path : " << req.path);
    // ft_log("-------------------------------");

    for(size_t i=0;i<s_conf._locations.size(); i++)
    {
        size_t x=0;
        size_t y=0;

        while(x<s_conf._locations[i]._path.size() && y<req.path.size() && s_conf._locations[i]._path[x]==req.path[y])
        {
            x++;y++;
        }

        if(x==s_conf._locations[i]._path.size())
        {
            if(s_conf._locations[i]._path=="/" && tmp_path.size()<2) // fix the edge case of / if the confige file was not order corctly 
            {
                tmp_path=s_conf._locations[i]._path;
            }
            else if(y==req.path.size() || req.path[y]=='/')
            {
                tmp_path=s_conf._locations[i]._path;
            }

            if (tmp_path.size() > tmp.size())// fix the edge case whre it dont update if it match / first that it match /xxxxx caus they have same numberof slashs
            {
                tmp = tmp_path;
            }
        }
        else
        {
            // ft_log("--------- " << i << " ---------");
            // ft_log("Failed ? : ");
            // ft_log("conf : " << s_conf._locations[i]._path);
            // ft_log("tmp_path : " << tmp_path);
            // ft_log("tmp : " << tmp);
            // ft_log("-------------------------------");
            continue;
        }
        
        // ft_log("--------- " << i << " ---------");
        // ft_log("conf : " << s_conf._locations[i]._path);
        // ft_log("tmp_path : " << tmp_path);
        // ft_log("tmp : " << tmp);
        // ft_log("-------------------------------");

        tmp_path="";
    }
    if(s_conf._locations.empty() || tmp=="")
    {
        status=404;
        return ;
    }


        // ft_log("\n-------------------------------");
        // ft_log("Finale ");
        // ft_log("tmp_path : " << tmp_path);
        // ft_log("tmp : " << tmp);
        // ft_log("-------------------------------");

    for(size_t i=0;i<s_conf._locations.size(); i++)
    {
        if(tmp==s_conf._locations[i]._path)
            s_info=s_conf._locations[i];
    }

    // ft_log(">> "<<s_info._path);
    // ft_log(">> "<<s_info._root);


            //creat the full path :
    if(!s_info._root.empty())
    {
        full_path=s_info._root + req.path;
    }


            // init the cgi variable :
    if(!s_info._cgi.empty())
    {
        size_t pos = full_path.find_last_of('.');
        if (pos != std::string::npos)
        {
            std::string tmp_cgi=full_path.substr(pos);
            size_t pos2 = tmp_cgi.find_first_of('/');
            if (pos2 != std::string::npos)
            {
                tmp_cgi=tmp_cgi.substr(0,pos2);
            }
            std::map<std::string, std::string>::const_iterator it =
                s_info._cgi.find(tmp_cgi);

            if (it != s_info._cgi.end())
            {
                cgi_exc = true;
                cgi_path = it->second;
            }
        }
    }

    //do the req cons:
    
    client.rsp.status_code=status;
    client.rsp.body.clear();
    client.rsp.body_fd=-1;
    client.rsp.body_size=0;
}


/* **********************************************/
/*                                              */
/*                  Resource :                  */
/*                                              */
/* **********************************************/

void Execution::ft_handle_resource(Client& client)
{
    if(status!=0)
    {   
        ft_handle_error(client);
        return;
    }



    if(req.method!="GET" && req.method!="POST" && req.method!="DELETE")
    {
        status = 501;
        ft_handle_error(client);
        return ;
    }



    int f=0;
    for(size_t i = 0 ; i<s_info._methods.size();i++)
    {
        if(req.method==s_info._methods[i])
        {
            f=1;
            break;
        }
    }
    if(!f)
    {
        status = 405;
        ft_handle_error(client);
        return;
    }



    if(s_info._returnCode!=0)
    {
        ft_handle_Redirect(client);
        return;
    }



    if(req.method=="GET")
    {
        ft_handle_Get(client);
    }
    else if(req.method=="POST")
    {
        ft_handle_Post(client);
    }
    else
    {
        ft_handle_Delete(client);
    }

    return;
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
        if (errno == ENOENT || errno == ENOTDIR)
            status = 404;
        else if (errno == EACCES || errno == EPERM)
            status = 403;
        else
            status = 500;
        ft_handle_error(client);
        return;
    }

    struct dirent *t2;
    std::ostringstream ss;

    ss << "<html><body><h1>Index of "
        << req.path
        << "</h1><ul>";

    while ((t2 = readdir(t)) != NULL)
    {
        std::string name = t2->d_name;

        if (name == "." || name == "..")
            continue;
            
        //do later "5. Escape filenames before adding them to HTML"
        ss << "<li><a href=\""
           << full_path << "/" << name
           << "\">"
           <<name
           << "</a></li>";
    }
    ss  <<"</ul> </body> </html>";

    client.rsp.body=ss.str();

    if (client.rsp.body_fd != -1)
        close(client.rsp.body_fd);
    client.rsp.body_fd=-1;

    client.rsp.status_code=200;

    client.rsp.body_size=client.rsp.body.size();

    client.rsp.headers["Content-Type"] = "text/html";
    client.rsp.headers["Content-Length"]=ft_to_string(client.rsp.body_size);

    closedir(t);
    return;
}
    

void Execution::ft_handle_G_files(Client& client, struct stat& info)
{
    int fd=open(full_path.c_str(), O_RDONLY);    
    if(fd==-1)
    {
        if(errno==ENOENT || errno==ENOTDIR)
            status = 404;
        else if (errno == EACCES || errno == EPERM)
            status = 403;
        else
            status = 500;
        ft_handle_error(client);
        return ;
    }

    if(client.rsp.body_fd!=-1)
        close(client.rsp.body_fd);
    client.rsp.body_fd=fd;
    
    client.rsp.status_code=200;
    
    client.rsp.body="";
    client.rsp.body_size=info.st_size;

    // need to redo this later to have all the types included like    .js / .css / .....
    client.rsp.headers["Content-Type"] = "text/html";
    // client.rsp.headers["Content-Type"] = ft_get_type(full_path);

    client.rsp.headers["Content-Length"]=ft_to_string(client.rsp.body_size);
}


void Execution::ft_handle_G_dir(Client& client)
{
    struct stat info;
    
    if(!s_info._index.empty())
    {
        full_path+="/";
        full_path+=s_info._index;

            if(stat(full_path.c_str(), &info)==-1)
            {
                if (errno == EACCES || errno == EPERM)
                    status = 403;
                else
                    status = 500;
                ft_handle_error(client);
            }
            else
            {
                if(S_ISREG(info.st_mode))
                    ft_handle_G_files(client, info);
            }
            
        return;
    }
    
    if(s_info._autoindex)
    {
        ft_handle_G_autoindex(client);
    }
    else
    {
        status=403;
        ft_handle_error(client);
    }
    return;
}


void Execution::ft_handle_Get(Client& client)
{
    // ft_validate_pathe2(); //valisdt safty of path  of root.

    if(cgi_exc)
    {
        // ft_handle_cgi();
        return ;
    }
    
    struct stat info;

    if(stat(full_path.c_str(), &info)==-1)
    {
        if(errno==ENOENT || errno==ENOTDIR)
            status = 404;
        else if (errno == EACCES || errno == EPERM)
            status = 403;
        else
            status = 500;
        ft_handle_error(client);
        return;
    }
    else
    {
        if(S_ISREG(info.st_mode))
        {
            ft_handle_G_files(client, info);
        }
        else if(S_ISDIR(info.st_mode))
        {
            ft_handle_G_dir(client);
        }
        else
        {
            status = 403;
            ft_handle_error(client);
        }
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
    // ft_validate_pathe2(); //valisdt safty of path  of root.

    if(cgi_exc)
    {
        // ft_handle_cgi()
        return;
    }

    if(!s_info._uploadEnable)
    {
        status=403;
        ft_handle_error(client);
        return;
    }

    std::map<std::string, std::string>::iterator itr=req.headers.find("content-type");
    if(itr!=req.headers.end())
    {
        if (itr->second.find("multipart/form-data") == 0)
        {
            status=415;
            ft_handle_error(client);
            return;
        }
    }

    // do later: filnemae probided  or no in req.path
    // do later: chek ifparent die do exist withe stat 

    int fd=open(full_path.c_str(),O_WRONLY | O_CREAT | O_EXCL, 0644);
    if(fd==-1)
    {
        if (errno == EEXIST)
            status = 409;
        else if(errno==ENOENT || errno==ENOTDIR)
            status = 404;
        else if (errno == EACCES || errno == EPERM)
            status = 403;
        else
            status = 500;
        ft_handle_error(client);
        return ;
    }

    size_t count=0;
    while(count<req.body.size())
    {
        ssize_t n=write(fd, req.body.data()+count,req.body.size()-count);
        if(n==-1)
        {
            if (errno == EINTR)
                continue;
            close(fd);
            std::remove(full_path.c_str());
            status=500;
            ft_handle_error(client);
            return;
        }
        if(n==0)
        {
            close(fd);
            std::remove(full_path.c_str());
            status=500;
            ft_handle_error(client);
            return;
        }
        count+=n;
    }

    if (close(fd) == -1)
    {
        status = 500;
        ft_handle_error(client);
        return;
    }

    client.rsp.status_code=201;

    if (client.rsp.body_fd != -1)
        close(client.rsp.body_fd);
    client.rsp.body_fd=-1;

    client.rsp.body.clear();
    client.rsp.body_size=0;

    client.rsp.headers["Location"] = req.path;
    client.rsp.headers["Content-Length"] = "0";
}


/* **********************************************/
/*                                              */
/*           ft_handle_Delete :                 */
/*                                              */
/* **********************************************/

void Execution::ft_handle_Delete(Client& client)
{
    // ft_validate_pathe2(); //valisdt safty of path  of root.

    struct stat info;

    if(stat(full_path.c_str(), &info)==-1)
    {
        if(errno==ENOENT || errno==ENOTDIR)
            status = 404;
        else if (errno == EACCES || errno == EPERM)
            status = 403;
        else
            status = 500;
        ft_handle_error(client);
        return;
    }
    
    if (!S_ISREG(info.st_mode))
    {
        status = 403;
        ft_handle_error(client);
        return;
    }

    if (std::remove(full_path.c_str()) == -1)
    {
        if (errno == ENOENT || errno == ENOTDIR)
            status = 404;
        else if (errno == EACCES || errno == EPERM)
            status = 403;
        else
            status = 500;

        ft_handle_error(client);
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
/*                  Response :                  */
/*                                              */
/* **********************************************/

int Execution::ft_error_1(Client& client)
{
    struct stat info;

    std::map<int, std::string>::iterator itr=s_conf._errorPages.find(status);

    if(itr!=s_conf._errorPages.end())
    {
        if(stat(itr->second.c_str(),&info)==0 && S_ISREG(info.st_mode))
        {
            int fd=open(itr->second.c_str(), O_RDONLY);
            if(fd!=-1)
            {
                if(client.rsp.body_fd!=-1)
                    close(client.rsp.body_fd);
                client.rsp.body_fd=fd;
                client.rsp.body.clear();
                client.rsp.body_size=info.st_size;
                client.rsp.headers["Content-Length"]=ft_to_string(info.st_size);

                return 1;
            }
        }
    }
    return 0;
}

void Execution::ft_error_2(Client& client)
{
    std::stringstream ss;
    ss << "<html><body><h1>"
       << status
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

void Execution::ft_handle_error(Client& client)
{
    client.rsp.status_code=status;

    client.rsp.headers["Content-Type"] = "text/html";
    if(status==405)
    {
        std::string tmp;
        for(size_t i=0; i< s_info._methods.size();i++)
        {
            tmp+=s_info._methods[i];
            if(i<s_info._methods.size()-1)
                tmp+=", ";
        }
        client.rsp.headers["Allow"] = tmp;
    }

    if(ft_error_1(client))
        return ;
    ft_error_2(client);
}

void Execution::ft_handle_Redirect(Client& client)
{
    client.rsp.status_code=s_info._returnCode;

    client.rsp.headers["Location"] = s_info._returnTarget;
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

bool Execution::ft_is_cgi()
{

    return false;
}

void Execution::ft_handle_cgi()
{

    return;
}



