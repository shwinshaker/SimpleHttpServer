#include <sysexits.h> 		// for standard exit code

#include <iostream>			// input output

#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <string>			// for bzero
#include <sys/socket.h> 	// creating socket
#include <netinet/in.h>		// for sockaddr_in
#include <unistd.h>			// for close fork and size_t and gethostname
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <sys/sendfile.h>

#include <netdb.h> // for getaddrinfo
#include <arpa/inet.h> // for inet_ntop

#include "logger.hpp"
#include "HttpdServer.hpp"

// #define PORT "8080"
#define BACKLOG 5 // how many pending connections queue will host
#define BUFFERSIZE 256

using namespace std;

void parse_request(const char *buf, string &url, int &error_code);
bool parse_initLine(string cur_line, string &url, int &error_code);
bool check_line(const string cur_line, int &error_code);
bool text_seg(string &text, string dltr, string &before, int &error_code);

HttpdServer::HttpdServer(INIReader& t_config)
	: config(t_config)
	// just an initialization for `config`
	// such that the class instances have a `config` variable
{
	auto log = logger(); // auto means deduce the type from initializer

	string pstr = config.Get("httpd", "port", "");
	if (pstr == "") {
		log->error("port was not in the config file");
		exit(EX_CONFIG); // 78
	}
	port = pstr;

	string dr = config.Get("httpd", "doc_root", "");
	if (dr == "") {
		log->error("doc_root was not in the config file");
		exit(EX_CONFIG); // 78
	}
	char real_path[PATH_MAX];
	realpath(dr.c_str(), real_path);
	doc_root = string(real_path);
	log->debug("doc root: {}", doc_root);

	string path = config.Get("httpd", "mime_types", "");
	if (path == "") {
		log->error("mime_types was not in the config file");
		exit(EX_CONFIG); // 78
	}
	init_mime_type_map(path);
	log->debug("test: {}", mime_types_dict[string(".html")] );
}


