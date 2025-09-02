## 🔥 Our Amazing Team 
[Colin Pireyre](https://github.com/cpireyre) | [Uygar Polat](https://github.com/uygarpolat) | [Jere Hirvonen](https://github.com/Jerehirvonenn)

# Webserv – *"This is when you finally understand why a URL starts with HTTP"*

This project is a deep dive into the fundamentals of web communication. The goal is to build a functional, non-blocking **HTTP server** from the ground up in C++.

The server is capable of handling multiple client connections simultaneously, parsing HTTP requests, serving static files, processing CGI scripts, and managing different virtual servers based on a detailed configuration file. This project explores **low-level network programming, file descriptor management, and protocol implementation**.

- **Core logic** → uses `poll()` (or an equivalent like `kqueue`/`epoll`) for multiplexing I/O.
- **Configuration** → mimics the flexible structure of NGINX server blocks.

## 🧩 The Challenge
The primary challenge is to create a robust and resilient web server that can:

- **Handle multiple clients** concurrently without blocking, using a single thread.
- **Parse and validate** incoming HTTP/1.1 requests.
- **Route requests** to the correct server and location block based on the configuration.
- **Serve static content** such as HTML, CSS, and images.
- **Process dynamic content** by executing CGI scripts (e.g., PHP, Python).
- **Manage file uploads** via POST requests.
- **Respond with accurate status codes** and headers for various scenarios (e.g., `200 OK`, `404 Not Found`, `301 Moved Permanently`).
- **Remain stable** under load and gracefully handle errors without crashing.

## ⚙️ How It Works
- The server starts by parsing a **configuration file** that defines one or more virtual servers, each with its own port, host, and specific routes.
- It creates listening sockets for each specified port and adds them to a polling mechanism (`poll`).
- The main event loop waits for activity on any of the monitored file descriptors.
- **New Connections:** When a listening socket is ready, the server accepts the new client connection, sets its file descriptor to non-blocking, and adds it to the list of polled sockets.
- **Client Requests:** When a client socket is ready for reading, the server reads the incoming HTTP request, parses its headers and body, and validates it.
- **Generating Responses:** Based on the parsed request and the configuration, the server takes action:
    - **GET:** Locates and sends the requested file. If it's a directory, it may show a directory listing or a default page.
    - **POST:** Typically used for file uploads or CGI script execution. The server saves the uploaded content or passes the request body to the CGI.
    - **DELETE:** Removes the specified resource.
- When the response is ready, the client socket is monitored for write-readiness, and the response is sent back to the client.
# 

🖥️ Usage
Run the server with a specific configuration file. If no file is provided, it will attempt to use a default one.

```Bash
./webserv [path_to_configuration_file]
```
Configuration File  
The server's behavior is entirely controlled by a configuration file. The syntax is inspired by NGINX.

Example Configuration:
```Nginx
# Server Configuration

server
{
	listen		8080	; # Port to listen
	host 127.0.0.1;
	server_name test.com www.test.com;
	max_client_body_size 5000000;
	max_client_header_size 4000;

	# Custom error pages
	error_page 400 /home/error/400.html;
	error_page 403 /home/error/403.html;
	error_page 404 /home/error/404.html;
	error_page 405 /home/error/405.html;
	error_page 408 /home/error/408.html;
	error_page 409 /home/error/409.html;
	error_page 411 /home/error/411.html;
	error_page 413 /home/error/413.html;
	error_page 414 /home/error/414.html;
	error_page 415 /home/error/415.html;
	error_page 431 /home/error/431.html;
	error_page 500 /home/error/500.html;
	error_page 501 /home/error/501.html;
	error_page 503 /home/error/503.html;
	error_page 505 /home/error/505.html;

	# Index.html
	index index.html;

	# Routes
	location	 /
	{
		root home;
		methods GET POST;
		cgi_path_php /usr/bin # /opt/homebrew/bin;
		cgi_path_python /usr/bin;
		dir_listing off;

		location /oldDir/
		{
			root home/oldDir;
			return 307 /newDir/;
		}
		location /newDir/
		{
			root home/newDir;
			methods GET;
			cgi_path_php /usr/bin;
			cgi_path_python /usr/bin;
			dir_listing on;
		}
		location /images/
		{
			root home/images;
			methods GET POST DELETE;
			cgi_path_php /usr/bin;
			cgi_path_python /usr/bin;
			upload_dir home/images/uploads/;
			dir_listing on;

			location /images/uploads/
			{
				root home/images/uploads;
				methods GET POST DELETE;
				cgi_path_php /usr/bin;
				cgi_path_python /usr/bin;
				dir_listing on;
			}
		}
		location /cgi/
		{
			root home/cgi;
			methods GET POST;
			cgi_path_php /usr/bin;
			cgi_path_python /usr/bin;
			dir_listing off;
		}
    location /imagesREDIR/
		{
			root home/imagesREDIR;
			return 307 /images/;
		}
	}
}
```

This example sets a server that listens to port 8080  
You can also define multiple server blocks and the program runs them concurrently 

## 📜 Core Features

| Feature | Description |
| --- | --- |
| **Non-Blocking I/O** | Uses `poll()` to handle many clients on a single thread. |
| **Configuration File** | Flexible NGINX-style config for multiple virtual servers. |
| **HTTP Methods** | Implements `GET`, `POST`, and `DELETE`. |
| **Static File Serving** | Serves files from the specified root directory. |
| **Directory Listing** | Can automatically generate a list of files in a directory. |
| **File Uploads** | Accepts and saves files uploaded via POST requests. |
| **CGI Execution** | Executes server-side scripts (like PHP) |
| **Redirections** | Can be configured to redirect specific routes. |
| **Custom Error Pages** | Allows setting user-defined pages for HTTP errors. |
