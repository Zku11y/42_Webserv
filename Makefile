CC = c++

# CFLAGS = -Wall -Werror -Wextra -std=c++98
CFLAGS =  -std=c++98

SRC = main.cpp src/http/config.cpp src/http/configParser.cpp src/core/core.cpp src/cgi_io/cgi.cpp 

OBJ = $(SRC:.cpp=.o)

HEAD = inc/http/config.hpp inc/http/configParser.hpp inc/main.hpp

NAME = sr

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME) && echo "" && ./sr default.conf

%.o: %.cpp $(HEAD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ)

fclean: clean
	rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re

