#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define SIZE 1000
#define MAX_ARGS 20
#define MAX_CMDS 10

int main() {
    char line[SIZE], path[SIZE] = "/bin:/usr/bin";
    char *args[MAX_CMDS][MAX_ARGS], *cmds[MAX_CMDS];
    int p[MAX_CMDS-1][2], n, i, j;

    while (1) {
        printf("shell> ");
        fflush(stdout);
        if (!fgets(line, SIZE, stdin)) break;
        line[strcspn(line, "\n")] = '\0';

        if (!strcmp(line, "exit")) break;
        if (!strncmp(line, "PATH=", 5)) {
            strcpy(path, line+5);
            continue;
        }

        n = 0;
        cmds[n++] = line;
        for (i = 0; line[i]; i++)
            if (line[i] == '|') {
                line[i] = '\0';
                if (n < MAX_CMDS) cmds[n++] = line+i+1;
                else { n = 0; break; }
            }
        if (!n) continue;

        for (i = 0; i < n; i++) {
            int a = 0;
            char *save, *w = strtok_r(cmds[i], " \t", &save);
            while (w && a < MAX_ARGS-1) {
                args[i][a++] = w;
                w = strtok_r(NULL, " \t", &save);
            }
            args[i][a] = NULL;
        }

        if (n == 1 && args[0][0] && !strcmp(args[0][0], "cd")) {
            if (chdir(args[0][1] ? args[0][1] : getenv("HOME")) < 0)
                perror("cd");
            continue;
        }

        for (i = 0; i < n-1; i++)
            if (pipe(p[i]) < 0) { perror("pipe"); exit(1); }

        for (i = 0; i < n; i++) {
            if (fork() == 0) {
                if (i) dup2(p[i-1][0], 0);
                if (i < n-1) dup2(p[i][1], 1);
                for (j = 0; j < n-1; j++) {
                    close(p[j][0]); close(p[j][1]);
                }

                for (j = 0; args[i][j]; j++)
                    if ((!strcmp(args[i][j], "<") ||
                         !strcmp(args[i][j], ">") ||
                         !strcmp(args[i][j], ">>")) && args[i][j+1]) {
                        int in = !strcmp(args[i][j], "<");
                        int fd = open(args[i][j+1],
                            in ? O_RDONLY : O_WRONLY|O_CREAT|
                            (!strcmp(args[i][j], ">>") ? O_APPEND : O_TRUNC),
                            0644);
                        if (fd < 0) { perror("open"); exit(1); }
                        dup2(fd, in ? 0 : 1); close(fd);
                        args[i][j] = NULL;
                        break;
                    }

                char copy[SIZE], full[SIZE], *save;
                strcpy(copy, path);
                char *d = strtok_r(copy, ":", &save);
                while (d) {
                    snprintf(full, SIZE, "%s/%s", d, args[i][0]);
                    execv(full, args[i]);
                    d = strtok_r(NULL, ":", &save);
                }
                execvp(args[i][0], args[i]);
                perror("command"); exit(1);
            }
        }

        for (i = 0; i < n-1; i++) {
            close(p[i][0]); close(p[i][1]);
        }
        for (i = 0; i < n; i++) wait(NULL);
    }
    return 0;
}