void HttpdServer::launch()
{

	auto log = logger();
	log->info("Launching web server"); // -> used for pointer `log` in this case; . used for actual object
	log->info("Port: {}", port);
	log->info("doc_root: {}", doc_root);

	// Put code here that actually launches your webserver...

	int sockfd, new_fd;
	struct addrinfo hints, *servinfo, *p;
	int true_value = 1;
	int rv;
	char addr[INET_ADDRSTRLEN];

	// get address info
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // auto v4 v6
	hints.ai_socktype = SOCK_STREAM; // use tcp
	hints.ai_flags = AI_PASSIVE; // use my ip

	if ((rv = getaddrinfo(NULL, (const char*)port.c_str(), &hints, &servinfo)) != 0){
		log->error("Server: getaddrinfo: {}", gai_strerror(rv));
		exit(EX_SOFTWARE);
	}
	inet_ntop(AF_INET, &(servinfo->ai_addr), addr, INET_ADDRSTRLEN);
	log->info("My addr: {}", addr);

	// loop through all infos we found
	for (p = servinfo; p != NULL; p = p->ai_next) {
		// 1. socket
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1) {
			log->error("server: socket init error.");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &true_value,
					   sizeof(int)) == -1) {
			log->error("server: socket configuration error.");
			exit(EX_SOFTWARE);
		}

		// 2. bind
		// ::bind because default bind conflict with std::bind
		if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			log->error("server: bind error");
			continue;
		}

		break;
	}

	if (p == NULL) {
		log->error("server: failed to bind");
		exit(EX_SOFTWARE);
	}

	freeaddrinfo(servinfo);

	/*
	// TA's code

	// 1. socket()
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	// AF_INET: ipv4
	// SOCK_STREAM: tcp sock schema
	// 0: protocol

	// if socket was created successfully, 
	// socket() returns a non-negative number
	if(sock < 0) {
		log->error("Error: creating socket");
		exit(EX_SOFTWARE);
	}

	// This is the old way to use bind, which is ipv4 specific
	// in the new way should use getaddrinfo
	// create a sockaddr_in struct
	struct sockaddr_in server_address; // ipv4 only sock address
	server_address.sin_family = AF_INET; // ipv4
	server_address.sin_port = htons(stoi(PORT)); // port number // host to network short type
	server_address.sin_addr.s_addr = htonl(INADDR_ANY); // should be inet_addr("0.0.0.0")
	// 32-bit ip address // host to network long type
	// INADDR_ANY // allow binding to any local ip address

	// 2. bind() // bind socket descriptor to a port such that incoming packets are directed this socket
	int b = ::bind(sock, (struct sockaddr*)&server_address,
				   sizeof(server_address));

	// if bind is successful it returns a 0, else 1
	if(b < 0) {
		log->error("Error: binding socket");
		close(sock);
		exit(EX_SOFTWARE);
	}
	*/

	// print some info of host
	char hostname[128];
	gethostname(hostname, sizeof hostname);
	log->info("Server is now running on {}", hostname);

	// 3. listen
	if (listen(sockfd, BACKLOG) == -1){
		log->error("Server: unable to listen.");
		exit(EX_SOFTWARE);
	};

	// 4. accept and 5. receive
	struct sockaddr_in client_address; // address who calling // using sockaddr_storage to allow both ipv4 and ipv6
	socklen_t client_length = sizeof(client_address); //  = seems not necessary
	char client_ip[INET6_ADDRSTRLEN];

	log->info("server: now waiting for connections..");
	while(1) {
		new_fd = accept(sockfd, (struct sockaddr*)&client_address,
						&client_length);

		// if connection was created successfully, 
		// accept() returns a non-negative number
		if(new_fd < 0) {
			log->error("Error: accept connection");
			//close(sock);
			continue;
		}

		// show some info from client request
		inet_ntop(client_address.sin_family,
				  get_in_addr((struct sockaddr *)&client_address),
				  client_ip, sizeof client_ip);
		log->info("Server: got connection from {}", client_ip);

		// fork a child process to handle the request
		if (!fork()) {
			// child process
			close(sockfd);

			// int n = read(new_fd, buffer, BUFFERSIZE);
			// buffer to accept message
			char buffer[BUFFERSIZE];
			bzero(buffer, BUFFERSIZE);

			string message;
			int messageLen = 0;

			// string request;

			int bufferLen = BUFFERSIZE;
			// while(n == BUFFERSIZE){
			while(true){
			    bufferLen = recv(new_fd, buffer, BUFFERSIZE, 0); // recv is just the specialized version of read
			    if(bufferLen < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
				    // timeout
				    if (messageLen > 0) {
					// if timeout happens, and some messsage received
					// reply back with 400
					handle_request(NULL, new_fd, false);
				    }
				    // close the connection
				    close(new_fd);
				    log->error("Time out. Close connection.");
				    exit(EX_NOINPUT);
				}
				close(new_fd);
			    	log->error("Error: receive message.");
				exit(EX_SOFTWARE);
			    } else if (bufferLen == 0) {
				close(new_fd);
				log->error("Remote closed the connection by orderly shutdown.");
				exit(EX_NOUSER);
			    } else {
			    	log->info("Message received.");
			    	log->info("Message length: {}", bufferLen);
			    }

			    message += string(buffer, bufferLen);
			    messageLen += message.length();

			    // process each line
			    size_t delimiter = message.find("\r\n\r\n");
			    while(delimiter != string::npos) {
				// we have found a request end with "\r\n\r\n"
				// cut out the request
				// string request = message.substr(0, delimiter);
				string request = message.substr(0, delimiter+2);
				// +2: include one \r\n to indicate the line
				log->info("request: {}", request);
				// if client wants to close connection after this request,
				//// handle request and then close socket
				if (request.find("Connection: close") != string::npos) {
				    handle_request(request.c_str(), new_fd, true);
				    close(new_fd);
				    log->info("Remote closed the connection by Connection: close.");
				    exit(EX_NOUSER);
				}
				// otherwise, handle the request normally
				handle_request(request.c_str(), new_fd, false);

				/*
				if (isInitialLine(line)) {
				    log->debug("child socket: handle get!");
				    handle_get(line, new_fd);
				}
				if (isClosedLine(line)) {
				    log->debug("child socket: handle closed!");
				    handle_close(NULL, new_fd);
				}
				*/

				// rest of the message
				message = message.substr(delimiter+4);
				delimiter = message.find("\r\n\r\n");
				if (message.length() == 0) {
				    // this `if` isn't necessary
				    // this segment end
				    break;
				}
			    }
			}
			log->error("Should never reach this!");
			close(new_fd);
			exit(0);
		}

		// 7. close
		// parent - close child socket
		close(new_fd);
		// sleep(1);
	}

}

