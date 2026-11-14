#include "../../inc/http/http.hpp"

HttpParser::HttpParser(size_t _max_body_size) : state(REQUEST_LINE), error_code(NO_ERROR), content_length(0),
	chunk_size(0), chunk_received(0), max_body_size(_max_body_size) {}
HttpParser::~HttpParser() {}

HttpParser::State	HttpParser::get_state() const{
	return state;
}

const HttpRequest&	HttpParser::get_request() const{
	return request;
}

bool	HttpParser::parse_request_line(std::string& buffer){
	size_t		end;
	size_t		space1;
	size_t		space2;
	size_t		query;
	std::string	line;

	end = buffer.find("\r\n");
	if (end == std::string::npos)
		return false;
	line = buffer.substr(0, end);
	space1 = line.find(' ');
	if (space1 == std::string::npos){
		return set_error(BAD_REQUEST);
	}
	space2 = line.find(' ', space1 + 1);
	if (space2 == std::string::npos){
		return set_error(BAD_REQUEST);
	}
	request.method = line.substr(0, space1);
	request.target = line.substr(space1 + 1, space2 - space1 - 1);
	request.version = line.substr(space2 + 1);
	if (!valid_method(request.method) || !valid_target(request.target) || request.version.empty()){
		return set_error(BAD_REQUEST);
	}
	if (request.version.size() != 8 || request.version.compare(0, 5, "HTTP/") != 0
		|| request.version[5] < '0' || request.version[5] > '9'
		|| request.version[6] != '.'
		|| request.version[7] < '0' || request.version[7] > '9')
		return set_error(BAD_REQUEST);
	if (request.version != "HTTP/1.1"){
		return set_error(HTTP_VERSION_NOT_SUPPORTED);
	}
	query = request.target.find('?');
	if (query == std::string::npos){
		request.path = request.target;
		request.queryString.clear();
	}
	else{
		request.path = request.target.substr(0, query);
		request.queryString = request.target.substr(query + 1);
	}
	buffer.erase(0, end + 2);
	state = HEADERS;
	return true;
}

bool	HttpParser::parse_headers(std::string& buffer){
	size_t		end;
	size_t		colon;
	std::string	line;
	std::string	name;
	std::string	value;

	while (true){
		end = buffer.find("\r\n");
		if (end == std::string::npos)
			return false;
		if (end == 0){
			if (!validate_request_headers()){
				return set_error(BAD_REQUEST);
			}
			buffer.erase(0, 2);
			return determine_body_framing();
		}
		line = buffer.substr(0, end);
		if (line[0] == ' ' || line[0] == '\t'){
			return set_error(BAD_REQUEST);
		}
		colon = line.find(':');
		if (colon == std::string::npos){
			return set_error(BAD_REQUEST);
		}
		value = trim_ows(line.substr(colon + 1));
		if (!valid_header_value(value)){
			return set_error(BAD_REQUEST);
		}
		name = line.substr(0, colon);
		if (!valid_header_name(name)){
			return set_error(BAD_REQUEST);
		}
		name = to_lower(name);
		if (name == "host" && request.headers.find(name) != request.headers.end()){
			return set_error(BAD_REQUEST);
		}
		if (request.headers.find(name) == request.headers.end())
			request.headers[name] = value;
		else
			request.headers[name] += "," + value;
		buffer.erase(0, end + 2);
	}
}

bool	HttpParser::parse_body(std::string& buffer){
	size_t	remaining;
	size_t	to_read;

	if (request.body.size() > content_length){
		return set_error(BAD_REQUEST);
	}
	remaining = content_length - request.body.size();
	if (remaining == 0){
		state = COMPLETE;
		return true;
	}
	if (buffer.empty())
		return false;
	if (buffer.size() < remaining)
		to_read = buffer.size();
	else
		to_read = remaining;
	request.body.append(buffer,0, to_read);
	buffer.erase(0, to_read);
	if (request.body.size() == content_length)
		state = COMPLETE;
	return true;
}

bool	HttpParser::parse_chunk_size(std::string& buffer){
	size_t		end;
	size_t		semicolon;
	std::string	line;

	end = buffer.find("\r\n");
	if (end == std::string::npos)
		return false;
	line = buffer.substr(0, end);
	semicolon = line.find(';');
	if (semicolon != std::string::npos)
		line = line.substr(0, semicolon);
	if (!parse_hex_size(line, chunk_size)){
		return set_error(BAD_REQUEST);
	}
	buffer.erase(0, end + 2);
	chunk_received = 0;
	if (chunk_size == 0)
		state = CHUNK_TRAILERS;
	else
		state = CHUNK_DATA;
	return true;
}

bool	HttpParser::parse_chunk_data(std::string& buffer){
	size_t	remaining;
	size_t	to_read;

	if (chunk_received > chunk_size){
		return set_error(BAD_REQUEST);
	}
	remaining = chunk_size - chunk_received;
	if (remaining > 0)
	{
		if (buffer.empty())
			return false;
		if (buffer.size() < remaining)
			to_read = buffer.size();
		else
			to_read = remaining;
		if (to_read > max_body_size|| 
			request.body.size() > max_body_size - to_read){
			return set_error(CONTENT_TOO_LARGE);
		}
		request.body.append(buffer, 0, to_read);
		buffer.erase(0, to_read);
		chunk_received += to_read;
		if (chunk_received < chunk_size)
			return false;
	}
	if (buffer.size() < 2)
		return false;
	if (buffer[0] != '\r' || buffer[1] != '\n'){
		return set_error(BAD_REQUEST);
	}
	buffer.erase(0, 2);
	chunk_size = 0;
	chunk_received = 0;
	state = CHUNK_SIZE;
	return true;
}

bool	HttpParser::parse_chunk_trailers(std::string& buffer){
	size_t	end;
	size_t	colon;

	while (true){
		end = buffer.find("\r\n");
		if (end == std::string::npos)
			return false;
		if (end == 0){
			buffer.erase(0, 2);
			state = COMPLETE;
			return true;
		}
		colon = buffer.find(':');
		if (colon == std::string::npos || colon >= end || !valid_header_name(buffer.substr(0, colon))
			|| !valid_header_value(buffer.substr(colon + 1, end - colon - 1)))
			return set_error(BAD_REQUEST);
		buffer.erase(0, end + 2);
	}
}

void	HttpParser::parse(std::string& buffer){
	while(state != COMPLETE && state != ERROR){
		if (state == REQUEST_LINE){
			if (!parse_request_line(buffer))
				return;
		}
		else if (state == HEADERS){
			if (!parse_headers(buffer))
				return;
		}
		else if (state == BODY_CONTENT_LENGTH){
			if (!parse_body(buffer))
				return;
		}
		else if (state == CHUNK_SIZE){
			if (!parse_chunk_size(buffer))
			return;
		}
		else if (state == CHUNK_DATA){
			if (!parse_chunk_data(buffer))
				return;
		}
		else if (state == CHUNK_TRAILERS){
			if (!parse_chunk_trailers(buffer))
				return;
		}
		else
			return;
	}
}
