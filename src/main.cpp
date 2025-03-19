/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/19 20:34:56 by upolat           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include "parser.hpp"

int	main(int argc, char **argv)
{
	// if (argc != 2)
	// {
	// 	std::cout << "./webserv [configuration file]\n";
	// 	return (1);
	// }
	// std::cout << "Hello from webserv\n";
	// std::cout << "We'll want to parse " << argv[1] << "\n";
	(void)argc;
	(void)argv;
	parser();
	return (0);
}
