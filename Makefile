# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/03/19 10:16:05 by copireyr          #+#    #+#              #
#    Updated: 2025/03/19 20:37:35 by upolat           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

.DEFAULT_GOAL := all
CC := c++
CFLAGS := -Wall -Wextra -Werror -std=c++20 # -MMD -MP
NAME := webserv

src_files := main.cpp parser.cpp

src = $(addprefix ./src/, $(src_files))
obj := $(addprefix ./obj/, $(src:%.cpp=%.o))

./obj/%.o: %.cpp
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

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
	./$(NAME) config_file.txt

-include $(obj:.o=.d)
