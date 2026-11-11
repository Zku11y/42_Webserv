#pragma once

#include <string>
#include <map>
#include <cstddef>
#include <cctype>
#include <vector>
#include <limits>

struct HttpRequest
{
	std::string							method;
	std::string							target;
	std::string							path;
    std::string							queryString;
	std::string							version;
	std::map<std::string, std::string>	headers;
	std::string							body;

	std::string							host;
	std::string							port;
};

class HttpParser
{
	public:
		enum State
		{
			REQUEST_LINE,
			HEADERS,
			BODY_CONTENT_LENGTH,
			CHUNK_SIZE,
			CHUNK_DATA,
			CHUNK_TRAILERS,
			COMPLETE,
			ERROR
		};

		enum ErrorCode
		{
			NO_ERROR = 0,
			BAD_REQUEST = 400,
			CONTENT_TOO_LARGE = 413,
			URI_TOO_LONG = 414,
			HEADER_TOO_LARGE = 431,
			NOT_IMPLEMENTED = 501,
			HTTP_VERSION_NOT_SUPPORTED = 505
		};

	private:
		State		state;
		ErrorCode	error_code;
		size_t		content_length;
		size_t		chunk_size;
		size_t		chunk_received;
		size_t		max_body_size;
		HttpRequest	request;

		bool	parse_request_line(std::string& buffer);
		bool	parse_headers(std::string& buffer);
		bool	parse_body(std::string& buffer);

		bool		set_error(ErrorCode code);

		///// request-line helpers
		bool	valid_method(const std::string& method) const;
		bool	valid_target(const std::string& target) const;

		///// headers helpers
		std::string	trim_ows(const std::string& str) const;
		std::string	to_lower(const std::string& str) const;
		bool		is_token_char(char c) const;
		bool		valid_header_name(const std::string& name) const;
		bool		valid_header_value(const std::string& value) const;
		bool		validate_request_headers();
		bool		parse_host_header(const std::string& value);
		bool		valid_host_name(const std::string& value) const;
		bool		valid_port(const std::string& value) const;
		bool		valid_ip_literal(const std::string& value) const;

		///// body content-length
		std::vector<std::string>	split(const std::string& str, char del) const;
		bool						parse_size_value(const std::string& value, size_t& result) const;
		bool						parse_content_length(const std::string& value);
		bool						determine_body_framing();

		///// body chunks parsing
		bool	parse_hex_size(const std::string& value, size_t& result) const;
		bool	parse_chunk_size(std::string& buffer);
		bool	parse_chunk_data(std::string& buffer);
		bool	parse_chunk_trailers(std::string& buffer);
		bool	valid_transfer_encoding(const std::string& value) const;

	public:
		HttpParser(size_t max_body_size);
		~HttpParser();

		void	parse(std::string& buffer);
		void	reset();

		bool	is_complete() const;
		bool	has_error() const;
		ErrorCode	get_error_code() const;

		State	get_state() const;
		const HttpRequest&	get_request() const;
};
