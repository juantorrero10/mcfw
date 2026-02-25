#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <string.h>


#include "cli.h"
#include "log.h"


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
        char* a = argv[i]; (void)a;
        fprintf(f, "%s ", argv[i]);
    }
    
    fputc('\n', f);
    fclose(f);
}

extern volatile sig_atomic_t received_signal;

/**
 * @param daemon run in background and return PID.
 */
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
        pid_t parent = getpgrp(); 
        if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, pid);
        
        while(1) {
            int r = waitpid(pid, &status, 0);

            if (r == -1) {
                // Received a signal
                if (errno == EINTR) {
                    // If its not SIGUSR1, ignore
                    if (!received_signal) continue;
                    received_signal = 0; // reset
                    // Get terminal
                    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, parent);

                    int status = cmd_build(1);
                    // If compilation returned 0 or 2 don't reload.
                    // 0 -> fail.
                    // 2 -> files didn't changed and no compilation happened.
                    if (status != 1) {
                        // Produce a "red herring" indicator to tell the child to not do anything.

                        int fd = creat(COMPILATION_RED_HERRING, 0644);
                        if (fd < 0) {
                            log_warn("Couldn't produce red-herring: %s", strerror(errno));
                        } else {
                            close(fd);
                        }
                    }

                    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, pid);
                    
                    continue;
                // actual error.
                } else {
                    log_fatal("Couldn't wait on child process: %s", strerror(errno));
                    _exit(1);
                }
            }
            break;
        }

        if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, parent);
        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            return TERM_BY_SIGNAL + sig;
        } else if (!WIFEXITED(status)) {
            return 127;
        }

        return WEXITSTATUS(status);
    }
    return pid;

}

/**
 * @brief wait if curr_jobs >= MAX_JOBS
 * @return -1 if we didn't wait,
 * @return 0 if subprocess returned != 0 or exited abnormally,
 * @return PID if subprocess returned 0.
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
    return pid;
}

int wait_one(size_t* n_jobs) {
    int status;
    pid_t pid = 0;
    if ((pid = wait(&status)) > 0) {
        (*n_jobs)--;
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            return 0;
        }
    }
    return pid;
}

void wait_all(size_t* n_jobs) {
    while(*n_jobs > 0) { 
        if (wait(NULL) > 0) {
            (*n_jobs)--;
        }    
    }
}

