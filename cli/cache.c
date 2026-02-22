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

#include "cli.h"



static inline void make_cache() {
    struct stat st = {0};
    if (stat(CACHE_DIR, &st) == -1)
        mkdir(CACHE_DIR, 0700);
}

static inline uint32_t fnv1a(const char* name, size_t sz) {
    uint32_t h = 0x811c9dc5;
    for (size_t i = 0; i < sz; i++)
    {
        uint8_t b = name[i];
        h ^= b;
        h = (h * 0x01000193) & 0xffffffff;
    }
    
    return h;
}

static inline int __is_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

static inline uint64_t checksumfile(const char* file) {
    FILE* f = fopen(file, "rb");
    if (!f) return 0;
    uint64_t hash = 5381;
    int c;
    while ((c = fgetc(f)) != EOF) {
        hash = ((hash << 5) + hash) + (uint8_t)c;
    }
    fclose(f);
    return hash;
}

static inline int valid_extension(char* buff, size_t sz) {
    return (!memcmp(buff + sz - 2, ".c", 2));
}


typedef struct {
    char* s;
    size_t cur;
    size_t capacity;
    size_t str_amnt;
} char_vec;

char_vec lcl_charvec = {0};

static void char_push(char_vec* v, char* s, size_t sz) {
    if (v->cur + sz >= v->capacity) {
        v->s = realloc(v->s, 2 * v->capacity);
        v->capacity *= 2;
    }
    memcpy(v->s + v->cur, s, sz);
    v->cur += sz;
}

int checksum_matches(char* cache_file_name, char* src_file) {
    char p[4096];
    sprintf(p, "%s/%s", CACHE_DIR, cache_file_name);
    uint64_t src = checksumfile(src_file);

    int fd = open(p, O_RDONLY);
    if (fd <= 0) goto __set_hash;

    uint64_t cached = 0;

    read(fd, &cached, 8);
    if (src == cached) return 1;

__set_hash:
    fd = open(p, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    write(fd, &src, 8);
    return 0;
}

int has_changed(const char *fpath, const struct stat *sb, 
    int tflag, struct FTW *ftwbuf
) {
    (void)sb; (void)ftwbuf;
    if (tflag == FTW_F) {
        size_t sz = strlen(fpath);
        if (valid_extension((char*)fpath, sz)) {
            uint32_t hash_name = fnv1a(fpath, sz);
            char buff[9];
            sprintf(buff, "%08x", hash_name);
            if (!checksum_matches(buff, (char*)fpath) || force_cache) {
                char_push(&lcl_charvec, (char*)fpath, sz + 1);
                lcl_charvec.str_amnt += 1;
            }
        }
    }
    return 0; // Tell nftw to continue walking
}

char** get_changed_files(char* src_folder, size_t* out_n_files) {
    lcl_charvec = (char_vec){.s = calloc(1, 50), .capacity = 50, .cur = 0};
    make_cache();
    if (nftw(src_folder, has_changed, 20, FTW_PHYS) == -1) {
        perror("nftw");
        return NULL;
    }
    char** ret = (char**)lcl_charvec.s;
    if (out_n_files) *out_n_files = lcl_charvec.str_amnt;
    lcl_charvec = (char_vec){0};
    return ret;
}

void delete_cache_err(char** files, size_t n_files) {
    char* b = *files;
    char buff[PATH_MAX];
    for (size_t i = 0; i < n_files; i++)
    {
        size_t sz = strlen(b);
        uint32_t hash_name = fnv1a(b, sz);
        snprintf(buff, PATH_MAX, CACHE_DIR"/%08x", hash_name);
        remove(buff);
        b += sz + 1;
    }
    
}