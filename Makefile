# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/03/19 10:16:05 by copireyr          #+#    #+#              #
#    Updated: 2025/03/27 08:57:39 by copireyr         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

.DEFAULT_GOAL := all
CC := c++
CFLAGS := -Wall -Wextra -Werror -MMD -MP -std=c++20
debug := -DDEBUG
CPPFLAGS := -I./include/ $(debug)
NAME := webserv

src_files := main.cpp Logger.cpp Socket.cpp Connection.cpp Configuration.cpp Parser.cpp Server.cpp
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
re: fclean all

.PHONY: test
test: all
	./test.sh

-include $(obj:.o=.d)
