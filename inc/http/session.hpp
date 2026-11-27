// #pragma once

// #include <string>
// #include <map>
// #include <cstddef>
// #include <cstdlib>
// #include <ctime>
// #include <sstream>
// #include "http.hpp"

// struct	Session{
// 	std::string	id;
// 	size_t		visits;

// 	Session() : visits(0) {}
// };

// class	sessionManager{
// 	private:
// 		std::map<std::string, Session>	sessions;

// 		std::string	generateSessionId();

// 	public:
// 		sessionManager();
// 		~sessionManager();

// 		Session&	createSession();
// 		Session*	findSession(const std::string& id);
// 		Session&	retreiveSession(const HttpRequest& request, bool& created);
// };

