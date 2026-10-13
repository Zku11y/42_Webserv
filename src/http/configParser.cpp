#include "../../inc/http/configParser.hpp"
#include "../../inc/http/config.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <set>

ConfigParser::ConfigParser(const std::string& filename)
	: _filename(filename), _pos(0){
	_locationHandlers["methods"] = &ConfigParser::parseMethods;
	_locationHandlers["root"] = &ConfigParser::parseRoot;
	_locationHandlers["index"] = &ConfigParser::parseIndex;
	_locationHandlers["autoindex"] = &ConfigParser::parseAutoindex;
	_locationHandlers["upload_enable"] = &ConfigParser::parseUploadEnable;
	_locationHandlers["upload_store"] = &ConfigParser::parseUploadStore;
	_locationHandlers["return"] = &ConfigParser::parseReturn;
	_locationHandlers["cgi_ext"] = &ConfigParser::parseCgiExt;
	
	//////////
	//////////
	//////////
	//////////

	_serverHandlers["listen"] = &ConfigParser::parseListen;
	_serverHandlers["host"] = &ConfigParser::parseHost;
	_serverHandlers["server_name"] = &ConfigParser::parseServerName;
	_serverHandlers["client_max_body_size"] = &ConfigParser::parseClientMaxBodySize;
	_serverHandlers["error_page"] = &ConfigParser::parseErrorPage;
}


////////////
////////////
////////////
////////////

void	ConfigParser::parseMethods(LocationConfig& location){
	expect("methods");
	if (!hasMoreTokens())
		throw std::runtime_error("Expected methods");
	std::vector<std::string> methods;
	while (hasMoreTokens() && currentToken() != ";"){
		methods.push_back(currentToken());
		advance();
	}
	if (!hasMoreTokens())
		throw std::runtime_error("Missing ';' after methods");
	if (methods.empty())
		throw std::runtime_error("methods requires at least one method");
	location.setMethods(methods);
	expect(";");
}

void	ConfigParser::parseRoot(LocationConfig& location){
	expect("root");
	if (!hasMoreTokens())
		throw std::runtime_error("Expected root path");
	location.setRoot(currentToken());
	advance();
	expect(";");
}

void	ConfigParser::parseIndex(LocationConfig& location){
	expect("index");
	if (!hasMoreTokens())
		throw std::runtime_error("Expected index path");
	location.setIndex(currentToken());
	advance();
	expect(";");
}

void	ConfigParser::parseAutoindex(LocationConfig& location){
	expect("autoindex");
	if (!hasMoreTokens())
		throw std::runtime_error("Expected autoindex state");
	if (currentToken() == "on")
		location.setAutoindex(true);
	else if (currentToken() == "off")
		location.setAutoindex(false);
	else
		throw std::runtime_error("autoindex must be 'on' or 'off'");
	advance();
	expect(";");
}

void	ConfigParser::parseUploadEnable(LocationConfig& location){
	expect("upload_enable");
	if (!hasMoreTokens())
		throw std::runtime_error("Expected upload_enable state");
	if (currentToken() == "on")
		location.setUploadEnable(true);
	else if (currentToken() == "off")
		location.setUploadEnable(false);
	else
		throw std::runtime_error("uploadenable must be 'on' or 'off'");
	advance();
	expect(";");
}

void	ConfigParser::parseUploadStore(LocationConfig& location){
	expect("upload_store");
	if (!hasMoreTokens())
		throw std::runtime_error("Expected upload_store path");
	location.setUploadStore(currentToken());
	advance();
	expect(";");
}

void	ConfigParser::parseReturn(LocationConfig& location){
	expect("return");
	if (!hasMoreTokens())
		throw std::runtime_error("Expected return path");
	location.setReturnCode(strtoi(currentToken()));
	advance();
	if (!hasMoreTokens() || currentToken() == ";")
		throw std::runtime_error("Expected return target");
	location.setReturnTarget(currentToken());
	advance();
	expect(";");
}

