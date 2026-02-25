
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/inotify.h>

#include <asclib/core.h>
#include <asclib/sync.h>

#include "private/configs.h"
#include "private/log.h"



decl_async(void, hot_reload_watch);


async_(hot_reload_watch) {
    detach;
    pid_t parent_pid = getppid();
    char* watch_folder = (char*)g_app_config->app.source_folder;
    char* files[] = {watch_folder, NULL};

    while(1) {
        // Watch source files
        await(asc_inotify(files, NULL, 1, IN_MOVED_TO | IN_CLOSE_WRITE));

        // Send message to father
        kill(parent_pid, SIGUSR1);
        
        // Wait until compilation is finished.
        // If we detect a change in app .so -> trigger reload mechanism.
        // If the file is the "red herring" we ignore the change.
        char* file_changed;
        await(asc_inotify((char*[]){HOST_FOLDER, NULL}, &file_changed, 1, IN_MOVED_TO | IN_CLOSE_WRITE | IN_CREATE));
        if(!strcmp(file_changed, APP_SO)) {       
            /** @todo actually reload */ 
            mlog_info("hot_reload_watch", "recompile");
            (void)file_changed;
        } else if (!strcmp(file_changed, COMPILATION_RED_HERRING)){
            mlog_info("hot_reload_watch", "cancelled");
            remove(COMPILATION_RED_HERRING);
        }
        free(file_changed);

    }
    end_return_void();
}