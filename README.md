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

## SOAL 2: Poke MOO

### Deskripsi Masalah
Praktikan ditugaskan untuk mengamankan sebuah layanan *database* dengan mengimplementasikan sistem file berbasis *Filesystem in Userspace* (FUSE). Terdapat dua direktori: `encrypted_storage` (sumber fisik) dan `fuse_mount` (titik virtual). Setiap file yang dibuat atau diubah di dalam direktori virtual harus terenkripsi secara otomatis menggunakan algoritma XOR (dengan kunci `0x76`) dan tersimpan dengan tambahan ekstensi `.enc` di direktori fisik. Direktori FUSE ini kemudian diintegrasikan sebagai penyimpanan basis data ke dalam sebuah aplikasi *server* yang dikontainerisasi menggunakan Docker. Untuk berinteraksi dengan *server* tersebut, sebuah program klien (`client.c`) juga perlu dibuat menggunakan protokol komunikasi TCP.

### Konsep Penyelesaian
Penyelesaian sistem pengamanan data ini menggunakan FUSE API versi 31 dengan pendekatan manipulasi *system call* tingkat pengguna. Operasi *bitwise* XOR diaplikasikan pada lapisan baca (`read`) dan tulis (`write`) sehingga proses enkripsi dan dekripsi berjalan secara transparan (tidak disadari oleh pengguna atau aplikasi). Pengelolaan nama file dikendalikan dengan mendeteksi dan memanipulasi *string path* guna menambah atau menghapus ekstensi `.enc`. Di sisi jaringan, `client.c` menggunakan *TCP Sockets* standar POSIX untuk terhubung ke porta `9000`. Untuk infrastruktur, `Dockerfile` berbasis Ubuntu digunakan untuk membungkus (*containerize*) *server* dan mengikat (*bind mount*) sistem file FUSE ke *path* `/app/db`.

### Penjelasan Kode dan Logika Program
1. **Manajemen Ekstensi dan Resolusi Path (`get_real_path` & `readdir`):** Fungsi pembantu `get_real_path()` bertugas menerjemahkan rute virtual menjadi rute fisik. Fungsi ini selalu mencoba menambahkan `.enc` terlebih dahulu (mengasumsikan target adalah file). Jika tidak ditemukan (menggunakan `stat`), maka diasumsikan target adalah direktori. Pada implementasi `xmp_readdir`, ekstensi `.enc` secara eksplisit dipotong (disensor) menggunakan *pointer string* (`*ext = '\0'`) sebelum ditampilkan ke layar, sehingga pengguna hanya melihat nama file normal. Pembuatan file baru melalui `xmp_create` juga dipaksa untuk menambahkan akhiran `.enc`.
```C
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

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    // ... [Inisialisasi DIR] ...
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
```
2. **Kriptografi Transparan (`xmp_read` & `xmp_write`):** Fungsi utilitas `xor_crypto()` melakukan iterasi untuk meng-XOR-kan setiap *byte* memori dengan konstanta `KEY = 0x76`. Pada saat instruksi sistem `write` dipanggil, memori *buffer* diduplikasi menggunakan `malloc` dan `memcpy`, lalu dienkripsi sebelum dilempar ke disk fisik melalui `pwrite()`. Sebaliknya, saat file diakses melalui `read`, data *ciphertext* diambil dari disk menggunakan `pread()`, langsung didekripsi di memori lokal, lalu diserahkan kembali kepada aplikasi. Hal ini membuat aplikasi pembaca tidak perlu tahu bahwa file tersebut sebenarnya terenkripsi.
```C
void xor_crypto(char *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] ^= KEY;
    }
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
```
3. **Komunikasi Client-Server (`client.c`):** Program `client.c` menginisiasi *socket* TCP (`AF_INET`, `SOCK_STREAM`) dan mencoba menyambungkan koneksi (`connect()`) ke `127.0.0.1` pada *port* 9000. Program ini beroperasi dalam sebuah *loop* tak berhingga yang menampilkan prapencetak kustom `db > `. Input baris perintah dari terminal dibaca, karakter *newline* dibuang, dan string tersebut dikirim melalui `send()`. Program kemudian masuk ke status penahanan (*blocking*) pada `read()` hingga *server* merespons, dan mencetak *output* tersebut ke layar. Siklus ini dapat dihentikan menggunakan komando `"exit"`.
```C
//masuk ke loop utama untuk meminta input
    while(1) {
        printf("db > ");
        memset(input, 0, BUFFER_SIZE);
        fgets(input, BUFFER_SIZE, stdin);
        
        // Menghapus karakter newline (enter) dari input
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "exit") == 0) {
            break;
        }

        // Mengirimkan perintah ke server
        send(sock, input, strlen(input), 0);
        
        // Membaca dan menampilkan jawaban dari server
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(sock, buffer, BUFFER_SIZE);
        if (valread > 0) {
            printf("%s\n", buffer);
        }
    }
```
4. **Integrasi Containerization (`Dockerfile`):** Dockerfile dirancang untuk menampung program *server* di direktori kerja `/app`. Skrip ini membuat direktori target `/app/db` yang nantinya digunakan sebagai titik singgung (titik *bind mount*) bagi direktori `fuse_mount` dari mesin induk (*host*). *Port* 9000 diekspos agar *container* dapat menerima lalu lintas dari program `client.c`.
```C
FROM ubuntu:latest
WORKDIR /app
COPY server /app/server
RUN chmod +x /app/server
RUN mkdir -p /app/db
EXPOSE 9000
CMD ["./server"]
```
