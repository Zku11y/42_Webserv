#include "../../inc/main.hpp"


// ============================================================
// FAKE REQUEST CONFIGURATION
// Change the active block to test a different resource.
// ============================================================

static HttpRequest ft_make_fake_request()
{
    HttpRequest req;

    // TEST 1: STATIC HTML
    req.method = "GET";
    req.uri = "/index.html";
    req.query_string = "";
    req.physical_path = "www/index.html";
    req.is_directory = false;
    req.autoindex = false;
    req.cgi_executable = "";
    req.upload_store = "";
    req.body_fd = -1;

    // TEST 2: PYTHON CGI

    // req.method = "GET";
    // req.uri = "/cgi-bin/test.py";
    // req.query_string = "user=ayoub&action=test";
    // req.physical_path = "www/cgi-bin/test.py";
    // req.is_directory = false;
    // req.autoindex = false;
    // req.cgi_executable = "/usr/bin/python3";
    // req.upload_store = "";
    // req.body_fd = -1;

    // TEST 3: SHELL CGI

    // req.method = "GET";
    // req.uri = "/cgi-bin/test.sh";
    // req.query_string = "user=ayoub&action=test";
    // req.physical_path = "www/cgi-bin/test.sh";
    // req.is_directory = false;
    // req.autoindex = false;
    // req.cgi_executable = "/bin/bash";
    // req.upload_store = "";
    // req.body_fd = -1;

    // TEST 4: DIRECTORY

    // req.method = "GET";
    // req.uri = "/directory/";
    // req.query_string = "";
    // req.physical_path = "www/directory";
    // req.is_directory = true;
    // req.autoindex = true;
    // req.cgi_executable = "";
    // req.upload_store = "";
    // req.body_fd = -1;

    // TEST 5: UPLOAD

    // req.method = "POST";
    // req.uri = "/upload/test.txt";
    // req.query_string = "";
    // req.physical_path = "www/uploads/test.txt";
    // req.is_directory = false;
    // req.autoindex = false;
    // req.cgi_executable = "";
    // req.upload_store = "www/uploads";
    // req.body_fd = -1;

    return req;
}

// ============================================================
// RESPONSE HELPERS
// Prototype only. Dev 2 owns final HTTP response generation.
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

void ft_set_error(Client& client, int status)
{
    std::ostringstream body;

    body << "<html><body><h1>"
         << status << " " << ft_status_text(status)
         << "</h1></body></html>\n";

    client.status_code = status;
    client.raw_body = body.str();
}

static void ft_build_response(Client& client)
{
    std::ostringstream response;

    response << "HTTP/1.1 "
             << client.status_code << " "
             << ft_status_text(client.status_code)
             << "\r\n";

    response << "Content-Type: text/html\r\n";
    response << "Content-Length: "
             << client.raw_body.size() << "\r\n";
    response << "Connection: close\r\n\r\n";
    response << client.raw_body;

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
// PROTOTYPE REQUEST COMPLETENESS CHECK
// This does not replace Dev 2's HTTP parser.
// ============================================================

static bool ft_request_complete(const std::string& request)
{
    std::string::size_type header_end = request.find("\r\n\r\n");

    if (header_end == std::string::npos)
        return false;

    std::string::size_type pos = request.find("\r\n");

    if (pos == std::string::npos)
        return false;

    pos += 2;

    size_t content_length = 0;
    bool has_content_length = false;

    while (pos < header_end)
    {
        std::string::size_type end = request.find("\r\n", pos);

        if (end == std::string::npos || end > header_end)
            break;

        std::string line = request.substr(pos, end - pos);
        std::string::size_type colon = line.find(':');

        if (colon != std::string::npos)
        {
            std::string name = line.substr(0, colon);

            for (size_t i = 0; i < name.size(); ++i)
                name[i] = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(name[i]))
                );

            if (name == "content-length")
            {
                std::string value = line.substr(colon + 1);
                std::string::size_type first = value.find_first_not_of(" \t");
                std::string::size_type last = value.find_last_not_of(" \t");

                if (first == std::string::npos)
                    return false;

                value = value.substr(first, last - first + 1);

                size_t parsed = 0;

                for (size_t i = 0; i < value.size(); ++i)
                {
                    if (value[i] < '0' || value[i] > '9')
                        return false;

                    size_t digit = static_cast<size_t>(value[i] - '0');

                    if (parsed > (static_cast<size_t>(-1) - digit) / 10)
                        return false;

                    parsed = parsed * 10 + digit;
                }

                if (has_content_length && content_length != parsed)
                    return false;

                content_length = parsed;
                has_content_length = true;
            }
        }

        pos = end + 2;
    }

    if (!has_content_length)
        return true;

    return request.size() - (header_end + 4) >= content_length;
}

