#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "log.h"
#include "cli.h"

char* app_entry_c = ""
"/**\n * @brief Generated example application\n**/\n"
"#include <mcfw/router.h>\n"
"#include <mcfw/server.h>\n\n"
"void home(const HttpRequest*, HttpResponse* res) {\n"
"\t// Set the content of the response as HTML and attach a literal string.\n"
"\tres_set_content(res, CONTENT_HTML, DS_LIT(\"<h1>Welcome!</h1>\"));\n"
"\t// Add a custom header\n"
"\tres_add_header(res, DS_LIT(\"X-Foo\"), DS_LIT(\"Var\"));\n"
"}\n\n"
"void stop(const HttpRequest*, HttpResponse* res) {\n"
"\t// Gracefully stop the server and add \"Connection: Close\" header\n"
"\tserver_stop();\n"
"\tres_close_connection(res);\n"
"}\n\n\n"
"void app_entry(Router* r) {\n\n"
"\trouter_set_routes(r,\n"
"\t\tROUTE(GET, \"/\", HANDLER_FN(home)),\n"
"\t\tROUTE(GET, \"/stop\", HANDLER_FN(stop)),\n"
"\t);\n\n"
"}";

char* gitignore = "mcfw\n.mcfw\n"COMPERR_REDIRECT;

char* app_config_json = "{\n"
    "\t\"app\" : \n"
    "\t{\n"
        "\t\t\"source_folder\": \"src\",\n"
        "\t\t\"entry_point\" : \"app_entry\",\n"

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