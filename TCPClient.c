#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

enum sizeConstants {
	BUFSIZE = 512,
};

// Given a sockaddr struct, return it as a string (from D&C book)
char * get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen);

int main(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr, "Usage: <host> <port> <path>");
	}

	const char * host = argv[1];
	const char * port = argv[2];
	// const char * input_path = argv[3];

	struct addrinfo hints, *servinfo, *p;
	int rv, sock;

	fprintf(stderr, "Connecting to %s:%s\n", host, port);

	// get info
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;     // either IPv6 or IPV4 OK
	hints.ai_socktype = SOCK_STREAM; // use tcp

	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: \%s\n", gai_strerror(rv));
		exit(1);
	}

	// Connect to address
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sock = socket(p->ai_family, p->ai_socktype,
				   p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}

		if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
			close(sock);
			perror("connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "can't find an address to connect to");
		exit(2);
	}
	freeaddrinfo(servinfo);

	char addrbuf[128];
	get_ip_str(p->ai_addr, addrbuf, 128);
	fprintf(stderr, "Connected to %s!\n", addrbuf);

	// Send the message
	// const char * message = "A53286357\r\n";
	const char * message = "GET / HTTP/1.1\r\n"
			       "Host:  \r\n"
			       "key: value\r\n"
	 		       "\r\n"
			       "GET /subdir1/ HTTP/1.1\r\n"
			       "Host:  server\r\n"
			       "Connection: close\r\n"
			       "\r\n";
	//
	// char * message = 0;
	// long length;
	// FILE * f = fopen(input_path, "rb");
	
	// if (f) {
	//   fseek(f, 0, SEEK_END);
	//   length = ftell(f);
	//   fseek(f, 0, SEEK_SET);
	//   message = malloc(length);
	//   if (message)
	//   {
	//     fread(message, 1, length, f);
	//   }
	//   fclose (f);
	// } else {
	// 	fprintf(stderr, "file open error!");
	// }
	// if (!message) {
	// 	fprintf(stderr, "empty file!");
	// }
	printf(message);

	int ret = send(sock, message, strlen(message), 0);
	if (ret != strlen(message)) {
		fprintf(stderr, "send failed");
		exit(3);
	}

	// Receive the response
	ssize_t numBytes;
	do {
		char buffer[BUFSIZE]; // I/O buffer
		// recv only returns if server's sending. Incorrect format won't trigger
		numBytes = recv(sock, buffer, BUFSIZE - 1, 0);
		if (numBytes < 0)
			perror("recv");
		else if (numBytes == 0)
			break;
		buffer[numBytes] = '\0';    // Terminate the string!
		fputs(buffer, stdout);      // Print the echo buffer
	} while (numBytes > 0);

	fputc('\n', stdout); // Print a final linefeed

	close(sock);
	fprintf(stderr, "Closed the socket!\n");

	exit(0);
}

char * get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
	switch(sa->sa_family) {
		case AF_INET:
			inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
				s, maxlen);
				break;

		case AF_INET6:
			inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
				s, maxlen);
				break;

		default:
			strncpy(s, "Unknown AF", maxlen);
			return NULL;
	}

	return s;
}
