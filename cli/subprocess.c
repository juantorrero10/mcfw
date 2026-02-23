#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>


#include "cli.h"


static inline char* get_new_log_name() {
    sprintf(cache_string, "log_%lu.txt", (unsigned long)time(NULL));
    return cache_string;
}

static void start_logging() {

}

void log_cmd(char* file, char** argv) {
    FILE* f = fopen(CACHE_DIR"/last_comp.txt", "a");
    fprintf(f, "[%s] ", file);

    for (int i = 0; argv[i] != NULL; i++) {
        fprintf(f, "%s ", argv[i]);
    }
    
    fputc('\n', f);
    fclose(f);
}

int run_subprocess(char* file, char** argv, int daemon, int quiet, int log) {
    (void)start_logging;
    int status = 0;
    log_cmd(file, argv);


    pid_t pid = fork();
    assert(pid != -1);

    if (!pid) {
        signal(SIGINT, SIG_DFL);

        if (quiet && !log) {
            int devnull = open("/dev/null", O_WRONLY, 0);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execvp(file, argv);
        assert(0);
    }

    if (!daemon) {
        if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, pid);
        
        waitpid(pid, &status, 0);

        if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, getpgrp());
        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            return TERM_BY_SIGNAL + sig;
        }

        return WEXITSTATUS(status);
    }
    return 0;

}

/**
 * @brief wait if curr_jobs >= MAX_JOBS
 * @return -1 if we didn't wait,
 * @return 0 if subprocess returned != 0 or exited abnormally,
 * @return 1 if subprocess returned 0.
 */
int wait_parallel(size_t* n_jobs, size_t max_jobs) {
    if (*n_jobs < max_jobs) return -1;
    int status;
    pid_t pid = wait(&status);
    if (pid <= 0) return 1;
    (*n_jobs)--;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return 0;
    }
    return 1;
}

int wait_one(size_t* n_jobs) {
    int status;
    if (wait(&status) > 0) {
        (*n_jobs)--;
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            return 0;
        }
    }
    return 1;
}

void wait_all(size_t* n_jobs) {
    while(*n_jobs > 0) { 
        if (wait(NULL) > 0) {
            (*n_jobs)--;
        }    
    }
}

