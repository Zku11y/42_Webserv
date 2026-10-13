#include "../../inc/http/config.hpp"
#include "../../inc/http/configParser.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <set>


LocationConfig::LocationConfig() : _autoindex(false), _uploadEnable(false), _returnCode(0){}
LocationConfig::~LocationConfig(){}

void	LocationConfig::validateMethods() const{
	if (_methods.empty())
		throw std::runtime_error("Location has no allowed HTTP methods");
	std::set<std::string>	seen;
	for (size_t i = 0; i < _methods.size(); i++){
		if (_methods[i] != "GET" && _methods[i] != "POST" && _methods[i] != "DELETE")
			throw std::runtime_error("Invalid HTTP method: "+ _methods[i]);
		if (seen.find(_methods[i]) != seen.end())
			throw std::runtime_error("Duplicate HTTP method: " + _methods[i]);
		seen.insert(_methods[i]);
	}
	// if (getPath() == "/" && _methods.size() != 1)
	// 	throw std::runtime_error("Root location can only have GET as HTTP methods");
}

void	LocationConfig::validateReturn() const{
	if (_returnCode == 0)
		return;
	if (_returnCode != 301 && _returnCode != 302)
		throw std::runtime_error("Invalid redirect status code");
	if (_returnTarget.empty())
		throw std::runtime_error("Redirect target cannot be empty");
}

void	LocationConfig::validateUpload() const{
	if (_uploadEnable && _uploadStore.empty())
		throw std::runtime_error("Upload_enable is on but upload_store is missing");
	if (!_uploadEnable && !_uploadStore.empty())
		throw std::runtime_error("upload_store set while uploads are disabled");
}

void	LocationConfig::validateCgi() const{
	std::map<std::string, std::string>::const_iterator	it = _cgi.begin();
	while (it != _cgi.end()){
		if (it->first.empty() || it->first[0] != '.')
			throw std::runtime_error("CGI extension must start with a '.'");
		if (it->second.empty())
			throw std::runtime_error("CGI interpreter cannot be empty");
		it++;
	}
}

void	LocationConfig::validate() const{
	if (_path.empty() || _path[0] != '/')
		throw std::runtime_error("Location must start with '/'");
	validateMethods();
	validateReturn();
	validateUpload();
	validateCgi();
	if (_returnCode == 0 && _root.empty())
		throw std::runtime_error("Location requires root unless it redirects");
}


////////
//////// Locationconfig   setters

void LocationConfig::setPath(const std::string& path){
	_path = path;
}

void	LocationConfig::setMethods(const std::vector<std::string>& methods){
	_methods = methods;
}

void LocationConfig::setRoot(const std::string& root){
	_root = root;
}

void LocationConfig::setIndex(const std::string& index){
	_index = index;
}

void	LocationConfig::setAutoindex(const bool autoindex){
	_autoindex = autoindex;
}

void	LocationConfig::setUploadEnable(const bool uploadEnable){
	_uploadEnable = uploadEnable;
}

void	LocationConfig::setUploadStore(const std::string& uploadStore){
	_uploadStore = uploadStore;
}

void	LocationConfig::setReturnCode(const int returnCode){
	_returnCode = returnCode;
}

void	LocationConfig::setReturnTarget(const std::string& returntarget){
	_returnTarget = returntarget;
}

void	LocationConfig::addCgi(const std::string& extension, const std::string& interpreter){
	if (_cgi.find(extension) != _cgi.end())
		throw std::runtime_error("Duplicate cgi_ext: " + extension);
	_cgi[extension] = interpreter;
}


////////
//////// Locationconfig   getters


const std::string&	LocationConfig::getPath() const{
	return _path;
}

const std::vector<std::string>&	LocationConfig::getMethods() const{
	return _methods;
}

const std::string&	LocationConfig::getRoot() const{
	return _root;
}

const std::string&	LocationConfig::getIndex() const{
	return _index;
}

bool	LocationConfig::getAutoindex() const{
	return _autoindex;
}

bool	LocationConfig::getUploadEnable() const{
	return _uploadEnable;
}

const std::string&	LocationConfig::getUploadStore() const{
	return _uploadStore;
}

int	LocationConfig::getReturnCode() const{
	return _returnCode;
}

const std::string&	LocationConfig::getReturnTarget() const{
	return _returnTarget;
}

const std::map<std::string, std::string>&	LocationConfig::getCGI() const{
	return _cgi;
}

////////
////////



ServerConfig::ServerConfig() : _port(0), _clientMaxBodySize(0) {}
ServerConfig::~ServerConfig(){}

void	ServerConfig::validateLocations() const{
	std::set<std::string>	seen;
	for (size_t i = 0; i < _locations.size(); i++){
		_locations[i].validate();
		if (seen.find(_locations[i].getPath()) != seen.end())
			throw std::runtime_error("Duplicate location: " + _locations[i].getPath());
		seen.insert(_locations[i].getPath());
	}
	
}

void	ServerConfig::validate() const{
	if (_port == 0)
		throw std::runtime_error("Server is missing listen directive");
	if (_host.empty())
		throw std::runtime_error("Server is missing host directive");
	validateLocations();
}

////////
//////// Serverconfig   setters

void	ServerConfig::setPort(int port){
	_port = port;
}

void	ServerConfig::setHost(const std::string& host){
	_host = host;
}

void	ServerConfig::setServerName(const std::string& name){
	_serverName = name;
}

void	ServerConfig::setClientMaxBodySize(size_t size){
	_clientMaxBodySize = size;
}

void	ServerConfig::addErrorPage(int code, const std::string& path){
	if (_errorPages.find(code) != _errorPages.end()){
		std::stringstream	s;
		s << "Duplicate error_page_code: " << code;
		throw std::runtime_error(s.str());
	}
	_errorPages[code] = path;
}



////////
//////// Serverconfig   getters


int	ServerConfig::getPort() const{
	return _port;
}

const std::string&	ServerConfig::getHost() const{
	return _host;
}

const std::string&	ServerConfig::getServerName() const{
	return _serverName;
}

size_t	ServerConfig::getClientMaxBodySize() const{
	return _clientMaxBodySize;
}

const std::map<int, std::string>&	ServerConfig::getErrorPages() const{
	return _errorPages;
}

const std::vector<LocationConfig>&	ServerConfig::getLocations() const{
	return _locations;
}


////////
////////


void ServerConfig::addLocation(const LocationConfig& location){
	_locations.push_back(location);
}

void	Config::validate(){
	std::set<std::pair<std::string, int> >	seen;
	if (_servers.empty())
		throw std::runtime_error("Configuration contains no server");
	for (size_t i = 0; i < _servers.size(); i++){
		_servers[i].validate();
		std::pair<std::string, int> endpoint(_servers[i].getHost(), _servers[i].getPort());
			if (seen.find(endpoint) != seen.end())
			throw std::runtime_error("Duplicate server host:port");
		seen.insert(endpoint);
	}
}


Config::Config(const std::string& filename){
	ConfigParser parser(filename);
	parser.parse(*this);
	validate();
}

Config::~Config(){}


void Config::addServer(const ServerConfig& server){
	_servers.push_back(server);
}

const std::vector<ServerConfig>&	Config::getServers() const{
	return _servers;
}
