
#define _POSIX_C_SOURCE 199309L

#include <crefl/core.h>
#include <crefl/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "cli.h"
#include "log.h"

#define MCFW_BINARY MCFW_DIR"/bin/.mcfw_server"

#define COMP_RED_HERRING MCFW_DIR"/.host/dont_reload"

REFLECT(enum) {
    SIGHUP           =1,
    SIGINT           =2,
    SIGQUIT          =3,
    SIGILL           =4,
    SIGTRAP          =5,
    SIGABRT          =6,
    SIGBUS           =7,
    SIGFPE           =8,
    SIGKILL          =9,
    SIGUSR1         =10,
    SIGSEGV         =11,
    SIGUSR2         =12,
    SIGPIPE         =13,
    SIGALRM         =14,
    SIGTERM         =15,
    SIGSTKFLT       =16,
    SIGCHLD         =17,
    SIGCONT         =18,
    SIGSTOP         =19,
    SIGTSTP         =20,
    SIGTTIN         =21,
    SIGTTOU         =22,
    SIGURG          =23,
    SIGXCPU         =24,
    SIGXFSZ         =25,
    SIGVTALRM       =26,
    SIGPROF         =27,
    SIGWINCH        =28,
    SIGIO           =29,
    SIGPWR          =30,
    SIGSYS          =31,
    SIGUNUSED       =31
} signals;



#include <signal.h>

// Signal turned on by SIGUSR1 handler
volatile sig_atomic_t received_signal = 0;

static void sigusr1_h(int s) {
    (void)s;
    received_signal = 1;
}

int cmd_run() {

    close(creat(CACHE_DIR"/last_comp.txt", 0644));

    if (g_flags.watch) {
        struct sigaction sa = {0};
        sa.sa_handler = sigusr1_h;
        sigemptyset(&sa.sa_mask);

        if (sigaction(SIGUSR1, &sa, NULL) == -1) {
            log_fatal("Could not set watch signal handler (SIGUSR1)");
            _exit(1);
        }
    }
    if (!cmd_build(0)) return 0;

    if (!g_flags.daemon)
        log_running(MCFW_BINARY);

    char* argv[] = {MCFW_BINARY, NULL, NULL, NULL, NULL};
    argv[1] = "--log-level";
    char buff[10];
    argv[2] = buff;
    sprintf(argv[2], "%d", g_flags.log_level);
    argv[3] = (g_flags.watch)? "--enable-hot-reloading" : NULL;

    int ret = run_subprocess(
        MCFW_BINARY, 
        argv, 
        g_flags.daemon, 
        (g_flags.quiet)? "/dev/null" : NULL,
        (g_flags.quiet)? "/dev/null": NULL,
        g_flags.create_log);
    if (g_flags.daemon) {
        log_running(STYLE_ITALIC""MCFW_BINARY""COLOR_RESET" ["STYLE_BOLD"%d"COLOR_RESET""COLOR_WHITE"]"COLOR_RESET, ret);
        return 0;
    }
    if (ret != 0) {
        if (ret > TERM_BY_SIGNAL) {
            REnum* e = refl_fetch_enum("signals");
            log_warn("application terminated by signal %d (%s)", 
                ret - TERM_BY_SIGNAL, 
                refl_enum_get_label(e, ret - TERM_BY_SIGNAL));
            return 0;
        }
        log_warn("application exited abnormally, code: 0x%x", ret);
        return 0;
    }

    
    return 1;
}