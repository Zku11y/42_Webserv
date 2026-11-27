#include "../../inc/http/response.hpp"

HttpResponseBuilder::HttpResponseBuilder() {
	status_message[200] = "OK";
	status_message[201] = "Created";
	status_message[204] = "No Content";
	status_message[301] = "Moved Permanently";
	status_message[302] = "Found";
	status_message[400] = "Bad Request";
	status_message[403] = "Forbidden";
	status_message[404] = "Not Found";
	status_message[405] = "Method Not Allowed";
	status_message[413] = "Content Too Large";
	status_message[500] = "Internal Server Error";
	status_message[501] = "Not Implemented";
	status_message[502] = "Bad Gateway";
	status_message[504] = "Gateway Timeout";
	status_message[505] = "HTTP Version Not Supported";
}

HttpResponseBuilder::~HttpResponseBuilder() {}

std::string HttpResponseBuilder::gen_phrase(int status_code) const{
	std::map<int, std::string>::const_iterator  it;

	it = status_message.find(status_code);
	if (it == status_message.end())
		return "Internal Server Error";
	return it->second;
}

std::string	HttpResponseBuilder::to_lower(const std::string& str) const
{
	std::string	result;

	result = str;
	for (size_t a = 0; a < result.size(); a++){
		result[a] = std::tolower(static_cast<unsigned char>(result[a]));
	}
	return result;
}

void    HttpResponseBuilder::erase_header(HttpResponse& response, const std::string& name) const{
	std::map<std::string, std::string>::iterator    it;
	std::map<std::string, std::string>::iterator    next;
	std::string                                     target;

	it = response.headers.begin();
	target = to_lower(name);
	while (it != response.headers.end()){
		next = it;
		++next;
		if (to_lower(it->first) == target)
			response.headers.erase(it);
		it = next;
	}
}

bool	HttpResponseBuilder::is_token_char(char c) const{
	if (std::isalnum(static_cast<unsigned char>(c)))
		return true;
	if (c == '!' || c == '#' || c == '$' || c == '%'
		||c == '&' || c == '\'' || c == '*' || c == '+'
		|| c == '-' || c == '.' || c == '^' || c == '_'
		|| c == '`' || c == '|' || c == '~')
		return true;
	return false;
}

bool	HttpResponseBuilder::valid_header_name(const std::string& name) const{
	if (name.empty())
		return false;
	for (size_t a = 0; a < name.size(); a++){
		if (!is_token_char(name[a]))
			return false;
	}
	return true;
}

bool	HttpResponseBuilder::valid_header_value(const std::string& value) const{
	unsigned char	c;
	for (size_t a = 0; a < value.size(); a++){
		c = static_cast<unsigned char>(value[a]);
		if (c == '\r' || c == '\n')
			return false;
		if (c < 32 && c != '\t')
			return false;
		if (c == 127)
			return false;
	}
	return true;
}

std::string HttpResponseBuilder::read_body_fd(int body_fd, size_t m_size) const{
	std::string 	result;
	char			buffer[8192];
	ssize_t			bytes;
	size_t			total;

	total = 0;
	while (total < m_size){
		bytes = read(body_fd, &buffer, sizeof(buffer));
		if (bytes == 0)
			break;
		if (bytes < 0)
			break;
		if (total + static_cast<size_t>(bytes) > m_size){
			result.append(buffer, m_size - total);
			total = m_size;
			break;
		}
		result.append(buffer, bytes);
		total += static_cast<size_t>(bytes);
	}
	return result;
}

void HttpResponseBuilder::build(HttpResponse& response)
{
	std::stringstream   ss;
	std::map<std::string, std::string>::const_iterator  it;

	response.responc.clear();
	
	ss.imbue(std::locale::classic());
	if (status_message.find(response.status_code) == status_message.end())
		response.status_code = 500;
	response.responc += "HTTP/1.1 ";
	ss << response.status_code;
	response.responc += ss.str();
	response.responc += " ";
	response.responc += gen_phrase(response.status_code);
	response.responc += "\r\n";

	ss.str("");
	ss.clear();
	erase_header(response, "transfer-encoding");
	if (response.status_code == 204){
		response.body.clear();
		if(response.body_fd!=-1)
			close(response.body_fd);
		response.body_fd = -1;
		response.body_size = 0;
		erase_header(response, "content-length");
	}
	if (response.body_fd == -1)
		response.body_size = response.body.size();
	if (response.status_code != 204){
		erase_header(response, "content-length");
		ss << response.body_size;
		response.headers["Content-Length"] = ss.str();
	}

	for (it = response.headers.begin(); it != response.headers.end(); it++){
		if (!valid_header_name(it->first) || !valid_header_value(it->second))
			continue;
		response.responc += it->first;
		response.responc += ": ";
		response.responc += it->second;
		response.responc += "\r\n";
	}

    // Set-Cookie headers
    for (size_t i = 0; i < response.set_cookies.size(); i++){
        if (!valid_header_value(response.set_cookies[i]))
            continue;
        response.responc += "Set-Cookie: ";
        response.responc += response.set_cookies[i];
        response.responc += "\r\n";
    }
	
	response.responc += "\r\n";
	if (response.body_fd == -1)
		response.responc += response.body;
}
