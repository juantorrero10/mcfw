#define _POSIX_C_SOURCE 200809L

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

static int set_terminal_foreground(int fd, pid_t process_group) {
    sigset_t block_sigttou;
    sigset_t previous_mask;
    sigemptyset(&block_sigttou);
    sigaddset(&block_sigttou, SIGTTOU);

    if (sigprocmask(SIG_BLOCK, &block_sigttou, &previous_mask) == -1) {
        return -1;
    }

    int result = tcsetpgrp(fd, process_group);
    int saved_errno = errno;
    if (sigprocmask(SIG_SETMASK, &previous_mask, NULL) == -1 && result == 0) {
        return -1;
    }
    errno = saved_errno;
    return result;
}

static int stop_cli_until_continued(void) {
    struct sigaction default_action = {0};
    struct sigaction previous_action;
    sigset_t suspend_signal;
    sigset_t previous_mask;
    default_action.sa_handler = SIG_DFL;
    sigemptyset(&default_action.sa_mask);

    if (sigaction(SIGTSTP, &default_action, &previous_action) == -1) {
        return -1;
    }

    sigemptyset(&suspend_signal);
    sigaddset(&suspend_signal, SIGTSTP);
    if (sigprocmask(SIG_UNBLOCK, &suspend_signal, &previous_mask) == -1) {
        int saved_errno = errno;
        sigaction(SIGTSTP, &previous_action, NULL);
        errno = saved_errno;
        return -1;
    }

    int result = kill(getpid(), SIGTSTP);
    int saved_errno = errno;
    if (sigprocmask(SIG_SETMASK, &previous_mask, NULL) == -1 && result == 0) {
        result = -1;
        saved_errno = errno;
    }
    if (sigaction(SIGTSTP, &previous_action, NULL) == -1 && result == 0) {
        result = -1;
        saved_errno = errno;
    }
    errno = saved_errno;
    return result;
}

static void child_default_signals(void) {
    const int signals[] = {
        SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGCHLD
    };
    struct sigaction action = {0};
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
        sigaction(signals[i], &action, NULL);
    }
}

/**
 * @param daemon run in background and return PID.
 */
