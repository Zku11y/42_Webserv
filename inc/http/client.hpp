#pragma once

#include "./HttpResponse.hpp"
#include "./http.hpp"

#include <string>

struct Client {

    int         fd;

    std::string request_buffer;//manuel req ,remove later
    HttpParser parser;
    
    
    HttpRequest parsed_req;
    HttpResponse rsp;
    

    std::string response_buffer;//manuel rsp ,remove later 

    Client() : parser(20 * 1024 * 1024) {}   
};
