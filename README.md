# LAPORAN RESMI PRAKTIKUM SISTEM OPERASI - MODUL 4

**Nama:** Mahrinza Redouane Zakariyah
**NRP:** 5027251074
**Kelas:** Sistem Operasi A

## SOAL 1: Save Asisten Kenz

### Deskripsi Masalah
Pada soal ini, praktikan diminta untuk membangun sebuah *Filesystem in Userspace* (FUSE) bernama `kenz_rescue`. Program ini bertujuan untuk membantu mencari Asisten Kenz yang hilang dengan memecahkan teka-teki koordinat yang terpecah ke dalam 7 file teks (`1.txt` hingga `7.txt`) di dalam sebuah direktori sumber. Sistem file virtual ini harus melakukan *passthrough* dari direktori sumber ke direktori *mount*, dengan sebuah syarat khusus: sistem harus secara dinamis membangkitkan sebuah file virtual bernama `tujuan.txt` (*on-the-fly*) yang berisi gabungan isi ketujuh file tersebut beserta sebuah *prefix* string, tanpa membuat file `tujuan.txt` tersebut secara fisik di dalam direktori sumber (flashdisk).

### Konsep Penyelesaian
Penyelesaian masalah ini menggunakan FUSE di bahasa pemrograman C. Konsep utamanya adalah memanipulasi fungsi *callback* FUSE (`getattr`, `readdir`, `open`, dan `read`) agar memalsukan dan merekayasa keberadaan file `tujuan.txt`. File-file fisik di direktori asal diakses dengan meneruskan (*passthrough*) *system call* POSIX standar. Namun, ketika *user* atau sistem mengakses file `tujuan.txt`, FUSE akan mencegat (*intercept*) permintaan tersebut, lalu memproses pembacaan ketujuh file asli dan menyatukannya langsung di dalam memori saat *runtime*.

### Penjelasan Kode dan Logika Program
1. **Fungsi `getattr`:** Saat *system call* meminta metadata atau atribut file, program akan mengecek apakah *path* yang direkuisisi adalah `/tujuan.txt`. Jika iya, FUSE mengembalikan struktur metadata buatan secara manual (seperti memberikan hak akses *read-only* `0444`). Untuk file lainnya, *path* akan ditranslasikan dan diteruskan ke direktori sumber menggunakan fungsi `lstat()`.
```C
static int xmp_getattr(const char *path, struct stat *stbuf) {
    int res;
    char fpath[1000];
    sprintf(fpath, "%s%s", dirpath, path);

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/tujuan.txt") == 0) {
        stbuf->st_mode = S_IFREG | 0444; 
        stbuf->st_nlink = 1;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_size = 1024;
        return 0;
    }

    // Passthrough untuk file lain (1.txt - 7.txt)
    res = lstat(fpath, stbuf);
    if (res == -1) return -errno;

    return 0;
}
```
2. **Fungsi `readdir`:** Fungsi ini bertanggung jawab menampilkan isi direktori. Selain membaca dan memasukkan isi direktori asli ke dalam *buffer* pengisian menggunakan fungsi `filler()`, program secara eksplisit menambahkan *entry* bernama `tujuan.txt`. Hal ini memastikan file virtual tersebut muncul ketika *user* mengeksekusi perintah `ls` di direktori *mount*.
```C
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    char fpath[1000];
    if(strcmp(path,"/") == 0) {
        path = dirpath;
        sprintf(fpath,"%s",path);
    } else {
        sprintf(fpath, "%s%s", dirpath, path);
    }

    DIR *dp;
    struct dirent *de;
    (void) offset;
    (void) fi;

    dp = opendir(fpath);
    if (dp == NULL) return -errno;

    
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        filler(buf, de->d_name, &st, 0);
    }

    // Menambahkan entri tujuan.txt secara virtual di root folder mount
    if (strcmp(path, dirpath) == 0) {
        filler(buf, "tujuan.txt", NULL, 0);
    }

    closedir(dp);
    return 0;
}
```
3. **Fungsi `read`:** Jika file yang dibaca adalah file biasa, FUSE meneruskannya menggunakan fungsi `pread()`. Akan tetapi, jika target pembacaannya adalah `/tujuan.txt`, program mendeklarasikan sebuah *buffer*, menyisipkan teks `"Tujuan Mas Amba: "`, lalu melakukan iterasi (*looping*) untuk membuka `1.txt` hingga `7.txt`. Isi dari masing-masing file tersebut dibaca, digabungkan ke dalam *buffer* memori menggunakan manipulasi *string*, dan akhirnya diserahkan ke *buffer* milik *user*.
```C
static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char fpath[1000];
    sprintf(fpath, "%s%s", dirpath, path);
    int res = 0;
    int fd = 0;

    
    if (strcmp(path, "/tujuan.txt") == 0) {
        char temp_buffer[2048] = "Tujuan Mas Amba: "; 
        char frag_path[1000];
        char frag_buf[256];

        // Looping untuk membuka dan membaca file 1.txt hingga 7.txt
        for (int i = 1; i <= 7; i++) {
            sprintf(frag_path, "%s/%d.txt", dirpath, i);
            FILE *fp = fopen(frag_path, "r");
            if (fp != NULL) {
                memset(frag_buf, 0, sizeof(frag_buf));
                fread(frag_buf, 1, sizeof(frag_buf) - 1, fp); 
                strcat(temp_buffer, frag_buf);
                fclose(fp);
            }
        }

        int len = strlen(temp_buffer);
        if (offset < len) {
            if (offset + size > len) size = len - offset;
            memcpy(buf, temp_buffer + offset, size);
        } else {
            size = 0;
        }
        return size;
    }

    // Passthrough untuk membaca file biasa secara normal
    (void) fi;
    fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;

    res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;

    close(fd);
    return res;
}
```

