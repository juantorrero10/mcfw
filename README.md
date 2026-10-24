# MCFW (Modern C Framework)

A lightweight HTTP backend framework written in pure C. MCFW provides an asynchronous runtime, a reflection and serialization engine, and a built-in CLI that enables automatic builds and hot reloading for user applications.

## Architecture and Components

MCFW is composed of three primary systems:
*   **asclib**: A single-threaded, stackful asynchronous runtime that manages the I/O event loop and non-blocking networking.
*   **crefl**: A C reflection and serialization engine used to map network payloads (such as JSON) directly into C structs.
*   **CLI Build System**: A developer tool (`./mcfw`) that compiles the framework, manages dependencies, and provides hot reloading (via the `watch` command).

## Installation and Setup

### 1. Clone the Repository
> Because the framework relies on submodules for `asclib` and `crefl`, you must clone the repository using the `--recursive` flag:
```bash
git clone --recursive https://github.com/juantorrero10/mcfw.git
cd mcfw
```


### 2. Install dependencies
> **TODO: script to install them**
> **This project only works in a POSIX-compliant system and x86_64 architecture**
> requires `gcc`, `make` and crefl requires `python3.12+` and `pycparser`.

### 3. Build the framework
> Run the setup script to build the submodules, compile the framework, and generate the development environment inside the `./app` directory:
```bash
./setup.sh
```


### 4. Initialize the Application
> The enviroment includes the binaries but mainly, the CLI application `./mcfw`.
> To start a project do:
```bash
cd ./app
./mcfw init
```

## CLI Usage (`./mcfw`)
The included CLI tool manages compilation, execution, and hot reloading with `./mcfw watch`.
Help message with `./mcfw --help` 
```text
Usage:
MCFW: The Modern C Framework.

    -h, --help                show this help message and exit

Compilation options.
    -f, --force-compile       Ignore cached obj files, recompile everything
    -s, --serial-compilation  Do not parallelize the compilation.
    --silent-comp             Redirect compilarion gcc errors to "comp-out.log".

Runtime options.
    --log-level=<int>         0 = INFO, 1 = WARNINGS, 2 = ERRORS AND PANICS
    -q, --quiet               Run without output.
    -d, --daemon              Run in background

Misc.
    --set-pwd=<str>           Set working directory.

Commands: init, build, run, watch, clean
```

## Examples

### 1. Basic application and routing
MCFW maps HTTP methods and paths to handler functions.

```c
#include <mcfw/router.h>
#include <mcfw/server.h>

void home(const HttpRequest* req, HttpResponse* res) {
    // Set response content to HTML
    res_set_content(res, CONTENT_HTML, DS_LIT("<h1>Welcome!</h1>"));
    // Add custom headers
    res_add_header(res, DS_LIT("X-Foo"), DS_LIT("Var"));
}

void stop_server(const HttpRequest* req, HttpResponse* res) {
    // Gracefully stop the server and add "Connection: Close" header
    server_stop();
}

void app_entry(Router* r) {
    router_set_routes(r,
        ROUTE(GET, "/", HANDLER_FN(home)),
        ROUTE(GET, "/stop", HANDLER_FN(stop_server)),
    );
}
```

### 2. Static files and folders
You can map routes to files/dirs locally. 
```c

void app_entry(Router* r) {
    router_set_routes(r,
        ROUTE(GET, "/", HANDLER_DIR("./www")),
        ROUTE(GET, "/documentation", HANDLER_FILE("./some_pdf.pdf")),
    );
}
```

### 3. String and Buffer Management
MCFW uses a specialized set of types defined in `<mcfw/buffers.h>` to manage memory safely and avoid standard C string overhead:
*   **`DStatic`**: Represents a buffer that is either not owned or doesn't require to be freed. Often initialized with the `DS_...` macros.
*   **`DHeap`**: An owned, heap-allocated string or buffer. Often initialized with the `DH_...` macros.
*   **`DBuffer`**: A generalitzation of the of the above types through the macro `DB_FROM(buff)`, If a functions asks for a DBuffer it means that it will try to take ownership of the memory so **DON'T TRY TO FREE IT**.
```c
#include <mcfw/buffers.h>
#include <mcfw/http.h>

void example_static(const HttpRequest* req, HttpResponse* res) {
    // DS_LIT creates a DStatic view at compile time without allocation
    DStatic greeting = DS_LIT("<h1>Static buff!</h1>");
    // res_set_content macro converts DStatic to DBuffer automatically
    // the response knows not to free it.
    res_set_content(res, CONTENT_HTML, greeting);
}

void example_heap(const HttpRequest* req, HttpResponse* res) {
    // Allocate 1024 bytes in the heap.
    DHeap h_buff = DH_NEW(1024);
    int foo = 4467;
    DH_WRITE(buff, "<h1>Heap buff!: %d</h1>", foo);
    // res_set_content macro converts DHeap to DBuffer automatically
    // the response will free it.
    res_set_content(res, CONTENT_HTML, h_buff);
    // NO: DH_FREE(h_buff)
}
```



### 4. Reflection and JSON Deserialization
Using the `crefl` engine, you can parse incoming JSON (in the future Query parameters too) payloads directly into C structs. The `REFLECT` macro tags the struct for the preprocessor, and `ROUTE_DESERIALIZE` handles the deserilization automatically.

```c
#include <mcfw/router.h>
#include <mcfw/server.h>

REFLECT(struct) {
    uint64_t session_id;
    const char* title;
    OPTION(char*) body;
} bug_report_post_t;

REFLECT(struct) {
	int32_t id;
	const char* status;
} bug_report_response_t;

void bug_report(const HttpRequest* req, HttpResponse* res) {
    RHandle* h = server_get_crefl_handle();
    
    // Retrieve the automatically deserialized request body
    bug_report_post_t* bug = req_get_deserialized_body(req);
    
    // Access each member
    bug->session_id // ...
    bug->title // ...
    bug->body // ...

    // Make a JSON response an set status code
	if (bug->session_id != 738473) {
		bug_report_response_t r = { .id = 1, .status = "Failure, unauthorized user" };
		res_set_content_serialize(res, CONTENT_JSON, bug_report_response_t, &r);
		res_set_status_code(res, HTTP_UNAUTHORIZED);
		return;
	}
}

void app_entry(Router* r) {
    router_set_routes(r,
        ROUTE_DESERIALIZE(POST, "/api/report", HANDLER_FN(bug_report), CONTENT_JSON, bug_report_post_t)
    );
}
```