/*
bool isInitialLine(string line) {
    // todo
    return true;
    return false;
}
*/

void HttpdServer::handle_request(const char* buf, int client_sock, bool connection_close){

	auto log = logger();

	int error_code = 200;
	string url = "";
	// check and parse request
	parse_request(buf, url, error_code);

	// get full path
	string full_path = get_file_path(url, error_code);

	// create headers
	string headers = build_headers(full_path, error_code, connection_close);
	log->debug("headers: {}", headers);

	// send headers // why need to convert to void
	send(client_sock, (void*) headers.c_str(), (ssize_t) headers.size(), 0);
	// should handle the return value: size that actually sent

	// send body
	struct stat finfo;
	int fd = open(full_path.c_str(), O_RDONLY);
	fstat(fd, &finfo);
	// off_t off = 0;
	// int h = sendfile(fd, client_sock, 0, &off, NULL, 0);
	int h = sendfile(client_sock, fd, NULL, finfo.st_size);
	// verbose more status
	log->info("sendfile status: {}", h); // -1: error; 0: success
}

void parse_request(const char *buf, string &url, int &error_code) {
	auto log = logger();

	if (strlen(buf) == 0) {
		log->error("request empty: {}", buf);
		error_code = 400;
		return;
	}
	else{
		// Copy the buffer to parse
		string buf_copy = buf;
		string cur_line;

		// parse initial line
		if (!text_seg(buf_copy, "\r\n", cur_line, error_code)) return;
		if (!parse_initLine(cur_line, url, error_code)) return;

		// check host
		if (!text_seg(buf_copy, "\r\n", cur_line, error_code)) return;
		if (!check_line(cur_line, error_code)) {
			return;
		} else {
			size_t pos = cur_line.find(": ");
			string key = cur_line.substr(0, pos);
			if (key.find("Host") == string::npos) {
				log->error("Host missing in key.");
				error_code = 400;
				return;
			}
		}

		// check rest of the header
		while (error_code == 200 and buf_copy.length() > 0){
			if (!text_seg(buf_copy, "\r\n", cur_line, error_code)) return;
			if (!check_line(cur_line, error_code)) return;
		}
	}
}

bool check_line(const string cur_line, int &error_code) {
	auto log = logger();

	size_t pos = cur_line.find(": ");
	if (pos == string::npos){
		log->error("Illegal header line: Missing ': ': {}", cur_line);
		error_code = 400;
		return false;
	}
	if (pos == 0) {
		log->error("Key missing for line: {}", cur_line);
		error_code = 400;
		return false;
	}
	if (pos+2 == cur_line.length()) {
		log->error("Value missing for line: {}", cur_line);
		error_code = 400;
		return false;
	}
	return true;
}

bool text_seg(string &text, string dltr, string &before, int &error_code){
	auto log = logger();

	if (text.length() == 0) {
		log->error("Unexpected end of text {}", text);
		error_code = 400;
		return false;
	}

	size_t pos = text.find(dltr);
	if (pos == string::npos) {
		log->error("deliminiter {} not exists in text {}", dltr, text);
		error_code = 400;
		return false;
	}
	before = text.substr(0, pos);
	text = text.substr(pos + dltr.length());
	return true;
}

