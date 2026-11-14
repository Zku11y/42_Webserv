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
#include "./http/client.hpp"
#include "./cgi_io/Execution.hpp"

#define PORT 8080
#define BACKLOG 10
#define READ_BUFFER_SIZE 4096
#define MAX_REQUEST_SIZE (25 * 1024 * 1024)

#define ft_log_(x) std::cout << x
#define ft_log(x) std::cout << x << std::endl
#define ft_log_r(x) std::cerr << x << std::endl


int main(int ac, char **av);
