NAME    = webserv

CXX     = c++
# CXXFLAGS = -Wall -Wextra  -std=c++98
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS    = main.cpp src/http/config.cpp src/http/configParser.cpp\
		  src/http/http.cpp src/http/http_helper.cpp\
		  src/http/response.cpp
OBJS    = $(SRCS:.cpp=.o)

HEADERS = inc/http/config.hpp inc/http/configParser.hpp inc/http/http.hpp\
		  inc/http/response.hpp

TEST_SRC = tests/http_response_test.cpp
RESPONSE_SRCS = src/http/response.cpp
TEST_BIN = tests/http_response_test
SANITIZE_BIN = tests/http_response_test_sanitize

PARSER_TEST_SRC = tests/http_chunk_edges.cpp
PARSER_SRCS = src/http/http.cpp src/http/http_helper.cpp
PARSER_TEST_BIN = tests/http_chunk_edges
PARSER_SANITIZE_BIN = tests/http_chunk_edges_sanitize

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TEST_BIN) $(SANITIZE_BIN) $(PARSER_TEST_BIN) $(PARSER_SANITIZE_BIN)

fclean: clean
	rm -f $(NAME)

re: fclean all

$(TEST_BIN): $(TEST_SRC) $(RESPONSE_SRCS) inc/http/response.hpp
	$(CXX) $(CXXFLAGS) $(TEST_SRC) $(RESPONSE_SRCS) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(SANITIZE_BIN): $(TEST_SRC) $(RESPONSE_SRCS) inc/http/response.hpp
	$(CXX) $(CXXFLAGS) -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(TEST_SRC) $(RESPONSE_SRCS) -o $@

test-sanitize: $(SANITIZE_BIN)
	./$(SANITIZE_BIN)

$(PARSER_TEST_BIN): $(PARSER_TEST_SRC) $(PARSER_SRCS) inc/http/http.hpp
	$(CXX) $(CXXFLAGS) $(PARSER_TEST_SRC) $(PARSER_SRCS) -o $@

test-parser: $(PARSER_TEST_BIN)
	./$(PARSER_TEST_BIN)

$(PARSER_SANITIZE_BIN): $(PARSER_TEST_SRC) $(PARSER_SRCS) inc/http/http.hpp
	$(CXX) $(CXXFLAGS) -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(PARSER_TEST_SRC) $(PARSER_SRCS) -o $@

test-parser-sanitize: $(PARSER_SANITIZE_BIN)
	./$(PARSER_SANITIZE_BIN)

.PHONY: all clean fclean re test test-sanitize test-parser test-parser-sanitize
