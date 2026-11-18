#pragma once

#include <cstddef>
#include <string>
#include <map>

struct HttpResponse
{
    int status_code;
    std::map<std::string, std::string> headers;

    std::string body;
    int body_fd;
    size_t body_size;
};