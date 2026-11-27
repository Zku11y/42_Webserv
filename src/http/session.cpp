// #include "../../inc/http/session.hpp"

// sessionManager::sessionManager() {
// 	std::srand(std::time(NULL));
// }
// sessionManager::~sessionManager() {}

// #include <fcntl.h>
// #include <unistd.h>
// #include <stdexcept>

// std::string	f_generate_session_id()
// {
// 	unsigned char	buffer[16];
// 	const char		hex[] = "0123456789abcdef";
// 	std::string		id;
// 	size_t			total;
// 	ssize_t			bytes;
// 	int				fd;

// 	fd = open("/dev/urandom", O_RDONLY);
// 	if (fd < 0)
// 		return "";
// 	total = 0;
// 	while (total < sizeof(buffer))
// 	{
// 		bytes = read(fd, buffer + total, sizeof(buffer) - total);
// 		if (bytes <= 0)
// 		{
// 			close(fd);
// 			return "";
// 		}
// 		total += static_cast<size_t>(bytes);
// 	}
// 	close(fd);
// 	for (size_t i = 0; i < sizeof(buffer); i++)
// 	{
// 		id += hex[(buffer[i] >> 4) & 0x0F];
// 		id += hex[buffer[i] & 0x0F];
// 	}
// 	return id;
// }

// std::string	sessionManager::generateSessionId(){
// 	std::stringstream	ss;
// 	const char			hex[] = "0123456789abcdef";
// 	std::string			id;

// 	while (true)
// 	{
// 		ss.str("");
// 		ss.clear();
// 		for (int i = 0; i < 32; i++)
// 			ss << hex[std::rand() % 16];
// 		id = ss.str();
// 		if (sessions.find(id) == sessions.end())
// 			return id;
// 	}
// }

// Session&	sessionManager::createSession(){
// 	Session		session;
// 	std::string	id;

// 	id = generateSessionId();
// 	session.id = id;
// 	sessions[id] = session;
// 	return sessions[id];
// }

// Session*	sessionManager::findSession(const std::string& id){
// 	std::map<std::string, Session>::iterator	it;

// 	it = sessions.find(id);
// 	if (it == sessions.end())
// 		return NULL;
// 	return &it->second;
// }

// Session&	sessionManager::retreiveSession(const HttpRequest& request, bool& created){
// 	std::map<std::string, std::string>::const_iterator	it;
// 	Session*										session;

// 	created = false;
// 	it = request.cookies.find("session_id");
// 	if (it != request.cookies.end() && !it->second.empty()){
// 		session = findSession(it->second);
// 		if (session != NULL)
// 			return *session;
// 	}
// 	created = true;
// 	return createSession();
// }
