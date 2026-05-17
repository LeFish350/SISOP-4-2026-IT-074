# LAPORAN RESMI PRAKTIKUM SISTEM OPERASI - MODUL 4

**Nama:** Mahrinza Redouane Zakariyah

**NRP:** 5027251074

**Kelas:** Sistem Operasi A

## SOAL 1: Save Asisten Kenz

### Deskripsi Masalah
Pada soal ini, praktikan diminta untuk membangun sebuah *Filesystem in Userspace* (FUSE) bernama `kenz_rescue`. Program ini bertujuan untuk membantu mencari Asisten Kenz yang hilang dengan memecahkan teka-teki koordinat yang terpecah ke dalam 7 file teks (`1.txt` hingga `7.txt`) di dalam sebuah direktori sumber (flashdisk). Sistem file virtual ini harus melakukan *passthrough* dari direktori sumber ke direktori *mount*, dengan syarat khusus: sistem harus membangkitkan file virtual bernama `tujuan.txt` secara dinamis. File virtual ini harus berisi gabungan koordinat rahasia yang diekstrak dari baris berawalan `"KOORD:"` pada ketujuh file tersebut, dirangkai menjadi satu kalimat dengan *prefix* `"Tujuan Mas Amba: "`.

### Konsep Penyelesaian
Penyelesaian ini menggunakan API FUSE versi 31. Konsep utamanya adalah memanipulasi fungsi *callback* FUSE (`getattr`, `readdir`, `open`, dan `read`) agar memalsukan keberadaan file `tujuan.txt`. File-file fisik di direktori asal diakses dengan meneruskan (*passthrough*) *system call* POSIX ke direktori sumber yang di-*resolve* menggunakan `realpath()`. Ketika sistem mengakses file `tujuan.txt`, FUSE akan mencegat (*intercept*) permintaan tersebut. Sebuah fungsi *helper* bernama `generate_tujuan_content` akan dipanggil untuk membaca ketujuh file fisik secara *real-time*, melakukan *parsing* teks untuk membuang string awalan `"KOORD:"` dan karakter *newline*, lalu menyatukannya ke dalam satu *buffer* memori.

### Penjelasan Kode dan Logika Program
1. **Ekstraksi String (`generate_tujuan_content`):** Ini adalah fungsi inti yang dipanggil saat ukuran atau isi file virtual dibutuhkan. Fungsi ini melakukan iterasi untuk membuka file `1.txt` hingga `7.txt`. Di dalam setiap file, ia membaca baris demi baris menggunakan `fgets()` dan mencari baris yang memiliki awalan `"KOORD:"` menggunakan `strncmp()`. Setelah ditemukan, teks koordinat diekstrak, dibersihkan dari spasi awal dan karakter *enter* (`\r\n`), lalu digabungkan (*concatenate*) ke dalam sebuah variabel statis.

```C
void generate_tujuan_content(char *output) {
    char combined[4096] = "";
    for (int i = 1; i <= 7; i++) {
        char filepath[1024];
        sprintf(filepath, "%s/%d.txt", source_dir, i);
        FILE *f = fopen(filepath, "r");
        if (f) {
            char line[256];
            // Membaca baris demi baris
            while (fgets(line, sizeof(line), f)) {
                // Memfilter hanya baris yang mengandung awalan "KOORD:"
                if (strncmp(line, "KOORD:", 6) == 0) {
                    char *fragment = line + 6;
                    if (fragment[0] == ' ') fragment++; // Menghilangkan spasi setelah titik dua
                    fragment[strcspn(fragment, "\r\n")] = 0; // Menghilangkan newline/enter
                    strcat(combined, fragment); // Menyambungkan string
                    break; 
                }
            }
            fclose(f);
        }
    }
    // Merakit hasil akhir
    sprintf(output, "Tujuan Mas Amba: %s\n", combined);
}
```

2. **Metadata Virtual (`xmp_getattr`):** Saat meminta atribut file, jika *path* yang dituju adalah `/tujuan.txt`, FUSE mengembalikan struktur metadata buatan secara manual dengan hak akses *read-only* `0444`. Ukuran file (parameter `st_size`) dikalkulasi secara dinamis saat itu juga dengan menghitung panjang *string* keluaran dari fungsi ekstraksi.

```C
static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/tujuan.txt") == 0) {
        stbuf->st_mode = S_IFREG | 0444; 
        stbuf->st_nlink = 1;
        
        char content[4096];
        generate_tujuan_content(content); // Men-generate string untuk mengukur sizenya
        stbuf->st_size = strlen(content);
        return 0;
    }
    
    // Passthrough untuk file asli
    char fpath[1024];
    sprintf(fpath, "%s%s", source_dir, path);
    int res = lstat(fpath, stbuf);
    if (res == -1) return -errno;
    return 0;
}
```

3. **Pendaftaran Direktori (`xmp_readdir`):** Fungsi ini menampilkan isi direktori sumber menggunakan pembacaan `opendir` dan `readdir` standar. Sebagai tambahan, program menggunakan fungsi `filler()` untuk menyisipkan entri `/tujuan.txt` secara eksplisit agar muncul saat pengguna melakukan perintah `ls` di terminal.

```C
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    // Jika sedang berada di root folder (mount point), tambahkan tujuan.txt
    if (strcmp(path, "/") == 0) filler(buf, "tujuan.txt", NULL, 0, 0);
    return 0;
}
```

4. **Pembacaan Virtual (`xmp_read`):** Jika file yang dibaca adalah file reguler, FUSE meneruskannya dengan fungsi `pread()`. Namun, jika yang dibaca adalah `/tujuan.txt`, FUSE mengeksekusi fungsi `generate_tujuan_content` untuk mendapatkan hasil kalimat akhir, dan menyalinnya ke *buffer* sistem pengguna berdasarkan besaran `offset` dan `size` yang diminta.

```C
static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        char content[4096];
        generate_tujuan_content(content);
        size_t len = strlen(content);
        
        // Memastikan pembacaan tidak melebihi batas (buffer overflow)
        if (offset < len) {
            if (offset + size > len) size = len - offset;
            memcpy(buf, content + offset, size);
        } else {
            size = 0;
        }
        return size;
    }
    // ... [kode passthrough file normal] ...
}
```
