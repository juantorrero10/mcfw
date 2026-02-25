#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "log.h"

char* app_entry_c = "/**\n * @brief Genrated template entry for the app.\n**/\n"
"#include <asclib/core.h>\n#include <stdio.h>\n\n\nasync(mcfw_start) {\n\n"
"   printf(\"app entry!!\\n\");\n"
"   end_return_void();\n"
"}\n";

char* gitignore = ".host\nmcfw\n._ccomp_cache__\nlogs\n.crefl";

char* app_config_json = "{\n"
    "\t\"app\" : \n"
    "\t{\n"
        "\t\t\"source_folder\": \"src\",\n"
        "\t\t\"entry_point\" : \"mcfw_start\",\n"

        "\t\t\"compilation\" : {\n"
            "\t\t\t\"c_std\": \"17\",\n"
            "\t\t\t\"cflags\": []\n"
        "\t\t}\n"
    "\t},\n"

    "\n\t\"host\" : \n"
    "\t{\n"
        "\t\t\"addr\" : [127, 0, 0, 1],\n"
        "\t\t\"port\" : 8080\n"
    "\t}\n"
"}";


int cmd_init() {
    FILE* f = fopen(".gitignore", "w");
    fwrite(gitignore, strlen(gitignore), 1, f);
    fclose(f);
    struct stat st = {0};
    if (stat("src", &st) == -1)
        mkdir("src", 0700);
    f = fopen("src/app_entry.c", "w");
    fwrite(app_entry_c, strlen(app_entry_c), 1, f);
    fclose(f);
    f = fopen("app_config.json", "w");
    fwrite(app_config_json, strlen(app_config_json), 1, f);
    fclose(f);

    log_okay("Generated minimal workspace.");
    return 1;
}