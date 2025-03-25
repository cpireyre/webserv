/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/25 13:46:50 by upolat           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include "../include/Parser.hpp"

int parser(std::string fileName);

int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cout << "./webserv [configuration file]\n";
		return (1);
	}
	parser(argv[1]);
	return (0);
}
