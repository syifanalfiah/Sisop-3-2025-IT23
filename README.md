# Laporan Praktikum Modul 3

## Nama Anggota

| Nama                        | NRP        |
| --------------------------- | ---------- |
| Syifa Nurul Alfiah          | 5027241019 |
| Alnico Virendra Kitaro Diaz | 5027241081 |
| Hafiz Ramadhan              | 5027241096 |

## Soal No 1

## Soal No 2

## Soal No 3

## Soal No 4

### Deskripsi

Proyek ini merupakan sistem simulasi "hunter dan dungeon" yang berjalan di atas shared memory menggunakan bahasa C. Sistem ini terdiri dari dua komponen:

* `system.c`: program admin untuk mengelola dungeon dan hunter.
* `hunter.c`: program client untuk hunter yang dapat mendaftar, login, raid dungeon, dan battle.

Shared memory dan semaphore digunakan untuk memastikan sinkronisasi antar proses yang berbeda.

### Struktur File

* `system.c`: admin controller, dungeon generator, pengelola hunter (ban, reset, info).
* `hunter.c`: hunter manager untuk registrasi, login, raid, battle, dan notifikasi.
* `shm_common.h`: definisi struktur data dan key shared memory.

### 1. Inisialisasi Shared Memory & Semaphore

```c
void init_shared_memory();
void init_semaphore();
void lock();
void unlock();
```

Fungsi-fungsi ini digunakan untuk mengakses/membuat shared memory dan mengatur semaphore.

* `lock()` dan `unlock()` menjamin akses eksklusif saat data dibaca/tulis.

### 2. Registrasi dan Login Hunter

```c
void register_hunter();
void login_hunter();
```

* `register_hunter`: menyimpan hunter baru dengan stats awal dan key unik.
* `login_hunter`: hunter dapat login jika tidak di-ban.

### 3. Notifikasi Dungeon (Multithreading)

```c
void* notification_thread(void* arg);
void toggle_notification();
```

* Thread berjalan menampilkan dungeon setiap 3 detik jika level hunter mencukupi.
* Bisa diaktifkan atau dimatikan melalui toggle.

### 4. Generate dan Tampilkan Dungeon

```c
void generate_dungeon();
void show_dungeons();
```

* Admin bisa membuat dungeon dengan nama dan stat reward acak.
* Semua dungeon ditampilkan lengkap beserta info reward dan key.

### 5. Info dan Pengelolaan Hunter

```c
void show_hunters();
void ban_hunter();
void reset_hunter();
```

* Menampilkan semua hunter (username, stats, status banned).
* `ban_hunter`: mem-ban atau membuka ban hunter.
* `reset_hunter`: mengembalikan stat hunter ke awal.

### 6. Fitur Dungeon untuk Hunter

```c
void show_available_dungeons();
void raid_dungeon();
```

* Hanya dungeon dengan level minimum ≤ level hunter yang ditampilkan.
* Jika raid berhasil, hunter mendapat reward dan exp (naik level jika exp ≥ 500).

### 7. Battle Hunter

```c
void battle_hunter();
```

* Hunter dapat memilih lawan.
* Pemenang mendapatkan semua stat lawan, yang kalah dihapus dari sistem.

### 8. Shutdown dan Logout

```c
void cleanup(int sig);
void logout();
```

* Membersihkan shared memory dan semaphore saat program dimatikan.
* Logout melepaskan hubungan dengan shared memory.

### 9. Spesifikasi Berdasarkan Cerita Soal

| Fitur                | Deskripsi                                                    |
| -------------------- | ------------------------------------------------------------ |
| **Registrasi**       | Stats awal: Level 1, EXP 0, ATK 10, HP 100, DEF 5, key unik. |
| **Info Hunter**      | Tampilkan semua hunter terdaftar.                            |
| **Generate Dungeon** | Level 1-5, ATK 100-150, HP 50-100, DEF 25-50, EXP 150-300.   |
| **Raid Dungeon**     | Dungeon dihapus, stat hunter bertambah.                      |
| **Battle Hunter**    | Winner ambil semua stat, loser dihapus.                      |
| **Ban/Reset**        | Admin bisa membatasi atau reset hunter.                      |
| **Notifikasi**       | Menampilkan dungeon yang bisa diakses setiap 3 detik.        |
| **Keamanan**         | Shared memory dihapus saat program dimatikan.                |

### Revisi
penambahan
```c
#include <stdatomic.h>
```
