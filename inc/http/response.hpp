#pragma once

#include <string>
#include <iostream>
#include <map>
#include <sstream>

struct HttpResponse
{
	int									status_code;
	std::map<std::string, std::string>	headers;
	std::string							body;

	int									body_fd;
	size_t								body_size;
};

class	HttpResponseBuilder{
	private:
		std::map<int, std::string>	status_message;

		std::string	gen_phrase(int status_code) const;
		std::string	to_lower(const std::string& str) const;
		void		erase_header(HttpResponse& response, const std::string& name) const;
		bool		valid_header_name(const std::string& name) const;
		bool		is_token_char(char c) const;
		bool		valid_header_value(const std::string& value) const;

	public:
		HttpResponseBuilder();
		~HttpResponseBuilder();

		std::string	build(HttpResponse&	response);
};