void	ConfigParser::parseCgiExt(LocationConfig& location){
	expect("cgi_ext");
	if (!hasMoreTokens() || currentToken() == ";")
		throw std::runtime_error("Expected CGI extension");
	std::string extention = currentToken();
	advance();
	if (!hasMoreTokens() || currentToken() == ";")
		throw std::runtime_error("Expected CGI interpreter");
	std::string	interpreter = currentToken();
	advance();
	expect(";");
	location.addCgi(extention, interpreter);
}

////////////
////////////
////////////
////////////

int	ConfigParser::strtoi(const std::string& value){
	std::stringstream	s;
	int					nm;
	char				extra;

	s << value;
	if (!(s >> nm))
		throw std::runtime_error("Invalid number: " + value);
	if (s >> extra)
		throw std::runtime_error("Invalid number: " + value);
	return nm;
}

void	ConfigParser::parseListen(ServerConfig& server){
	expect("listen");
	if (!hasMoreTokens() || currentToken() == ";")
		throw std::runtime_error("Expected port");
	int	port = strtoi(currentToken());
	if (port < 1 || port > 65535)
		throw std::runtime_error("Port out of range");
	server.setPort(port);
	advance();
	expect(";");
}

void	ConfigParser::parseHost(ServerConfig& server){
	expect("host");
	if (!hasMoreTokens() || currentToken() == ";")
		throw std::runtime_error("Expected host");
	server.setHost(currentToken());
	advance();
	expect(";");
}

void	ConfigParser::parseServerName(ServerConfig& server){
	expect("server_name");
	if (!hasMoreTokens() || currentToken() == ";")
		throw std::runtime_error("Expected server name");
	server.setServerName(currentToken());
	advance();
	expect(";");
}

size_t	ConfigParser::parseSize(const std::string& value){
	size_t				multiplayer;
	size_t				number;
	std::string			numberpart;
	char				suffix;
	char				extra;
	std::stringstream	s;
	if (value.empty())
		throw std::runtime_error("Invalid body size");
	multiplayer = 1;
	suffix = value[value.size() - 1];
	if (suffix == 'K' || suffix == 'k')
		multiplayer = static_cast<size_t>(1024);
	else if (suffix == 'M' || suffix == 'm')
		multiplayer = static_cast<size_t>(1024) * 1024;
	else if (suffix == 'G' || suffix == 'g')
		multiplayer = static_cast<size_t>(1024) * 1024 * 1024;
	else if (!std::isdigit(static_cast<unsigned char>(suffix)))
		throw std::runtime_error("Invalid body size suffix");
	if (std::isdigit(static_cast<unsigned char>(suffix)))
		numberpart = value;
	else
		numberpart = value.substr(0, value.size() - 1);
	if (numberpart.empty())
		throw std::runtime_error("Missing number in body size");
	for (size_t i = 0; i < numberpart.size(); i++){
		if (!std::isdigit(static_cast<unsigned char>(numberpart[i])))
			throw std::runtime_error("Invalid body size: " + value);
	}
	
	s.str(numberpart);
	if (!(s >> number))
		throw std::runtime_error("Invalid body size: " + value);
	if (s >> extra)
		throw std::runtime_error("Invalid body size: " + value);
	if (number > std::numeric_limits<size_t>::max() / multiplayer)
		throw std::runtime_error("Body size is too large");
	return number * multiplayer;
}

void	ConfigParser::parseClientMaxBodySize(ServerConfig& server){
	expect("client_max_body_size");
	if (!hasMoreTokens() || currentToken() == ";")
		throw std::runtime_error("Expected client max body size");
	size_t	size = parseSize(currentToken());
	server.setClientMaxBodySize(size);
	advance();
	expect(";");
}

void	ConfigParser::parseErrorPage(ServerConfig& server){
	expect("error_page");
	if (!hasMoreTokens() || currentToken() == ";")
		throw std::runtime_error("Expected error status code");
	int code = strtoi(currentToken());
	advance();
	if (!hasMoreTokens() || currentToken() == ";")
		throw std::runtime_error("Expected error page path");
	std::string	path = currentToken();
	advance();
	expect(";");
	server.addErrorPage(code, path);
}


////////////
////////////
////////////
////////////


