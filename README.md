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
2. **Fungsi `readdir`:** Fungsi ini bertanggung jawab menampilkan isi direktori. Selain membaca dan memasukkan isi direktori asli ke dalam *buffer* pengisian menggunakan fungsi `filler()`, program secara eksplisit menambahkan *entry* bernama `tujuan.txt`. Hal ini memastikan file virtual tersebut muncul ketika *user* mengeksekusi perintah `ls` di direktori *mount*.
3. **Fungsi `read`:** Jika file yang dibaca adalah file biasa, FUSE meneruskannya menggunakan fungsi `pread()`. Akan tetapi, jika target pembacaannya adalah `/tujuan.txt`, program mendeklarasikan sebuah *buffer*, menyisipkan teks `"Tujuan Mas Amba: "`, lalu melakukan iterasi (*looping*) untuk membuka `1.txt` hingga `7.txt`. Isi dari masing-masing file tersebut dibaca, digabungkan ke dalam *buffer* memori menggunakan manipulasi *string*, dan akhirnya diserahkan ke *buffer* milik *user*.
