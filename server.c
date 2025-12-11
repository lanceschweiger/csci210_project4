
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

struct message {
	char source[50];
	char target[50]; 
	char msg[200]; // message body
};

void terminate(int sig) {
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

int main() {
	int server;
	int target;
	int dummyfd;
	struct message req;
	ssize_t n;
	signal(SIGPIPE,SIG_IGN);
	signal(SIGINT,terminate);
	
	server = open("serverFIFO",O_RDONLY);
	if (server < 0) {
		fprintf(stderr, "server: cannot open serverFIFO for reading: %s\n", strerror(errno));
	dummyfd = open("serverFIFO",O_WRONLY);

	while (1) {
		// TODO:
		// read requests from serverFIFO

		n = read(server, &req, sizeof(req));
		if (n <= 0) {
			if (n == 0) {
				usleep(200000);
				continue;
			}
			if (n == -1) {
				usleep(200000);
				continue;
			}
		}
		
		printf("Received a request from %s to send the message %s to %s.\n",req.source,req.msg,req.target);
		fflush(stdout);
		// TODO:
		// open target FIFO and write the whole message struct to the target FIFO
		// close target FIFO after writing the message
		targetfd = open(req.target, O_WRONLY);
		if (targetfd < 0) {
			fprintf(stderr, "server: failed to open target FIFO %s: %s\n", req.target, strerror(errno))
			continue;
		}
		if (write(targetfd, &req, sizeof(req)) != sizeof(req)) {
			fprintf(stderr, "server: write to %s failed: %s\n", req.target, strerror(errno));
		}
		close(targetfd);
	}
	close(server);
	if (dummyfd >= 0) close(dummyfd);
	return 0;
}

