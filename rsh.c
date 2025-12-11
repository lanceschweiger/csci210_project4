#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <spawn.h>
#include <sys/wait.h>

#define N 13

extern char **environ;
char uName[256];

char *allowed[N] = {"cp","touch","mkdir","ls","pwd","cat","grep","chmod","diff","cd","exit","help","sendmsg"};

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

void sendmsg (char *user, char *target, char *msg) {
    int sfd;
    struct message m;

    memset(&m, 0, sizeof(m));
    strncpy(m.source, user, sizeof(m.source)-1);
    strncpy(m.target, target, sizeof(m.target)-1);
    strncpy(m.msg, msg, sizeof(m.msg)-1);

    sfd = open("serverFIFO", O_WRONLY);
    if (sfd < 0) {
        perror("sendmsg: open serverFIFO");
        return;
    }

    if (write(sfd, &m, sizeof(m)) != sizeof(m)) {
        perror("sendmsg: write to serverFIFO");
    }
    close(sfd);
}

void* messageListener(void *arg) {
    FILE *fp;
    char buf[256];

    while (1) {
        fp = fopen(uName, "r");
        if (!fp) {
            if (errno == ENOENT) {
                usleep(200000);
                continue;
            } else {
                fprintf(stderr, "messageListener: cannot open FIFO %s: %s\n", uName, strerror(errno));
                usleep(200000);
                continue;
            }
        }

        while (fgets(buf, sizeof(buf), fp)) {
            printf("%s", buf);
            fflush(stdout);
        }

        fclose(fp);
    }

    pthread_exit(NULL);
}

int isAllowed(const char*cmd) {
    for (int i=0;i<N;i++) {
        if (strcmp(cmd,allowed[i])==0) return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char **cargv;
    char *path;
    char line[512];
    int status;
    posix_spawnattr_t attr;
    pthread_t listenerTid;

    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }

    signal(SIGINT, terminate);

    strncpy(uName, argv[1], sizeof(uName)-1);
    uName[sizeof(uName)-1] = '\0';

    if (mkfifo(uName, 0666) < 0 && errno != EEXIST) {
        fprintf(stderr, "rsh: mkfifo %s failed: %s\n", uName, strerror(errno));
        exit(1);
    }

    if (pthread_create(&listenerTid, NULL, messageListener, NULL) != 0) {
        fprintf(stderr, "Failed to create message listener thread\n");
        exit(1);
    }
    pthread_detach(listenerTid);

    while (1) {
        fprintf(stderr, "rsh>");
        if (fgets(line, sizeof(line), stdin) == NULL) continue;
        if (strcmp(line, "\n") == 0) continue;
        line[strcspn(line, "\n")] = '\0';

        char cmd[256];
        char line2[512];
        strncpy(line2, line, sizeof(line2)-1);
        line2[sizeof(line2)-1] = '\0';

        char *token = strtok(line2, " ");
        if (!token) continue;
        strncpy(cmd, token, sizeof(cmd)-1);
        cmd[sizeof(cmd)-1] = '\0';

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            char *p = line + strlen("sendmsg");
            while (*p == ' ') p++;
            if (*p == '\0') {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }
            char *target = p;
            char *sp = strchr(target, ' ');
            if (!sp) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }
            *sp = '\0';
            char *message = sp + 1;
            if (!message || *message == '\0') {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }
            sendmsg(uName, target, message);
            continue;
        }

        if (strcmp(cmd, "exit") == 0) break;

        if (strcmp(cmd, "cd") == 0) {
            char *targetDir = strtok(NULL, " ");
            if (!targetDir) {
                char *home = getenv("HOME");
                if (home) chdir(home);
            } else {
                if (strtok(NULL, " ") != NULL) {
                    printf("-rsh: cd: too many arguments\n");
                } else {
                    if (chdir(targetDir) != 0) perror("cd");
                }
            }
            continue;
        }

        if (strcmp(cmd, "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++) printf("%d: %s\n", i + 1, allowed[i]);
            continue;
        }

        char working[512];
        strncpy(working, line, sizeof(working)-1);
        working[sizeof(working)-1] = '\0';

        char *saveptr = NULL;
        char *t = strtok_r(working, " ", &saveptr);
        int n = 0;
        cargv = NULL;
        while (t != NULL) {
            cargv = (char **)realloc(cargv, sizeof(char *) * (n + 1));
            cargv[n] = strdup(t);
            n++;
            t = strtok_r(NULL, " ", &saveptr);
        }
        cargv = (char **)realloc(cargv, sizeof(char *) * (n + 1));
        cargv[n] = NULL;

        path = strdup(cargv[0]);

        posix_spawnattr_init(&attr);
        if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
            perror("spawn failed");
            for (int i = 0; i < n; i++) free(cargv[i]);
            free(cargv);
            free(path);
            posix_spawnattr_destroy(&attr);
            continue;
        }

        if (waitpid(pid, &status, 0) == -1) perror("waitpid failed");
        posix_spawnattr_destroy(&attr);

        for (int i = 0; i < n; i++) free(cargv[i]);
        free(cargv);
        free(path);
    }

    return 0;
}
