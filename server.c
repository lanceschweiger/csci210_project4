#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

struct message {
    char source[50];
    char target[50];
    char msg[200];
};

void terminate(int sig) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

int main() {
    int server = -1;
    int targetfd = -1;
    int dummyfd = -1;
    struct message req;
    ssize_t n;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    if (mkfifo("serverFIFO", 0666) < 0 && errno != EEXIST) {
        fprintf(stderr, "server: mkfifo failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    server = open("serverFIFO", O_RDONLY | O_NONBLOCK);
    if (server < 0) {
        fprintf(stderr, "server: cannot open serverFIFO for reading: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    dummyfd = open("serverFIFO", O_WRONLY | O_NONBLOCK);

    while (1) {
        n = read(server, &req, sizeof(req));
        if (n == sizeof(req)) {
            printf("Received a request from %s to send the message %s to %s.\n",
                   req.source, req.msg, req.target);
            fflush(stdout);

            targetfd = open(req.target, O_WRONLY);
            if (targetfd < 0) {
                perror("server: failed to open target FIFO");
                continue;
            }

            dprintf(targetfd, "Incoming message from %s: %s\n", req.source, req.msg);
            close(targetfd);
            continue;
        } else if (n == 0) {
            usleep(200000);
            continue;
        } else if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(200000);
                continue;
            } else {
                fprintf(stderr, "server: read error: %s\n", strerror(errno));
                usleep(200000);
                continue;
            }
        } else {
            usleep(200000);
            continue;
        }
    }

    if (server >= 0) close(server);
    if (dummyfd >= 0) close(dummyfd);
    return 0;
}

