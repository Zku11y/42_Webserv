#include "../../inc/http/http.hpp"

std::string	HttpParser::trim_ows(const std::string& str) const{
	size_t	start;
	size_t	end;

	start = 0;
	while (start < str.size() && (str[start] == ' ' || str[start] == '\t')){
		start++;
	}
	end = str.size();
	while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t')){
		end--;
	}
	return str.substr(start, end - start);
}

std::string	HttpParser::to_lower(const std::string& str) const{
	std::string	result;

	result = str;
	for (size_t a = 0; a < result.size(); a++){
		result[a] = std::tolower(static_cast<unsigned char>(result[a]));
	}
	return result;
}

bool	HttpParser::is_token_char(char c) const{
	if (std::isalnum(static_cast<unsigned char>(c)))
		return true;
	if (c == '!' || c == '#' || c == '$' || c == '%'
		||c == '&' || c == '\'' || c == '*' || c == '+'
		|| c == '-' || c == '.' || c == '^' || c == '_'
		|| c == '`' || c == '|' || c == '~')
		return true;
	return false;
}

bool	HttpParser::valid_header_name(const std::string& name) const{
	if (name.empty())
		return false;
	for (size_t a = 0; a < name.size(); a++){
		if (!is_token_char(name[a]))
			return false;
	}
	return true;
}

