#include <signal.h>
#include <crefl/core.h>
#include <crefl/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "cli.h"
#include "log.h"

#define MCFW_BINARY ".host/.mcfw_server"

int cmd_run() {

    close(creat(CACHE_DIR"/last_comp.cmd", 0644));
    
    if (!cmd_build()) return 0;

    log_running(MCFW_BINARY);

    char* argv[] = {MCFW_BINARY, NULL, NULL, NULL};
    argv[1] = "--log-level";
    char buff[10];
    argv[2] = buff;
    sprintf(argv[2], "%d", g_flags.log_level);

    signal(SIGINT, SIG_IGN);
    int ret = run_subprocess(MCFW_BINARY, argv, g_flags.daemon, g_flags.quiet, g_flags.create_log);
    if (ret != 0) log_warn("application exited abnormally, code: 0x%x", ret);


    return ret != 0;
}