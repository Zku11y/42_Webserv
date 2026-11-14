#include "inc/main.hpp"


static void ft_log_response(const HttpResponse& rsp)
{
    ft_log("\n========================================");
    ft_log("          HTTP RESPONSE");
    ft_log("========================================");

    // Status
    ft_log("\n[STATUS]");
    ft_log("Status Code   : [" << rsp.status_code << "]");

    // Headers
    ft_log("\n[HEADERS] (" << rsp.headers.size() << ")");

    for (std::map<std::string, std::string>::const_iterator it =
         rsp.headers.begin(); it != rsp.headers.end(); ++it)
    {
        ft_log("  " << it->first << " : [" << it->second << "]");
    }

    // Body
    ft_log("\n[BODY]");
    ft_log("Body FD       : [" << rsp.body_fd << "]");
    ft_log("Body Size     : [" << rsp.body_size << "] bytes");
    ft_log("String Size   : [" << rsp.body.size() << "] bytes");

    if (rsp.body_fd == -1)
        ft_log("Body Content  : [" << rsp.body << "]");
    else
        ft_log("Body Content  : [Stored in file descriptor "
               << rsp.body_fd << "]");

    ft_log("========================================\n");
}

static void ft_log_request(const HttpRequest& req)
{
    ft_log("\n========================================");
    ft_log("          PARSED HTTP REQUEST");
    ft_log("========================================");

    // Connection information
    ft_log("\n[CONNECTION]");
    ft_log("Local Host    : [" << req.localHost << "]");
    ft_log("Local Port    : [" << req.localPort << "]");

    // Request line
    ft_log("\n[REQUEST LINE]");
    ft_log("Method        : [" << req.method << "]");
    ft_log("Target        : [" << req.target << "]");
    ft_log("Path          : [" << req.path << "]");
    ft_log("Query String  : [" << req.queryString << "]");
    ft_log("HTTP Version  : [" << req.version << "]");

    // Parsed Host header
    ft_log("\n[HOST]");
    ft_log("Host          : [" << req.host << "]");
    ft_log("Port          : [" << req.port << "]");

    // Headers
    // ft_log("\n[HEADERS] (" << req.headers.size() << ")");

    // for (std::map<std::string, std::string>::const_iterator it =
    //      req.headers.begin(); it != req.headers.end(); ++it)
    // {
    //     ft_log("  " << it->first << " : [" << it->second << "]");
    // }

    // Body
    ft_log("\n[BODY]");
    ft_log("Body Size     : " << req.body.size() << " bytes");
    ft_log("Body Content  : [" << req.body << "]");

    ft_log("========================================\n");
}

// ============================================================
// FAKE REQUEST CONFIGURATION
// Change the active block to test a different resource.
// ============================================================

static const char* ft_status_text(int status)
{
    switch (status)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Content Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default:  return "Internal Server Error";
    }
}

void ft_build_response(Client& client)
{
    HttpResponse& rsp = client.rsp;
    std::string body;

    // 1. Get response body
    if (rsp.body_fd == -1)
    {
        body = rsp.body;
    }
    else
    {
        char buffer[8192];
        ssize_t n;

        while (true)
        {
            n = read(rsp.body_fd, buffer, sizeof(buffer));

            if (n > 0)
                body.append(buffer, static_cast<size_t>(n));
            else if (n == 0)
                break;
            else if (errno == EINTR)
                continue;
            else
            {
                rsp.status_code = 500;
                body = "Internal Server Error";
                rsp.headers.clear();
                break;
            }
        }

        close(rsp.body_fd);
        rsp.body_fd = -1;
    }

    // 2. Set actual body size
    rsp.body_size = body.size();

    // 3. Build status line
    std::ostringstream response;

    response << "HTTP/1.1 "
             << rsp.status_code << " "
             << ft_status_text(rsp.status_code)
             << "\r\n";

    // 4. Send headers from HttpResponse

    for (std::map<std::string, std::string>::const_iterator it =
             rsp.headers.begin(); it != rsp.headers.end(); ++it)
    {
        std::string name = it->first;

        for (size_t i = 0; i < name.size(); ++i)
            name[i] = static_cast<char>(
                std::tolower(static_cast<unsigned char>(name[i]))
            );

        // Content-Length is calculated from the actual body
        if (name == "content-length")
            continue;

        if (name == "connection")
            continue;

        response << it->first << ": " << it->second << "\r\n";
    }

    // 5. Add Content-Length
        response << "Content-Length: " << rsp.body_size << "\r\n";
        response << "Connection: close\r\n";

    // 7. End headers
    response << "\r\n";

    // 8. Append body
    response << body;

    client.response_buffer = response.str();
}

