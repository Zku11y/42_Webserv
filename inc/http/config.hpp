#pragma once

#include <string>
#include <vector>
#include <map>

class LocationConfig
{
	private:
		std::string					_path;
		std::vector<std::string>	_methods;
		std::string					_root;
		std::string					_index;
		bool						_autoindex;

		bool						_uploadEnable;
		std::string					_uploadStore;

		int							_returnCode;
		std::string					_returnTarget;

		std::map<std::string, std::string>	_cgi;

		void	validateMethods() const;
		void	validateReturn() const;
		void	validateUpload() const;
		void	validateCgi() const;

	public:
		LocationConfig();
		~LocationConfig();

		void	validate() const;

		const std::string&							getPath() const;
		const std::vector<std::string>&				getMethods() const;
		const std::string&							getRoot() const;
		const std::string&							getIndex() const;
		bool										getAutoindex() const;
		bool										getUploadEnable() const;
		const std::string&							getUploadStore() const;
		int											getReturnCode() const;
		const std::string&							getReturnTarget() const;
		const std::map<std::string, std::string>&	getCGI() const;
	

		void	setPath(const std::string& path);
		void	setMethods(const std::vector<std::string>& methods);
		void	setRoot(const std::string& root);
		void	setIndex(const std::string& index);
		void	setAutoindex(const bool autoindex);

		void	setUploadEnable(const bool uploadEnable);
		void	setUploadStore(const std::string& uploadStore);

		void	setReturnCode(const int returnCode);
		void	setReturnTarget(const std::string& returntarget);

		void	addCgi(const std::string& extension, const std::string& interpreter);
};

class	ServerConfig
{
	private:
		int							_port;
		std::string					_host;
		std::string					_serverName;
		size_t						_clientMaxBodySize;

		std::map<int, std::string>	_errorPages;
		std::vector<LocationConfig>	_locations;

	public:
		ServerConfig();
		~ServerConfig();

		void		validateLocations() const;
		void		validate() const;

		int									getPort() const;
		const std::string&					getHost() const;
		const std::string&					getServerName() const;
		size_t								getClientMaxBodySize() const;
		const std::map<int, std::string>&	getErrorPages() const;
		const std::vector<LocationConfig>&	getLocations() const;

		
		void	setPort(int port);
		void	setHost(const std::string& host);
		void	setServerName(const std::string& name);
		void	setClientMaxBodySize(size_t size);
		void	addErrorPage(int code, const std::string& path);
		void	addLocation(const LocationConfig& location);
};

class Config
{
	private:
		std::vector<ServerConfig>	_servers;
	public:
		Config(const std::string& filename);
		~Config();

		const std::vector<ServerConfig>&	getServers() const;
		
		void		addServer(const ServerConfig& server);
		void		validate();
};

