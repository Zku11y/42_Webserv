#include <iostream>
#include <map>
#include <string>
#include "inc/http/http.hpp"

static const char *state_name(HttpParser::State state)
{
    if (state == HttpParser::REQUEST_LINE) return "REQUEST_LINE";
    if (state == HttpParser::HEADERS) return "HEADERS";
    if (state == HttpParser::BODY_CONTENT_LENGTH) return "BODY_CONTENT_LENGTH";
    if (state == HttpParser::CHUNK_SIZE) return "CHUNK_SIZE";
    if (state == HttpParser::CHUNK_DATA) return "CHUNK_DATA";
    if (state == HttpParser::CHUNK_TRAILERS) return "CHUNK_TRAILERS";
    if (state == HttpParser::COMPLETE) return "COMPLETE";
    if (state == HttpParser::ERROR) return "ERROR";
    return "UNKNOWN";
}

static void run_test(const std::string& name, const std::string& raw,
    HttpParser::State expected, bool future_rule)
{
    HttpParser parser(1024);
    std::string buffer = raw;
    const HttpRequest *req;
    std::map<std::string, std::string>::const_iterator it;

    parser.parse(buffer);
    req = &parser.get_request();
    std::cout << "\n========================================\n";
    std::cout << "TEST: " << name << "\n";
    std::cout << "Expected: " << state_name(expected) << "\n";
    if (future_rule)
        std::cout << "NOTE: Host-value validation not implemented yet\n";
    std::cout << "Actual:   " << state_name(parser.get_state()) << "\n";
    std::cout << "RESULT: "
        << (parser.get_state() == expected ? "PASS" : "FAIL") << "\n";
    std::cout << "--- Parsed HttpRequest ---\n";
    std::cout << "Method:  [" << req->method << "]\n";
    std::cout << "Target:  [" << req->target << "]\n";
    std::cout << "Path:    [" << req->path << "]\n";
    std::cout << "Query:   [" << req->queryString << "]\n";
    std::cout << "Version: [" << req->version << "]\n";
    std::cout << "Headers:\n";
    for (it = req->headers.begin(); it != req->headers.end(); ++it){
        std::cout << "  [" << it->first << "] = [" << it->second << "]\n";
	}
	std::cout << "Host:    [" << req->host << "]\n";
	std::cout << "Port:    [" << req->port << "]\n";
    std::cout << "Body:    [" << req->body << "]\n";
    std::cout << "Remaining buffer: [" << buffer << "]\n";
}

int main()
{
	run_test(
	"19 - Valid custom method",
	"BANANA / HTTP/1.1\r\n"
	"Host: localhost\r\n"
	"\r\n",
	HttpParser::COMPLETE,
	false
);

run_test(
	"20 - Valid method with hyphen",
	"CUSTOM-METHOD / HTTP/1.1\r\n"
	"Host: localhost\r\n"
	"\r\n",
	HttpParser::COMPLETE,
	false
);

run_test(
	"21 - Invalid method with @",
	"GE@T / HTTP/1.1\r\n"
	"Host: localhost\r\n"
	"\r\n",
	HttpParser::ERROR,
	false
);

run_test(
	"22 - Invalid method with slash",
	"GET/TEST / HTTP/1.1\r\n"
	"Host: localhost\r\n"
	"\r\n",
	HttpParser::ERROR,
	false
);
	return 0;
}