bool	HttpParser::valid_header_value(const std::string& value) const{
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

std::vector<std::string>	HttpParser::split(const std::string& str, char del) const{
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

bool	HttpParser::parse_size_value(const std::string& value, size_t& result) const{
	size_t	digit;

	result = 0;
	if (value.empty())
		return false;
	for (size_t a = 0; a < value.size(); a++){
		if (!std::isdigit(static_cast<unsigned char>(value[a])))
			return false;
		digit = value[a] - '0';
		if (result > (std::numeric_limits<size_t>::max() - digit) / 10)
			return false;
		result = result * 10 + digit;
	}
	return true;
}

bool	HttpParser::parse_content_length(const std::string& value){
	std::vector<std::string>	values;
	size_t						expected;
	size_t						current;

	values = split(value, ',');
	if (values.empty())
		return false;
	for (size_t a = 0; a < values.size(); a++){
		values[a] = trim_ows(values[a]);
		if (!parse_size_value(values[a], current))
			return false;
		if (a == 0)
			expected = current;
		else if (current != expected)
			return false;
	}
	content_length = expected;
	return true;	
}

bool	HttpParser::determine_body_framing(){
	std::map<std::string, std::string>::iterator	transfer_encoding;
	std::map<std::string, std::string>::iterator	_content_length;

	transfer_encoding = request.headers.find("transfer-encoding");
	_content_length = request.headers.find("content-length");
	if (transfer_encoding != request.headers.end() 
		&& _content_length != request.headers.end()){
		return set_error(BAD_REQUEST);
	}
	if (transfer_encoding != request.headers.end()){
		if (!valid_transfer_encoding(transfer_encoding->second)){
			return set_error(BAD_REQUEST);
		}
		state = CHUNK_SIZE;
		return true;
	}
	if (_content_length != request.headers.end()){
		if (!parse_content_length(_content_length->second)){
			return set_error(BAD_REQUEST);
		}
		if (content_length > max_body_size)
			return set_error(CONTENT_TOO_LARGE);
		if (content_length == 0)
			state = COMPLETE;
		else
			state = BODY_CONTENT_LENGTH;
		return true;
	}
	state = COMPLETE;
	return true;
}

bool	HttpParser::parse_hex_size(const std::string& value, size_t& result) const{
	size_t	digit;

	if (value.empty())
		return false;
	result = 0;
	for (size_t a = 0; a < value.size(); a++){
		if (value[a] >= '0' && value[a] <= '9')
			digit = value[a] - '0';
		else if (value[a] >= 'a' && value[a] <= 'f')
			digit = value[a] - 'a' + 10;
		else if (value[a] >= 'A' && value[a] <= 'F')
			digit = value[a] - 'A' + 10;
		else
			return false;
		if (result > (std::numeric_limits<size_t>::max() - digit) / 16)
			return false;
		result = result * 16 + digit;
	}
	return true;
}

bool	HttpParser::valid_transfer_encoding(const std::string& value) const{
	std::vector<std::string>	codings;
	std::string					last;

	codings = split(value, ',');
	if (codings.empty())
		return false;
	for (size_t a = 0; a < codings.size(); a++)
		codings[a] = to_lower(trim_ows(codings[a]));
	last = codings[codings.size() - 1];
	if (last != "chunked")
		return false;
	if (codings.size() != 1)
		return false;
	return true;
}

////////


bool	HttpParser::validate_request_headers(){
	std::map<std::string, std::string>::iterator	host;

	host = request.headers.find("host");
	if (host == request.headers.end())
		return false;
	if (!parse_host_header(host->second))
		return false;
	return true;
}

bool	HttpParser::valid_host_name(const std::string& value) const{
	if (value.empty())
	return true;
	for (size_t a = 0; a < value.size(); a++){
		if (!std::isalnum(static_cast<unsigned char>(value[a])) && value[a] != '-'
		&& value[a] != '.' && value[a] != '_')
		return false;
	}
	return true;
}

bool	HttpParser::valid_port(const std::string& value) const{
	for (size_t a = 0; a < value.size(); a++){
		if (!std::isdigit(static_cast<unsigned char>(value[a]))){
			return false;
		}
	}
	return true;
}

bool	HttpParser::valid_ip_literal(const std::string& value) const{
	if (value.empty())
		return false;
	for (size_t i = 0; i < value.size(); i++){
		if (!std::isxdigit(static_cast<unsigned char>(value[i])) 
			&& value[i] != ':' && value[i] != '.')
			return false;
	}
	return true;
}

bool	HttpParser::parse_host_header(const std::string& value){
	size_t	close;
	size_t	colon;

	request.host.clear();
	request.port.clear();
	if (value.empty())
		return true;
	if (value[0] == '['){
		close = value.find(']');
		if (close == std::string::npos)
			return false;
		request.host = value.substr(1, close - 1);
		if (!valid_ip_literal(request.host))
			return false;
		if (close + 1 == value.size())
			return true;
		if (value[close + 1] != ':')
			return false;
		request.port = value.substr(close + 2);
		if (!valid_port(request.port))
			return false;
		return true;
	}
	colon = value.find(':');
	if (colon == std::string::npos)
		request.host = value;
	else{
		request.host = value.substr(0, colon);
		request.port = value.substr(colon + 1);
		if (value.find(':', colon + 1) != std::string::npos)
			return false;
		if (!valid_port(request.port))
			return false;
	}
	if (!valid_host_name(request.host))
		return false;
	return true;
}

bool	HttpParser::valid_method(const std::string& method) const{
	if (method.empty())
		return false;
	for (size_t a = 0; a < method.size(); a++){
		if (!is_token_char(method[a]))
			return false;
	}
	return true;
}

bool	HttpParser::valid_target(const std::string& target) const{
	if (target.empty())
		return false;
	if (target[0] != '/')
		return false;
	for (size_t a = 0; a < target.size(); a++){
		unsigned char	c;
		c = static_cast<unsigned char>(target[a]);
		if (c <= 32 || c == 127)
			return false;
	}
	return true;
}

HttpParser::ErrorCode	HttpParser::get_error_code() const{
	return error_code;
}

bool	HttpParser::is_complete() const{
	return state == COMPLETE;
}

bool	HttpParser::has_error() const{
	return state == ERROR;
}

bool	HttpParser::set_error(ErrorCode code){
	error_code = code;
	state = ERROR;
	return false;
}

void	HttpParser::reset(){
	state = REQUEST_LINE;
	request = HttpRequest();
	content_length = 0;
	chunk_size = 0;
	chunk_received = 0;
	error_code = NO_ERROR;
}