// ============================================================
// MAIN: MOCK DEV 1 + DEV 2 SERVER
// ============================================================

int ft_core(Config&  config)
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
        int ready = poll(poll_fds.data(), poll_fds.size(), -1);

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

            // ACCEPT CONNECTIONS
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
                    client.status_code = 200;
                    clients[client_fd] = client;
                    send_offsets[client_fd] = 0;

                    ft_log("New client: " << client_fd);
                }

                continue;
            }

            // SOCKET ERRORS
            if (events & (POLLERR | POLLNVAL))
            {
                ft_remove_client(fd, poll_fds, clients, send_offsets);
                --i;
                --count;
                continue;
            }

            // READ REQUEST
            if ((events & POLLIN) && clients.find(fd) != clients.end())
            {
                char buffer[READ_BUFFER_SIZE];
                bool disconnected = false;
                bool failed = false;

                while (true)
                {
                    ssize_t n = recv(fd, buffer, sizeof(buffer), 0);

                    if (n > 0)
                    {
                        Client& client = clients[fd];

                        client.request_buffer.append(buffer, static_cast<size_t>(n));

                        if (client.request_buffer.size() > MAX_REQUEST_SIZE)
                        {
                            ft_set_error(client, 413);
                            ft_build_response(client);

                            for (size_t j = 0; j < poll_fds.size(); ++j)
                            {
                                if (poll_fds[j].fd == fd)
                                {
                                    poll_fds[j].events = POLLOUT;
                                    break;
                                }
                            }

                            break;
                        }

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

                if (disconnected || failed)
                {
                    ft_remove_client(fd, poll_fds, clients, send_offsets);
                    --i;
                    --count;
                    continue;
                }

                if (clients.find(fd) == clients.end())
                    continue;

                bool responding = false;

                for (size_t j = 0; j < poll_fds.size(); ++j)
                {
                    if (poll_fds[j].fd == fd &&
                        (poll_fds[j].events & POLLOUT))
                    {
                        responding = true;
                        break;
                    }
                }

                if (responding)
                    continue;

                Client& client = clients[fd];

                if (ft_request_complete(client.request_buffer))
                {
                    ft_log("\n---- Received request ----");
                    ft_log_(client.request_buffer);
                    ft_log("----- End request -----\n");

                    // Fake Dev 2 parsed-request handoff.
                    client.parsed_req = ft_make_fake_request();

                    // Dev 3 resource execution starts here.
                    ft_handle_resource(client, config);

                    // Fake Dev 2 response generation.
                    ft_build_response(client);

                    send_offsets[fd] = 0;

                    for (size_t j = 0; j < poll_fds.size(); ++j)
                    {
                        if (poll_fds[j].fd == fd)
                        {
                            poll_fds[j].events = POLLOUT;
                            break;
                        }
                    }
                }
            }

            // SEND RESPONSE
            if ((events & POLLOUT) && clients.find(fd) != clients.end())
            {
                Client& client = clients[fd];
                std::string& response = client.response_buffer;
                size_t& offset = send_offsets[fd];
                bool failed = false;

                while (offset < response.size())
                {
                    ssize_t n = send(fd, response.data() + offset,
                                     response.size() - offset, 0);

                    if (n > 0)
                    {
                        offset += static_cast<size_t>(n);
                        continue;
                    }

                    if (n < 0 && errno == EINTR)
                        continue;

                    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
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

            // CLIENT DISCONNECTED
            if ((events & POLLHUP) && clients.find(fd) != clients.end())
            {
                ft_remove_client(fd, poll_fds, clients, send_offsets);
                --i;
                --count;
                continue;
            }
        }
    }

    for (std::map<int, Client>::iterator it = clients.begin();
         it != clients.end(); ++it)
        close(it->first);

    close(server_fd);
    return 0;
}
