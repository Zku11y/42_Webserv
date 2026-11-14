NAME    = webserv

CXX     = c++
# CXXFLAGS = -Wall -Wextra  -std=c++98
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS    = src/http/http.cpp src/http/http_helper.cpp src/http/configParser.cpp src/http/config.cpp src/core/server.cpp

OBJS    = $(SRCS:.cpp=.o)

HEADERS = inc/http/config.hpp inc/http/configParser.hpp inc/core/server.hpp inc/http/http.hpp inc/http/http_helper.hpp

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
