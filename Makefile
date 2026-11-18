CC = c++

CFLAGS = -Wall -Werror -Wextra -std=c++98 -g -fsanitize=address
# CFLAGS =  -std=c++98

SRC = 	src/core/server.cpp\
		src/http/config.cpp\
		src/http/configParser.cpp\
		src/http/http.cpp\
		src/http/http_helper.cpp\
		src/cgi_io/Execution.cpp

OBJ = $(SRC:.cpp=.o)

HEAD = 	inc/core/server.hpp\
		inc/http/config.hpp\
		inc/http/configParser.hpp\
		inc/http/http.hpp\
		inc/cgi_io/Execution.hpp

NAME = sr

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME)
	
%.o: %.cpp $(HEAD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ)

fclean: clean
	rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re