ConfigParser::~ConfigParser(){
}

std::string	ConfigParser::currentToken() const{
	if (_pos >= _tokens.size())
		throw std::runtime_error("unexpected end of config");
	return _tokens[_pos];
}

void	ConfigParser::advance(){
	if (_pos < _tokens.size())
		_pos++;
}

bool	ConfigParser::hasMoreTokens() const{
	return _pos < _tokens.size();
}

void ConfigParser::readFile(){
	std::ifstream file(_filename.c_str());

	if (!file.is_open())
		throw std::runtime_error("Could not open config file");
	std::stringstream buffer;
	buffer << file.rdbuf();
	_content = buffer.str();
}

void	ConfigParser::tokenize(){
	std::string	token;
	size_t		a = 0;

	while (a < _content.size()){
		if (_content[a] == '#'){
			if (!token.empty()){
				_tokens.push_back(token);
				token.clear();
			}
			while (a < _content.size() && _content[a] != '\n')
				a++;
		}
		else if (std::isspace(static_cast<unsigned char>(_content[a]))){
			if (!token.empty()){
				_tokens.push_back(token);
				token.clear();
			}
		}
		else if (_content[a] == '{' || _content[a] == '}' || _content[a] == ';'){
			if (!token.empty()){
				_tokens.push_back(token);
				token.clear();
			}
			_tokens.push_back(std::string(1, _content[a]));
		}
		else
			token += _content[a];
		a++;
	}
	if (!token.empty())
		_tokens.push_back(token);
}

void ConfigParser::expect(const std::string& token){
	if (!hasMoreTokens())
		throw std::runtime_error("Unexpected end of config");
	if (currentToken() != token)
		throw std::runtime_error("Expected '"+token+"', got '"+currentToken()+"'");
	advance();
}

void	ConfigParser::parseLocation(ServerConfig& server){
	LocationConfig	location;
	std::set<std::string> seen;
	expect("location");
	if (!hasMoreTokens())
		throw std::runtime_error("Expected location paath");
	location.setPath(currentToken());
	advance();
	expect("{");
	while (hasMoreTokens() && currentToken() != "}"){
		if (currentToken() != "cgi_ext"){
			if (seen.find(currentToken()) != seen.end())
				throw std::runtime_error("Duplicate location directive: " + currentToken());
			seen.insert(currentToken());
		}
		parseLocationDirective(location);
	}
	if (!hasMoreTokens())
		throw std::runtime_error("Unclosed location block");
	expect("}");
	server.addLocation(location);
}

void	ConfigParser::parseLocationDirective(LocationConfig& location){
	std::map<std::string, LocationHandler>::iterator	it;

	it = _locationHandlers.find(currentToken());
	if (it == _locationHandlers.end())
		throw std::runtime_error("Unknown location directive: " + currentToken());
	(this->*(it->second))(location);
}

void	ConfigParser::parseServerDirective(ServerConfig& server){
	std::map<std::string, ServerHandler>::iterator it;
	it = _serverHandlers.find(currentToken());
	if (it == _serverHandlers.end())
		throw std::runtime_error("Unknown server directive");
	(this->*(it->second))(server);
}

void	ConfigParser::parseServer(Config& config){
	ServerConfig	server;
	std::set<std::string>	seen;
	expect("server");
	expect("{");
	while (hasMoreTokens() && currentToken() != "}"){
		if (currentToken() == "location")
			parseLocation(server);
		else{
			if (currentToken() != "error_page"){
				if (seen.find(currentToken()) != seen.end())
					throw std::runtime_error("Duplicate server directive: " + currentToken());
				seen.insert(currentToken());
			}
			parseServerDirective(server);
		}
	}
	if (!hasMoreTokens())
		throw std::runtime_error("Unclosed server block");
	expect("}");
	config.addServer(server);
}

void	ConfigParser::parseConfig(Config& config){
	while (hasMoreTokens()){
		if (currentToken() != "server")
			throw std::runtime_error("Expected 'server' block!");
		parseServer(config);
	}	
}

void ConfigParser::parse(Config& config){
	readFile();
	tokenize();
	parseConfig(config);
}