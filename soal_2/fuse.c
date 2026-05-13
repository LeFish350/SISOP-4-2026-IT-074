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
const char KEY = 0x76;

// Fungsi untuk Enkripsi dan Dekripsi XOR
void xor_crypto(char *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] ^= KEY;
    }
}

// Fungsi untuk mendapatkan path sebenarnya di encrypted_storage
void get_real_path(char *fpath, const char *path) {
    if (strcmp(path, "/") == 0) {
        sprintf(fpath, "%s", source_dir);
        return;
    }
    
    // Coba path dengan .enc terlebih dahulu (asumsi file)
    sprintf(fpath, "%s%s.enc", source_dir, path);
    struct stat st;
    if (stat(fpath, &st) != 0) {
        // Jika tidak ada, asumsi ini adalah direktori (tanpa .enc)
        sprintf(fpath, "%s%s", source_dir, path);
    }
}

static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    char fpath[1024];
    get_real_path(fpath, path);
    int res = lstat(fpath, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;
    char fpath[1024];
    get_real_path(fpath, path);

    DIR *dp = opendir(fpath);
    if (dp == NULL) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        char entry_name[256];
        strcpy(entry_name, de->d_name);

        // Hilangkan ekstensi .enc jika ada agar di fuse_mount terlihat normal
        char *ext = strstr(entry_name, ".enc");
        if (ext && strlen(ext) == 4) {
            *ext = '\0';
        }

        if (filler(buf, entry_name, &st, 0, 0)) break;
    }
    closedir(dp);
    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode) {
    char fpath[1024];
    sprintf(fpath, "%s%s", source_dir, path); // Folder tidak pakai .enc
    int res = mkdir(fpath, mode);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_rmdir(const char *path) {
    char fpath[1024];
    sprintf(fpath, "%s%s", source_dir, path);
    int res = rmdir(fpath);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char fpath[1024];
    sprintf(fpath, "%s%s.enc", source_dir, path); // File selalu ditambah .enc
    int res = creat(fpath, mode);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char fpath[1024];
    get_real_path(fpath, path);
    int res = open(fpath, fi->flags);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char fpath[1024];
    get_real_path(fpath, path);
    
    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    else xor_crypto(buf, res); // Dekripsi saat dibaca

    close(fd);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char fpath[1024];
    get_real_path(fpath, path);

    int fd = open(fpath, O_WRONLY);
    if (fd == -1) return -errno;

    // Enkripsi data sebelum ditulis ke disk
    char *enc_buf = malloc(size);
    memcpy(enc_buf, buf, size);
    xor_crypto(enc_buf, size);

    int res = pwrite(fd, enc_buf, size, offset);
    if (res == -1) res = -errno;

    free(enc_buf);
    close(fd);
    return res;
}

static int xmp_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    char fpath[1024];
    get_real_path(fpath, path);
    int res = truncate(fpath, size);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_unlink(const char *path) {
    char fpath[1024];
    get_real_path(fpath, path);
    int res = unlink(fpath);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_access(const char *path, int mask) {
    char fpath[1024];
    get_real_path(fpath, path);
    int res = access(fpath, mask);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi) {
    char fpath[1024];
    get_real_path(fpath, path);
    int res = utimensat(0, fpath, ts, AT_SYMLINK_NOFOLLOW);
    if (res == -1) return -errno;
    return 0;
}

static struct fuse_operations xmp_oper = {
    .getattr  = xmp_getattr,
    .readdir  = xmp_readdir,
    .mkdir    = xmp_mkdir,
    .rmdir    = xmp_rmdir,
    .create   = xmp_create,
    .open     = xmp_open,
    .read     = xmp_read,
    .write    = xmp_write,
    .truncate = xmp_truncate,
    .unlink   = xmp_unlink,
    .access   = xmp_access,
    .utimens  = xmp_utimens,
};

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;
    source_dir = realpath(argv[argc - 2], NULL);
    argv[argc - 2] = argv[argc - 1];
    argc--;
    int ret = fuse_main(argc, argv, &xmp_oper, NULL);
    free(source_dir);
    return ret;
}
