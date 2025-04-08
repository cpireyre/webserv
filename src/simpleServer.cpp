#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "../include/Parser.hpp"
#include "../include/HttpConnectionHandler.hpp"

#define PORT 8080

std::vector<Configuration> serverMap;

void startServer(int port, int ac, char **av) {
    if (ac != 2)
	{
		std::cout << "./webserv [configuration file]\n";
		return;
	}

	serverMap = parser(av[1]);
	if (serverMap.size() < 1)
		return ;
	serverMap[0].printServerBlock();


    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Error: Failed to create socket\n";
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error: Binding failed\n";
        close(serverSocket);
        return;
    }

    if (listen(serverSocket, 10) < 0) {
        std::cerr << "Error: Listening failed\n";
        close(serverSocket);
        return;
    }


    std::cout << "Server is running on port " << port << "...\n";

    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            std::cerr << "Error: Failed to accept connection\n";
            continue;
        }

        std::cout << "Client connected\n";

        HttpConnectionHandler handler;
        handler.setClientSocket(clientSocket);
        handler.setConfig(&serverMap[0]);
        if (handler.parseRequest()) {
            handler.handleRequest();
        } else {
            std::string errorResponse = handler.createHttpErrorResponse(handler.getErrorCode());
            send(clientSocket, errorResponse.c_str(), errorResponse.size(), 0);
        }

        close(clientSocket);
        std::cout << "Client disconnected\n";
    }

    close(serverSocket);
}

int main(int ac, char **av)
{
    startServer(PORT, ac, av);
    return 0;
}

