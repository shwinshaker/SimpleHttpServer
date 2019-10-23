#ifndef HTTPDSERVER_HPP
#define HTTPDSERVER_HPP

#include "inih/INIReader.h"
#include "logger.hpp"

#include <string.h>
#include <map>

using namespace std;

class HttpdServer {
public:
	HttpdServer(INIReader& t_config);

	void launch();
	void handle_request(const char* buf, int client_sock, bool isConnectionClosed);
	void *get_in_addr(struct sockaddr *sa);
	string get_mime_type(string filename);
	int init_mime_type_map(string filename);
	string build_headers(string filepath, int error_code, bool connection_close);
	string get_file_path(string url, int &error_code);

protected: // accessible by class and classes inherit from it
	INIReader& config;
	string port;
	string doc_root;
	map<string, string> mime_types_dict;
};

// int validate_file(string full_path, int &error_code);
int get_file_size(const char* filepath);
string get_last_modified(string path);


#endif // HTTPDSERVER_HPP