bool parse_initLine(string cur_line, string &url, int &error_code){

	auto log = logger();

	// legal request initial line
	log->debug("parsing first line: {}..", cur_line);
	if (cur_line.length() == 0) {
		log->error("Line empty.");
		error_code = 400;
		return false;
	}

	string method;
	if (!text_seg(cur_line, " ", method, error_code)) return false;
	if (method.compare("GET") != 0) {
		log->error("GET Method mismatch: {}", method);
		error_code = 400;
		return false;
	}

	if (!text_seg(cur_line, " ", url, error_code)) return false;
	if (url.length() == 0 or url[0] != '/') {
		log->debug("url format incorrect! (missing /): {}", url);
		error_code = 400;
		return false;
	}
	if (cur_line.compare("HTTP/1.1") != 0) {
		log->debug("HTTP/1.1 mismatch: {}", cur_line);
		error_code = 400;
		return false;
	}
	return true;
}

string HttpdServer::build_headers(string filepath, int error_code, bool connection_close){
	string response;
	if (error_code == 200){
		response += "HTTP/1.1 200 OK\r\n";
		response += "Server: Myserver 1.0 \r\n";
		response += "Last-Modified: " + get_last_modified(filepath) + "\r\n";
	} else if (error_code == 400){
		response += "HTTP/1.1 400 Client Error\r\n";
		response += "Server: Myserver 1.0 \r\n";
	}
	else{
		response += "HTTP/1.1 404 Not Found\r\n";
		response += "Server: Myserver 1.0 \r\n";
	}
	if (connection_close) {
		response += "Connection: close\r\n";
	}

	response += "Content-Length: "+ to_string(get_file_size(filepath.c_str())) +"\r\n";
	response += "Content-Type: " + get_mime_type(filepath)+"\r\n";
	response += "\r\n";
	return response;
	
}

void *HttpdServer::get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

string HttpdServer::get_file_path(string url, int &error_code){
	string full_path;

	if (error_code == 400) {
		full_path = doc_root + "/error.html";
	}
	else{
		full_path = doc_root + url;
		if (url[url.length()-1] == '/') {
			full_path += "index.html";
		}

		char real_path[PATH_MAX];
		realpath(full_path.c_str(), real_path);
		full_path = string(real_path);

		logger()->debug("previous error code: {}", error_code);
		logger()->debug("full path: {}", full_path);
		logger()->debug("root: {}", doc_root);
		logger()->debug("real path: {}", full_path);
		if (access(full_path.c_str(), F_OK) == -1 or full_path.find(doc_root) == string::npos){
			error_code = 404;
			full_path = doc_root + "/error.html";
		}
	}
	return full_path;
}

int get_file_size(const char* filepath)
{
    struct stat finfo;
    if (stat(filepath, &finfo) != 0) {
        //die_system("stat() failed");
    }
    return (int) finfo.st_size;
}

string HttpdServer::get_mime_type(string filename){
	int pos = filename.rfind(".");
	if (pos == -1)
		return "application/octet-stream";
	string suffix = filename.substr(pos, filename.length()-1);
	if (mime_types_dict.find(suffix) == mime_types_dict.end())
		return "application/octet-stream";
	return mime_types_dict[suffix];
}

int HttpdServer::init_mime_type_map(string path){
	// map<string, string> type_map;
	auto log = logger();
	ifstream ifile(path);
	while(!ifile.eof()){
		string str;
		getline(ifile, str);
		int pos = str.find(" ");
		string k = str.substr(0, pos);
		string v = str.substr(pos+1, str.length());
		mime_types_dict.insert(pair<string, string>(k, v));

	}
	return 0;
}

string get_last_modified(string path){
	FILE * fp;
	int fd;
	struct stat buf;
	fp=fopen(path.c_str(),"r");
	fd=fileno(fp);
	fstat(fd, &buf);
	time_t t = buf.st_mtime;
	struct tm lt;
	localtime_r(&t, &lt);
	char timbuf[80];
	strftime(timbuf, sizeof(timbuf), "%a, %d %b %y %T %z", &lt);
	string last_modified = timbuf;
	return last_modified;
}




/*Reference*/
// sysexit.h: https://www.freebsd.org/cgi/man.cgi?query=sysexits&apropos=0&sektion=0&manpath=FreeBSD+4.3-RELEASE&format=html