int run_subprocess(char* file, char** argv, int daemon, char* redir_stdout, char* redir_stderr, int log) {
    (void)start_logging; (void)log;
    int status = 0;
    log_cmd(file, argv);

    int terminal_fd = STDIN_FILENO;
    int has_terminal = isatty(terminal_fd);
    pid_t parent_group = getpgrp();
    int job_control = 0;
    if (!daemon && has_terminal) {
        pid_t foreground_group = tcgetpgrp(terminal_fd);
        if (foreground_group == -1) {
            log_warn("Couldn't get terminal foreground process group: %s", strerror(errno));
            return 127;
        }
        job_control = foreground_group == parent_group;
    }

    int gate[2] = {-1, -1};
    if (job_control && pipe(gate) == -1) {
        log_warn("Couldn't create subprocess start gate: %s", strerror(errno));
        return 127;
    }

    struct sigaction ignore_sigint = {0};
    struct sigaction previous_sigint;
    int changed_sigint = !daemon;
    if (changed_sigint) {
        ignore_sigint.sa_handler = SIG_IGN;
        sigemptyset(&ignore_sigint.sa_mask);
        if (sigaction(SIGINT, &ignore_sigint, &previous_sigint) == -1) {
            if (gate[0] != -1) {
                close(gate[0]);
                close(gate[1]);
            }
            log_warn("Couldn't ignore SIGINT while waiting for subprocess: %s", strerror(errno));
            return 127;
        }
    }

    pid_t pid = fork();
    if (pid == -1) {
        int saved_errno = errno;
        if (gate[0] != -1) {
            close(gate[0]);
            close(gate[1]);
        }
        if (changed_sigint) sigaction(SIGINT, &previous_sigint, NULL);
        log_warn("Couldn't create subprocess: %s", strerror(saved_errno));
        return 127;
    }

    if (!pid) {
        if (job_control) {
            close(gate[1]);
            char start;
            ssize_t bytes_read;
            do {
                bytes_read = read(gate[0], &start, sizeof(start));
            } while (bytes_read == -1 && errno == EINTR);
            close(gate[0]);
            if (bytes_read != 1 || start != 'G') _exit(127);
        }
        child_default_signals();

        if (redir_stdout) {
            int new = open(redir_stdout, O_WRONLY | O_CREAT | O_APPEND, 0);
            dup2(new, STDOUT_FILENO);
            close(new);
        }
        if (redir_stderr) {
            int new = open(redir_stderr, O_WRONLY | O_CREAT | O_APPEND, 0);
            dup2(new, STDERR_FILENO);
            close(new);
        }
        execvp(file, argv);
        perror(file);
        _exit(127);
    }

    if (job_control) {
        close(gate[0]);
        if (setpgid(pid, pid) == -1) {
            int saved_errno = errno;
            close(gate[1]);
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            if (changed_sigint) sigaction(SIGINT, &previous_sigint, NULL);
            log_warn("Couldn't create subprocess process group: %s", strerror(saved_errno));
            return 127;
        }
        if (set_terminal_foreground(terminal_fd, pid) == -1) {
            int saved_errno = errno;
            close(gate[1]);
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            if (changed_sigint) sigaction(SIGINT, &previous_sigint, NULL);
            log_warn("Couldn't give terminal to subprocess: %s", strerror(saved_errno));
            return 127;
        }
        char start = 'G';
        ssize_t bytes_written;
        do {
            bytes_written = write(gate[1], &start, sizeof(start));
        } while (bytes_written == -1 && errno == EINTR);
        close(gate[1]);
        if (bytes_written != 1) {
            int saved_errno = errno;
            set_terminal_foreground(terminal_fd, parent_group);
            kill(-pid, SIGKILL);
            waitpid(pid, NULL, 0);
            if (changed_sigint) sigaction(SIGINT, &previous_sigint, NULL);
            log_warn("Couldn't start subprocess: %s", strerror(saved_errno));
            return 127;
        }
    }

    if (!daemon) {
        int wait_error = 0;
        while(1) {
            int r = waitpid(pid, &status, WUNTRACED);

            if (r == -1) {
                // Received a signal
                if (errno == EINTR) {
                    // If its not SIGUSR1, ignore
                    if (!received_signal) continue;
                    received_signal = 0; // reset
                    // Get terminal
                    if (job_control && set_terminal_foreground(terminal_fd, parent_group) == -1) {
                        log_warn("Couldn't reclaim terminal for watch rebuild: %s", strerror(errno));
                        wait_error = 1;
                        break;
                    }

                    int build_status = cmd_build(1);
                    // If compilation returned 0 or 2 don't reload.
                    // 0 -> fail.
                    // 2 -> files didn't changed and no compilation happened.
                    if (build_status != 1) {
                        // Produce a "red herring" indicator to tell the child to not do anything.

                        int fd = creat(COMPILATION_RED_HERRING, 0644);
                        if (fd < 0) {
                            log_warn("Couldn't produce red-herring: %s", strerror(errno));
                        } else {
                            close(fd);
                        }
                    }

                    if (job_control && set_terminal_foreground(terminal_fd, pid) == -1) {
                        log_warn("Couldn't return terminal to subprocess: %s", strerror(errno));
                        wait_error = 1;
                        break;
                    }
                    
                    continue;
                // actual error.
                } else {
                    log_warn("Couldn't wait on child process: %s", strerror(errno));
                    wait_error = 1;
                    break;
                }
            }
            if (WIFSTOPPED(status)) {
                if (job_control) {
                    if (set_terminal_foreground(terminal_fd, parent_group) == -1) {
                        log_warn("Couldn't reclaim terminal from stopped subprocess: %s", strerror(errno));
                        wait_error = 1;
                        break;
                    }
                    if (stop_cli_until_continued() == -1) {
                        log_warn("Couldn't suspend CLI with stopped subprocess: %s", strerror(errno));
                        wait_error = 1;
                        break;
                    }
                    if (set_terminal_foreground(terminal_fd, pid) == -1) {
                        log_warn("Couldn't return terminal to resumed subprocess: %s", strerror(errno));
                        wait_error = 1;
                        break;
                    }
                }
                if (kill(job_control ? -pid : pid, SIGCONT) == -1 && errno != ESRCH) {
                    log_warn("Couldn't continue stopped subprocess: %s", strerror(errno));
                    kill(job_control ? -pid : pid, SIGTERM);
                }
                continue;
            }
            break;
        }

        if (wait_error) {
            pid_t child_target = job_control ? -pid : pid;
            kill(child_target, SIGCONT);
            kill(child_target, SIGTERM);
            while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
        }
        if (job_control && set_terminal_foreground(terminal_fd, parent_group) == -1) {
            log_warn("Couldn't restore terminal foreground process group: %s", strerror(errno));
        }
        if (changed_sigint && sigaction(SIGINT, &previous_sigint, NULL) == -1) {
            log_warn("Couldn't restore SIGINT handling: %s", strerror(errno));
        }
        if (wait_error || (!WIFEXITED(status) && !WIFSIGNALED(status))) return 127;
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
