# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/03/19 10:16:05 by copireyr          #+#    #+#              #
#    Updated: 2025/05/26 14:56:33 by copireyr         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

.DEFAULT_GOAL := all
CC := c++
CFLAGS := -Wall -Wextra -Werror -MMD -MP -std=c++20
CFLAGS += -Wimplicit-fallthrough -Wshadow -Wswitch-enum
# debug := -O0 -DDEBUG -g3
opt := -O2
CPPFLAGS := -I./include/ $(debug) $(opt)
NAME := webserv

src_files := CgiHandler.cpp Configuration.cpp Parser.cpp HttpConnectionHandler.cpp HttpConnectionHandler_CGI.cpp HttpConnectionHandler_Parsing.cpp HttpConnectionHandler_Response.cpp HttpConnectionHandler_MSG.cpp Logger.cpp main.cpp Queue.cpp Server.cpp Socket.cpp Client.cpp HttpConnectionHandler_Post.cpp
NAME := webserv

src = $(addprefix ./src/, $(src_files))
obj := $(addprefix ./obj/, $(src:%.cpp=%.o))


./obj/%.o: %.cpp
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(NAME): $(obj)
	$(CC) $(obj) -o $@

all: $(NAME)

.PHONY: clean
clean:
	$(RM) -r ./obj/

.PHONY: fclean
fclean: clean
	$(RM) $(NAME)

.PHONY: re
re: fclean
	make -j

.PHONY: test
test: all
	./$(NAME) test.conf

.PHONY: val
val: all
	rm -f valgrind.log
	valgrind -s --leak-check=full --show-leak-kinds=all --track-fds=yes --log-file=valgrind.log ./webserv test.conf || true
	cat valgrind.log

.PHONY: pytest
pytest: all
	python3 -m pytest -v python_unit_tests.py

-include $(obj:.o=.d)
