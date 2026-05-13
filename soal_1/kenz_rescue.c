#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

char *source_dir;

void generate_tujuan_content(char *output) {
    char combined[4096] = "";
    for (int i = 1; i <= 7; i++) {
        char filepath[1024];
        sprintf(filepath, "%s/%d.txt", source_dir, i);
        FILE *f = fopen(filepath, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "KOORD:", 6) == 0) {
                    char *fragment = line + 6;
                    if (fragment[0] == ' ') fragment++; 
                    fragment[strcspn(fragment, "\r\n")] = 0;
                    strcat(combined, fragment);
                    break; 
                }
            }
            fclose(f);
        }
    }
    sprintf(output, "Tujuan Mas Amba: %s\n", combined);
}

static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/tujuan.txt") == 0) {
        stbuf->st_mode = S_IFREG | 0444; 
        stbuf->st_nlink = 1;
        char content[4096];
        generate_tujuan_content(content);
        stbuf->st_size = strlen(content);
        return 0;
    }
    char fpath[1024];
    sprintf(fpath, "%s%s", source_dir, path);
    int res = lstat(fpath, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;
    char fpath[1024];
    sprintf(fpath, "%s%s", source_dir, path);
    DIR *dp = opendir(fpath);
    if (dp == NULL) return -errno;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0)) break;
    }
    closedir(dp);
    if (strcmp(path, "/") == 0) filler(buf, "tujuan.txt", NULL, 0, 0);
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY) return -EACCES;
        return 0;
    }
    char fpath[1024];
    sprintf(fpath, "%s%s", source_dir, path);
    int res = open(fpath, fi->flags);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        char content[4096];
        generate_tujuan_content(content);
        size_t len = strlen(content);
        if (offset < len) {
            if (offset + size > len) size = len - offset;
            memcpy(buf, content + offset, size);
        } else {
            size = 0;
        }
        return size;
    }
    char fpath[1024];
    sprintf(fpath, "%s%s", source_dir, path);
    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    close(fd);
    return res;
}

static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .open    = xmp_open,
    .read    = xmp_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;
    char *source_arg = argv[argc - 2];
    source_dir = realpath(source_arg, NULL);
    if (!source_dir) return 1;
    argv[argc - 2] = argv[argc - 1];
    argc--;
    int ret = fuse_main(argc, argv, &xmp_oper, NULL);
    free(source_dir);
    return ret;
}
