#ifndef PRIVATE_MCFW_H_
#define PRIVATE_MCFW_H_

#define _GNU_SOURCE

/**
 * @brief This function is accesible to the user application.
 */
#define _Public_
/**
 * @brief This parameter is optional and will be explicitly checked if its NULL.
 * @note In absense of this marker, argument will probably triggrer as SIGSEGV if passed NULL.
 */
#define _Opt_
/**
 * @brief This parameter is passed as a pointer to be written to by callee.
 */
#define _Out_
/**
 * @brief This paramater would be read from and written to by callee.
 */
#define _In_Out_
/**
 * @brief This function spawns an asyncronous task and will yield at some point.
 */
#define _Async_

#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <errno.h>

#include <asclib/core.h>
#include <asclib/net/tcp.h>
#include <asclib/sync.h>
#include <asclib/cleanup.h>
#include <asclib/io.h>

#include <crefl/core.h>
#include <crefl/scope.h>
#include <crefl/serial/formats.h>

#include <picohttpparser.h>
#include <argparse.h>

#include <private/arena.h>
#include <private/buffers.h>
#include <private/configs.h>
#include <private/glob.h>
#include <private/http.h>
#include <private/log.h>
#include <private/router.h>
#include <private/version.h>
#include <private/server.h>

#endif // PRIVATE_MCFW_H_