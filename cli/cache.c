#define _XOPEN_SOURCE 500
#include <ftw.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

#include "cli.h"
#include "log.h"


static inline void make_cache() {
    struct stat st = {0};
    if (stat(CACHE_DIR, &st) == -1)
        mkdir(CACHE_DIR, 0700);
}


static inline int __is_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

static inline uint64_t checksumfile(const char* file) {
    struct stat st;
    if (stat(file, &st) != 0) {
        log_fatal("Tried to checksum a non-existent file: %s", file);
        _Exit(1);
    }
    int fd = open(file, O_RDONLY);
    if (fd <= 0) {
        log_fatal("Could open file '%s': %s", file, strerror(errno));
        _Exit(1);
    }

    void* content = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (content == MAP_FAILED) {
        close(fd);
        return 0;
    }

    uint64_t hash = 5381;
    uint8_t* ptr = (uint8_t*)content;
    uint8_t* end = ptr + st.st_size;

    while (ptr < end) {
        hash = ((hash << 5) + hash) + *ptr++;
    }

    munmap(content, st.st_size);
    close(fd);
    return hash;
}

/**
 * @brief Update checksum and returned whether it matched
 */
int update_checksum(char* binary_file) {
    uint64_t cs = checksumfile(binary_file);
    struct stat st;
    if (stat(CHECKSUM_FILE, &st) == 0) {
        int fd = open(CHECKSUM_FILE, O_RDWR);
        if (fd <= 0) {
            log_fatal("Couldn't update the checksum: %s", strerror(errno));
            _Exit(1);
        }
        uint64_t old_cs;
        read(fd, &old_cs, 8);
        if (cs == old_cs) { close(fd); return 1; }
        lseek(fd, 0, SEEK_SET);
        write(fd, &cs, 8);
        close(fd);
        return 0;
    }
    int fd = creat(CHECKSUM_FILE, 0644);
    assert(fd);
    write(fd, &cs, 8);
    close(fd);
    return 0;
}

static inline int is_a_c_file(char* buff, size_t sz) {
    return (!memcmp(buff + sz - 2, ".c", 2));
}

static inline int is_a_source_file(char* buff, size_t sz) {
    return (!memcmp(buff + sz - 2, ".c", 2) || !memcmp(buff + sz - 2, ".h", 2));
}

char_vec lcl_changed_files_charvec = {0};
char_vec lcl_sources_charvec = {0};

void char_push(char_vec* v, char* s, size_t sz) {
    if (v->cur + sz + 1>= v->capacity) {
        v->s = realloc(v->s, 2 * v->capacity);
        v->capacity *= 2;
    }
    memcpy(v->s + v->cur, s, sz);
    v->cur += sz;
    v->s[v->cur] = '\0';
}

static inline void get_dep_name(char* source_path, size_t sz) {
    char* c = NULL;
    // Chande / for _
    while((c = strchr(source_path, '/'))) {
        *c = '_';
    }
    // Change .c for .d
    source_path[sz - 1] = 'd';
}

#define MAX_LINE 1024

static int deps_have_changed(char* dot_d_path) {
    FILE* f = fopen(dot_d_path, "r");
    // If no .d file compile again.
    if (!f) return 1;
    char line[MAX_LINE];
    int seen_backslash = 0;
    int nl = 0;
    struct stat st;
    int exists = 0;

__init:
    while((fgets(line, MAX_LINE, f))) {
        seen_backslash = 0;
        nl = 0;
        char* ptr = line;

        if ((ptr = strchr(line, ':'))) {
            ptr++;
        } else {
            ptr = line;
        }
        
__read_tok:   
        while(*ptr == ' ') ptr++;
        char* start_tok = ptr;
        while(*ptr) {
            switch (*ptr) {
            case '\t':
            case ' ':break;
            case '\\':seen_backslash = 1; ptr++; continue;
            case '\n':
                nl = 1;
                break;
            default:
                ptr++;
                continue;
            }
            break;
        }
        *ptr = '\0';
        exists = stat(start_tok, &st) == 0;
        if (nl) {
            if (seen_backslash) goto __init;
            else break;
        }
        if (!exists || st.st_mtime > mark_modified_timestamp || mark_modified_timestamp == -1) {
            fclose(f);
            return 1;
        }
        ptr++;
        start_tok = ptr;
        goto __read_tok;
    }    
    fclose(f);
    return 0;
}


int has_changed(const char *fpath, const struct stat *sb, 
    int tflag, struct FTW *ftwbuf
) {
    (void)sb; (void)ftwbuf;
    size_t sz = 0;
    if (tflag != FTW_F) return 0;
    sz = strlen(fpath);
    if (is_a_source_file((char*)fpath, sz)) {
        char_push(&lcl_sources_charvec, (char*)fpath, sz + 1);
        lcl_sources_charvec.str_amnt += 1;
    }
    if (!is_a_c_file((char*)fpath, sz)) return 0;
    
    struct stat st;
    if (g_flags.force_compile) {
        goto __recompile;
    }
    int e = stat(fpath, &st) == 0;
    // First check: recompile if timestamp is greater
    if (!e || mark_modified_timestamp == -1 || st.st_mtime > mark_modified_timestamp) {
        goto __recompile;
    }
    // Check for .d file
    char buff[PATH_MAX];
    strcpy(buff, CACHE_DIR"/obj/");
    strcat(buff, fpath);
    get_dep_name(buff + sizeof(CACHE_DIR) + sizeof("/obj/") - 2, sz);
    if (deps_have_changed(buff)) {
        goto __recompile;
    
    }
    return 0;

__recompile:
    char_push(&lcl_changed_files_charvec, (char*)fpath, sz + 1);
    lcl_changed_files_charvec.str_amnt += 1;
    return 0; // Tell nftw to continue walking
}

size_t extract_sources_info(char* src_folder, char** out_files_changed[], char* out_all_sources_list[], size_t* out_total_sources) {
    assert(out_files_changed && out_all_sources_list);
    lcl_changed_files_charvec = (char_vec){.s = calloc(1, 50), .capacity = 50, .cur = 0};
    lcl_sources_charvec = (char_vec){.s = calloc(1, 50), .capacity = 50, .cur = 0};
    make_cache();
    if (nftw(src_folder, has_changed, 20, FTW_PHYS) == -1) {
        perror("nftw");
        return 0;
    }
    *out_files_changed = (char**)lcl_changed_files_charvec.s;
    *out_all_sources_list = lcl_sources_charvec.s;
    if (out_total_sources) *out_total_sources = lcl_sources_charvec.str_amnt;
    size_t n = lcl_changed_files_charvec.str_amnt;
    lcl_changed_files_charvec = (char_vec){0};
    lcl_sources_charvec = (char_vec){0};

    return n;
}

int remove_cache(const char *fpath, const struct stat *sb, 
    int tflag, struct FTW *ftwbuf
) {
    (void)ftwbuf; (void)sb; (void)tflag;
    int rv = remove(fpath);
    if (rv) log_fatal("Couldn't remove cache folder: %s", strerror(errno));
    return rv;
}
int cmd_clean() {
    return nftw(CACHE_DIR, remove_cache, 20, FTW_PHYS | FTW_DEPTH);
}