NAME    = webserv

CXX     = c++
# CXXFLAGS = -Wall -Wextra  -std=c++98
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS    = main.cpp src/http/config.cpp src/http/configParser.cpp
OBJS    = $(SRCS:.cpp=.o)

HEADERS = inc/http/config.hpp inc/http/configParser.hpp

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