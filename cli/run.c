#include <crefl/core.h>
#include <crefl/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "cli.h"
#include "log.h"

#define MCFW_BINARY ".host/.mcfw_server"

REFLECT(enum) {
    SIGHUP           =1,
    SIGINT           =2,
    SIGQUIT          =3,
    SIGILL           =4,
    SIGTRAP          =5,
    SIGABRT          =6,
    SIGIOT           =6,
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

int cmd_run() {

    close(creat(CACHE_DIR"/last_comp.txt", 0644));
    
    if (!cmd_build()) return 0;

    if (!g_flags.daemon)
        log_running(MCFW_BINARY);

    char* argv[] = {MCFW_BINARY, NULL, NULL, NULL};
    argv[1] = "--log-level";
    char buff[10];
    argv[2] = buff;
    sprintf(argv[2], "%d", g_flags.log_level);

    signal(SIGINT, SIG_IGN);
    int ret = run_subprocess(MCFW_BINARY, argv, g_flags.daemon, g_flags.quiet, g_flags.create_log);
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