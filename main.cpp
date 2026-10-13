#include "inc/main.hpp"

void	printLocation(const LocationConfig& location)
{
	const std::vector<std::string>& methods = location.getMethods();
	const std::map<std::string, std::string>& cgi = location.getCGI();
	size_t i;

	std::cout << "\tlocation " << location.getPath() << " {" << std::endl;

	std::cout << "\t\tmethods";
	i = 0;
	while (i < methods.size())
	{
		std::cout << " " << methods[i];
		i++;
	}
	std::cout << ";" << std::endl;

	if (!location.getRoot().empty())
		std::cout << "\t\troot " << location.getRoot() << ";" << std::endl;

	if (!location.getIndex().empty())
		std::cout << "\t\tindex " << location.getIndex() << ";" << std::endl;

	std::cout << "\t\tautoindex "
		<< (location.getAutoindex() ? "on" : "off")
		<< ";" << std::endl;

	std::cout << "\t\tupload_enable "
		<< (location.getUploadEnable() ? "on" : "off")
		<< ";" << std::endl;

	if (!location.getUploadStore().empty())
		std::cout << "\t\tupload_store "
			<< location.getUploadStore() << ";" << std::endl;

	if (location.getReturnCode() != 0)
	{
		std::cout << "\t\treturn "
			<< location.getReturnCode() << " "
			<< location.getReturnTarget()
			<< ";" << std::endl;
	}

	std::map<std::string, std::string>::const_iterator it;
	it = cgi.begin();
	while (it != cgi.end())
	{
		std::cout << "\t\tcgi_ext "
			<< it->first << " "
			<< it->second << ";"
			<< std::endl;
		++it;
	}

	std::cout << "\t}" << std::endl;
}

void	printConfig(const Config& config)
{
	const std::vector<ServerConfig>& servers = config.getServers();
	size_t i;

	i = 0;
	while (i < servers.size())
	{
		const ServerConfig& server = servers[i];

		std::cout << "server {" << std::endl;

		std::cout << "\tlisten "
			<< server.getPort() << ";" << std::endl;

		std::cout << "\thost "
			<< server.getHost() << ";" << std::endl;

		if (!server.getServerName().empty())
			std::cout << "\tserver_name "
				<< server.getServerName() << ";" << std::endl;

		std::cout << "\tclient_max_body_size "
			<< server.getClientMaxBodySize()
			<< " bytes;" << std::endl;

		const std::map<int, std::string>& errors =
			server.getErrorPages();

		std::map<int, std::string>::const_iterator error;
		error = errors.begin();
		while (error != errors.end())
		{
			std::cout << "\terror_page "
				<< error->first << " "
				<< error->second << ";"
				<< std::endl;
			++error;
		}

		const std::vector<LocationConfig>& locations =
			server.getLocations();

		size_t j = 0;
		while (j < locations.size())
		{
			std::cout << std::endl;
			printLocation(locations[j]);
			j++;
		}

		std::cout << "}" << std::endl;

		if (i + 1 < servers.size())
			std::cout << std::endl;

		i++;
	}
}

int main(int ac, char **av)
{
	if (ac != 2){
		std::cerr << "Usage: ./webserv <config>" << std::endl;
		return 1;
	}

	try{
        Config  config(av[1]);
		// printConfig(config);

		ft_core(config);
		
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