// ============================================================
// SOCKET HELPERS
// ============================================================

static bool ft_set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return false;

    int fd_flags = fcntl(fd, F_GETFD, 0);

    if (fd_flags < 0 || fcntl(fd, F_SETFD, fd_flags | FD_CLOEXEC) < 0)
        return false;

    return true;
}

static void ft_remove_client(
    int fd,
    std::vector<struct pollfd>& poll_fds,
    std::map<int, Client>& clients,
    std::map<int, size_t>& send_offsets)
{
    close(fd);
    clients.erase(fd);
    send_offsets.erase(fd);

    for (size_t i = 0; i < poll_fds.size(); ++i)
    {
        if (poll_fds[i].fd == fd)
        {
            poll_fds.erase(poll_fds.begin() + i);
            break;
        }
    }
}


// ============================================================
// MAIN: MOCK DEV 1 + DEV 2 SERVER
// ============================================================

int ft_core(Config& config)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        ft_log_r("socket creation failed");
        return 1;
    }

    int opt = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0)
    {
        ft_log_r("setsockopt failed");
        close(server_fd);
        return 1;
    }

    if (!ft_set_nonblocking(server_fd))
    {
        ft_log_r("failed to configure server socket");
        close(server_fd);
        return 1;
    }

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd,
             reinterpret_cast<struct sockaddr*>(&server_addr),
             sizeof(server_addr)) < 0)
    {
        ft_log_r("bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0)
    {
        ft_log_r("listen failed");
        close(server_fd);
        return 1;
    }

    std::vector<struct pollfd> poll_fds;

    struct pollfd server_pfd;
    server_pfd.fd = server_fd;
    server_pfd.events = POLLIN;
    server_pfd.revents = 0;
    poll_fds.push_back(server_pfd);

    std::map<int, Client> clients;
    std::map<int, size_t> send_offsets;

    ft_log("Prototype listening on http://localhost:" << PORT);

    while (true)
    {
        int ready = poll(&poll_fds[0], poll_fds.size(), -1);

        if (ready < 0)
        {
            if (errno == EINTR)
                continue;

            ft_log_r("poll failed");
            break;
        }

        size_t count = poll_fds.size();

        for (size_t i = 0; i < count && i < poll_fds.size(); ++i)
        {
            int fd = poll_fds[i].fd;
            short events = poll_fds[i].revents;

            if (events == 0)
                continue;

            // ====================================================
            // ACCEPT CONNECTIONS
            // ====================================================

            if (fd == server_fd)
            {
                if (!(events & POLLIN))
                    continue;

                while (true)
                {
                    int client_fd = accept(server_fd, NULL, NULL);

                    if (client_fd < 0)
                    {
                        if (errno == EINTR)
                            continue;

                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;

                        ft_log_r("accept failed");
                        break;
                    }

                    if (!ft_set_nonblocking(client_fd))
                    {
                        close(client_fd);
                        continue;
                    }

                    struct pollfd client_pfd;
                    client_pfd.fd = client_fd;
                    client_pfd.events = POLLIN;
                    client_pfd.revents = 0;

                    poll_fds.push_back(client_pfd);

                    Client client;
                    client.fd = client_fd;

                    clients[client_fd] = client;
                    send_offsets[client_fd] = 0;

                    // ft_log("New client: " << client_fd);
                }

                continue;
            }

            // ====================================================
            // SOCKET ERRORS
            // ====================================================

            if (events & (POLLERR | POLLNVAL))
            {
                ft_remove_client(fd, poll_fds, clients, send_offsets);
                --i;
                --count;
                continue;
            }

            // ====================================================
            // READ REQUEST + DEV 2 HTTP PARSER
            // ====================================================

            if ((events & POLLIN) && clients.find(fd) != clients.end())
            {
                Client& client = clients[fd];

                char buffer[READ_BUFFER_SIZE];
                bool disconnected = false;
                bool failed = false;
                bool response_ready = false;

                while (true)
                {
                    ssize_t n = recv(fd, buffer, sizeof(buffer), 0);

                    if (n > 0)
                    {
                        // 1. Append received bytes
                        client.request_buffer.append(
                            buffer, static_cast<size_t>(n)
                        );

                        // 2. Parse HTTP request using Dev 2's parser
                        client.parser.parse(client.request_buffer);

                        // 3. Check parser error
                        if (client.parser.has_error())
                        {
                            client.rsp.status_code = 400;
                            client.rsp.body = "Bad Request";

                            ft_build_response(client);

                            response_ready = true;
                            break;
                        }

                        // 4. Check whether parsing is complete
                        if (client.parser.is_complete())
                        {
                            // Get the real parsed HttpRequest
                            client.parsed_req = client.parser.get_request();

                            // Connection information from socket
                            client.parsed_req.localHost = "0.0.0.0";
                            client.parsed_req.localPort = PORT;

                            bool debug_request =
                                client.parsed_req.path != "/.well-known/appspecific/com.chrome.devtools.json";

                            if (debug_request)
                                ft_log_request(client.parsed_req);

                            // 5. Pass request to your Execution class
                            Execution cgi(client, config);
                            cgi.ft_handle_resource(client);
                            
                            if (debug_request)
                                ft_log_response(client.rsp);

                            ft_build_response(client); // ADD THIS

                            response_ready = true;
                            break;
                        }

                        // Incomplete HTTP request:
                        // continue receiving available bytes
                        continue;
                    }

                    if (n == 0)
                    {
                        disconnected = true;
                        break;
                    }

                    if (errno == EINTR)
                        continue;

                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;

                    failed = true;
                    break;
                }

                // Remove client if disconnected or socket failed
                if (disconnected || failed)
                {
                    ft_remove_client(fd, poll_fds, clients, send_offsets);
                    --i;
                    --count;
                    continue;
                }

                // HTTP request complete or parser returned an error
                if (response_ready)
                {
                    send_offsets[fd] = 0;

                    // Change poll event from POLLIN to POLLOUT
                    for (size_t j = 0; j < poll_fds.size(); ++j)
                    {
                        if (poll_fds[j].fd == fd)
                        {
                            poll_fds[j].events = POLLOUT;
                            break;
                        }
                    }
                }

                // Otherwise wait for more request bytes
                continue;
            }

            // ====================================================
            // SEND RESPONSE
            // ====================================================

            if ((events & POLLOUT) && clients.find(fd) != clients.end())
            {
                Client& client = clients[fd];

                std::string& response = client.response_buffer;
                size_t& offset = send_offsets[fd];

                bool failed = false;

                while (offset < response.size())
                {
                    ssize_t n = send(
                        fd,
                        response.data() + offset,
                        response.size() - offset,
                        0
                    );

                    if (n > 0)
                    {
                        offset += static_cast<size_t>(n);
                        continue;
                    }

                    if (n < 0 && errno == EINTR)
                        continue;

                    if (n < 0 &&
                        (errno == EAGAIN || errno == EWOULDBLOCK))
                        break;

                    failed = true;
                    break;
                }

                if (failed || offset >= response.size())
                {
                    ft_remove_client(fd, poll_fds, clients, send_offsets);
                    --i;
                    --count;
                    continue;
                }
            }

            // ====================================================
            // CLIENT DISCONNECTED
            // ====================================================

            if ((events & POLLHUP) && clients.find(fd) != clients.end())
            {
                ft_remove_client(fd, poll_fds, clients, send_offsets);
                --i;
                --count;
                continue;
            }
        }
    }

    // ====================================================
    // CLEANUP
    // ====================================================

    for (std::map<int, Client>::iterator it = clients.begin();
         it != clients.end(); ++it)
    {
        close(it->first);
    }

    close(server_fd);

    return 0;
}








int main(int ac, char **av)
{
	if (ac != 2){
		std::cerr << "Usage: ./webserv <config>" << std::endl;
		return 1;
	}

	try{
        Config  config(av[1]);
		ft_core(config);


        // Client client;
        // client.parsed_req =ft_make_fake_request();
		// Execution cgi(client, config);
		// cgi.ft_handle_resource(client);

	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
