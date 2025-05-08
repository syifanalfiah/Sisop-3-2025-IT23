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

### Inisialisasi Shared Memory & Semaphore

```c
/system
void init_shared_memory() {
    key_t key = get_system_key();
    shmid = shmget(key, sizeof(struct SystemData), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    shared_data = (struct SystemData *)shmat(shmid, NULL, 0);
    if (shared_data == (void *)-1) {
        perror("shmat failed");
        exit(1);
    }
    if (shared_data->num_hunters == 0) {
        shared_data->num_hunters = 0;
        shared_data->num_dungeons = 0;
        shared_data->current_notification_index = 0;
    }
}

void init_semaphore() {
    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget failed");
        exit(1);
    }
    semctl(semid, 0, SETVAL, 1);
}

void lock() {
    struct sembuf sb = {0, -1, 0};
    semop(semid, &sb, 1);
}

void unlock() {
    struct sembuf sb = {0, 1, 0};
    semop(semid, &sb, 1);
}
```

Fungsi-fungsi ini digunakan untuk mengakses/membuat shared memory dan mengatur semaphore.

* `lock()` dan `unlock()` menjamin akses eksklusif saat data dibaca/tulis.

### Registrasi dan Login Hunter

```c
/hunter
void register_hunter() {
    struct Hunter new_hunter;
    printf("
=== REGISTRASI HUNTER ===
");
    printf("Username: ");
    fgets(new_hunter.username, 50, stdin);
    new_hunter.username[strcspn(new_hunter.username, "
")] = 0;

    new_hunter.level = 1;
    new_hunter.exp = 0;
    new_hunter.atk = 10;
    new_hunter.hp = 100;
    new_hunter.def = 5;
    new_hunter.banned = 0;
    new_hunter.shm_key = rand() % 10000;

    lock();
    for (int i = 0; i < shared_data->num_hunters; i++) {
        if (strcmp(shared_data->hunters[i].username, new_hunter.username) == 0) {
            printf("Username sudah digunakan!
");
            unlock();
            return;
        }
    }
    if (shared_data->num_hunters >= MAX_HUNTERS) {
        printf("Kapasitas hunter penuh!
");
        unlock();
        return;
    }
    shared_data->hunters[shared_data->num_hunters++] = new_hunter;

    printf("
Registrasi berhasil!
");
    printf("Selamat datang, @%s
", new_hunter.username);
    printf("🔑 Key Anda: %d
", new_hunter.shm_key);
    printf("Stats awal:
");
    printf("🎚 Level: 1 | 🌟 EXP: 0/500
");
    printf("⚔ ATK: 10 | ❤ HP: 100 | 🛡 DEF: 5
");

    current_hunter = new_hunter;
    logged_in = 1;
    unlock();
}

void login_hunter() {
    char username[50];
    printf("
=== LOGIN HUNTER ===
");
    printf("Username: ");
    fgets(username, 50, stdin);
    username[strcspn(username, "
")] = 0;

    lock();
    for (int i = 0; i < shared_data->num_hunters; i++) {
        if (strcmp(shared_data->hunters[i].username, username) == 0) {
            if (shared_data->hunters[i].banned) {
                printf("Akun ini diban! Tidak bisa login.
");
                unlock();
                return;
            }
            current_hunter = shared_data->hunters[i];
            logged_in = 1;
            printf("
Login berhasil!
");
            printf("Selamat datang kembali, @%s
", current_hunter.username);
            printf("🎚 Level: %d | 🌟 EXP: %d/500
", current_hunter.level, current_hunter.exp);
            printf("⚔ ATK: %d | ❤ HP: %d | 🛡 DEF: %d
",
                   current_hunter.atk, current_hunter.hp, current_hunter.def);
            unlock();
            return;
        }
    }
    printf("Username tidak ditemukan!
");
    unlock();
}
```

* `register_hunter`: menyimpan hunter baru dengan stats awal dan key unik.
* `login_hunter`: hunter dapat login jika tidak di-ban.

### Notifikasi Dungeon (Multithreading)

```c
/hunter
void* notification_thread(void* arg) {
    while (atomic_load(&notification_on)) {
        lock();
        
        if (shared_data->num_dungeons > 0) {
            printf("\n🔔 NOTIFIKASI DUNGEON 🔔\n");
            for (int i = 0; i < shared_data->num_dungeons; i++) {
                if (shared_data->dungeons[i].min_level <= current_hunter.level) {
                    printf("🏰 %s (Lvl %d) - ⚔️%d ❤️%d 🛡️%d 🌟%d\n",
                           shared_data->dungeons[i].name,
                           shared_data->dungeons[i].min_level,
                           shared_data->dungeons[i].atk,
                           shared_data->dungeons[i].hp,
                           shared_data->dungeons[i].def,
                           shared_data->dungeons[i].exp);
                }
            }
            printf("\nTekan enter untuk kembali ke menu...");
            fflush(stdout);
        }
        
        unlock();
        
        time_t start = time(NULL);
        while (time(NULL) - start < 3 && atomic_load(&notification_on)) {
            usleep(100000);
        }
    }
    return NULL;
}

void toggle_notification() {
    if (atomic_load(&notification_on)) {
        atomic_store(&notification_on, 0);
        pthread_join(notif_thread, NULL);
        printf("\n🔕 Notifikasi dungeon NONAKTIF\n");
    } else {
        atomic_store(&notification_on, 1);
        printf("\n🔔 Notifikasi dungeon AKTIF\n");
        pthread_create(&notif_thread, NULL, notification_thread, NULL);
    }
}
```

* Thread berjalan menampilkan dungeon setiap 3 detik jika level hunter mencukupi.
* Bisa diaktifkan atau dimatikan melalui toggle.

### Generate dan Tampilkan Dungeon

```c
/system
void generate_dungeon() {
    lock();
    
    if (shared_data->num_dungeons >= MAX_DUNGEONS) {
        printf("Dungeon sudah penuh!\n");
        unlock();
        return;
    }

    struct Dungeon new_dungeon;
    
    char *prefixes[] = {"Goblin", "Dragon", "Demon", "Undead", "Elemental"};
    char *suffixes[] = {"Cave", "Lair", "Nest", "Sanctum", "Fortress"};
    sprintf(new_dungeon.name, "%s %s", prefixes[rand()%5], suffixes[rand()%5]);
    
    new_dungeon.min_level = (rand() % 5) + 1;
    new_dungeon.atk = (rand() % 51) + 100;
    new_dungeon.hp = (rand() % 51) + 50;
    new_dungeon.def = (rand() % 26) + 25;
    new_dungeon.exp = (rand() % 151) + 150;
    new_dungeon.shm_key = rand() % 10000;
    
    shared_data->dungeons[shared_data->num_dungeons++] = new_dungeon;
    
    printf("\n🏰 Dungeon baru telah muncul! 🏰\n");
    printf("Nama: %s\n", new_dungeon.name);
    printf("🏆 Level Minimal: %d\n", new_dungeon.min_level);
    printf("⚔️ ATK Reward: %d\n", new_dungeon.atk);
    printf("❤️ HP Reward: %d\n", new_dungeon.hp);
    printf("🛡️ DEF Reward: %d\n", new_dungeon.def);
    printf("🌟 EXP Reward: %d\n", new_dungeon.exp);
    
    unlock();
}

void show_dungeons() {
    lock();
    
    printf("\n🔍 Daftar Semua Dungeon 🔍\n");
    for (int i = 0; i < shared_data->num_dungeons; i++) {
        struct Dungeon d = shared_data->dungeons[i];
        printf("\n%d. %s\n", i+1, d.name);
        printf("   🏆 Level Minimal: %d\n", d.min_level);
        printf("   ⚔️ ATK: %d | ❤️ HP: %d | 🛡️ DEF: %d | 🌟 EXP: %d\n", 
               d.atk, d.hp, d.def, d.exp);
        printf("   🔑 Key: %d\n", d.shm_key);
    }
    
    unlock();
}
```

* Admin bisa membuat dungeon dengan nama dan stat reward acak.
* Semua dungeon ditampilkan lengkap beserta info reward dan key.

### Info dan Pengelolaan Hunter

```c
/system
void show_hunters() {
    lock();
    
    printf("\n👥 Daftar Semua Hunter 👥\n");
    for (int i = 0; i < shared_data->num_hunters; i++) {
        struct Hunter h = shared_data->hunters[i];
        printf("\n%d. @%s\n", i+1, h.username);
        printf("   🎚 Level: %d | 🌟 EXP: %d/500\n", h.level, h.exp);
        printf("   ⚔️ ATK: %d | ❤️ HP: %d | 🛡️ DEF: %d\n", h.atk, h.hp, h.def);
        printf("   🔑 Key: %d | %s\n", h.shm_key, h.banned ? "🚫 BANNED" : "✅ Active");
    }
    
    unlock();
}

void ban_hunter() {
    show_hunters();
    printf("\nMasukkan nomor hunter yang ingin di-ban/unban: ");
    int choice;
    scanf("%d", &choice);
    getchar();
    
    if (choice < 1 || choice > shared_data->num_hunters) {
        printf("Nomor hunter tidak valid!\n");
        return;
    }
    
    lock();
    
    int idx = choice - 1;
    shared_data->hunters[idx].banned = !shared_data->hunters[idx].banned;
    
    printf("@%s telah %s\n", 
           shared_data->hunters[idx].username,
           shared_data->hunters[idx].banned ? "DIBAN" : "DIUNBAN");
    
    unlock();
}

void reset_hunter() {
    show_hunters();
    printf("\nMasukkan nomor hunter yang ingin di-reset: ");
    int choice;
    scanf("%d", &choice);
    getchar();
    
    if (choice < 1 || choice > shared_data->num_hunters) {
        printf("Nomor hunter tidak valid!\n");
        return;
    }
    
    lock();
    
    int idx = choice - 1;
    shared_data->hunters[idx].level = 1;
    shared_data->hunters[idx].exp = 0;
    shared_data->hunters[idx].atk = 10;
    shared_data->hunters[idx].hp = 100;
    shared_data->hunters[idx].def = 5;
    
    printf("Stats @%s telah direset ke awal!\n", 
           shared_data->hunters[idx].username);
    
    unlock();
}
```

* Menampilkan semua hunter (username, stats, status banned).
* `ban_hunter`: mem-ban atau membuka ban hunter.
* `reset_hunter`: mengembalikan stat hunter ke awal.

### Fitur Dungeon untuk Hunter

```c
/hunter
void show_available_dungeons() {
    lock();
    
    printf("\n🏰 DUNGEON TERSEDIA (Level %d) 🏰\n", current_hunter.level);
    int available = 0;
    
    for (int i = 0; i < shared_data->num_dungeons; i++) {
        if (shared_data->dungeons[i].min_level <= current_hunter.level) {
            printf("\n%d. %s\n", available+1, shared_data->dungeons[i].name);
            printf("   🏆 Level Minimal: %d\n", shared_data->dungeons[i].min_level);
            printf("   ⚔️ ATK: %d | ❤️ HP: %d | 🛡️ DEF: %d | 🌟 EXP: %d\n", 
                   shared_data->dungeons[i].atk,
                   shared_data->dungeons[i].hp,
                   shared_data->dungeons[i].def,
                   shared_data->dungeons[i].exp);
            available++;
        }
    }
    
    if (available == 0) {
        printf("Tidak ada dungeon yang tersedia untuk level Anda.\n");
    }
    
    unlock();
}

void raid_dungeon() {
    show_available_dungeons();
    
    printf("\nPilih dungeon yang ingin diraid (0 untuk batal): ");
    int choice;
    scanf("%d", &choice);
    getchar();
    
    if (choice == 0) return;
    
    lock();
    
    int available_idx = 0;
    struct Dungeon *selected = NULL;
    int dungeon_idx = -1;
    
    for (int i = 0; i < shared_data->num_dungeons; i++) {
        if (shared_data->dungeons[i].min_level <= current_hunter.level) {
            available_idx++;
            if (available_idx == choice) {
                selected = &shared_data->dungeons[i];
                dungeon_idx = i;
                break;
            }
        }
    }
    
    if (selected == NULL) {
        printf("Pilihan tidak valid!\n");
        unlock();
        return;
    }
    
    current_hunter.exp += selected->exp;
    current_hunter.atk += selected->atk;
    current_hunter.hp += selected->hp;
    current_hunter.def += selected->def;
    
    if (current_hunter.exp >= 500) {
        current_hunter.level++;
        current_hunter.exp = 0;
        printf("\n🎉 LEVEL UP! Sekarang Level %d 🎉\n", current_hunter.level);
    }
    
    for (int i = dungeon_idx; i < shared_data->num_dungeons - 1; i++) {
        shared_data->dungeons[i] = shared_data->dungeons[i+1];
    }
    shared_data->num_dungeons--;
    
    for (int i = 0; i < shared_data->num_hunters; i++) {
        if (strcmp(shared_data->hunters[i].username, current_hunter.username) == 0) {
            shared_data->hunters[i] = current_hunter;
            break;
        }
    }
    
    printf("\n🏆 RAID BERHASIL! 🏆\n");
    printf("Anda mendapatkan:\n");
    printf("⚔️ ATK +%d | ❤️ HP +%d | 🛡️ DEF +%d | 🌟 EXP +%d\n",
           selected->atk, selected->hp, selected->def, selected->exp);
    printf("\nStats Anda sekarang:\n");
    printf("🎚 Level: %d | 🌟 EXP: %d/500\n", current_hunter.level, current_hunter.exp);
    printf("⚔️ ATK: %d | ❤️ HP: %d | 🛡️ DEF: %d\n", 
           current_hunter.atk, current_hunter.hp, current_hunter.def);
    
    unlock();
}
```

* Hanya dungeon dengan level minimum ≤ level hunter yang ditampilkan.
* Jika raid berhasil, hunter mendapat reward dan exp (naik level jika exp ≥ 500).

### Battle Hunter

```c
void battle_hunter() {
    lock();
    
    printf("\n👥 DAFTAR HUNTER LAIN 👥\n");
    int available = 0;
    int available_indices[MAX_HUNTERS];
    
    for (int i = 0; i < shared_data->num_hunters; i++) {
        if (strcmp(shared_data->hunters[i].username, current_hunter.username) != 0 && 
            !shared_data->hunters[i].banned) {
            
            printf("\n%d. @%s\n", available+1, shared_data->hunters[i].username);
            printf("   🎚 Level: %d | ⚔️ ATK: %d | ❤️ HP: %d | 🛡️ DEF: %d\n",
                   shared_data->hunters[i].level,
                   shared_data->hunters[i].atk,
                   shared_data->hunters[i].hp,
                   shared_data->hunters[i].def);
            available_indices[available] = i;
            available++;
        }
    }
    
    if (available == 0) {
        printf("Tidak ada hunter lain yang tersedia untuk battle.\n");
        unlock();
        return;
    }
    
    printf("\nPilih hunter yang ingin dibattle (0 untuk batal): ");
    int choice;
    scanf("%d", &choice);
    getchar();
    
    if (choice == 0 || choice > available) {
        unlock();
        return;
    }
    
    int target_idx = available_indices[choice-1];
    struct Hunter *target = &shared_data->hunters[target_idx];
    
    int my_power = current_hunter.atk + current_hunter.hp + current_hunter.def;
    int enemy_power = target->atk + target->hp + target->def;
    
    printf("\n⚔️ BATTLE START! ⚔️\n");
    printf("@%s vs @%s\n", current_hunter.username, target->username);
    printf("Total Power: %d vs %d\n", my_power, enemy_power);
    
    if (my_power > enemy_power) {
        current_hunter.atk += target->atk;
        current_hunter.hp += target->hp;
        current_hunter.def += target->def;
        
        for (int i = target_idx; i < shared_data->num_hunters - 1; i++) {
            shared_data->hunters[i] = shared_data->hunters[i+1];
        }
        shared_data->num_hunters--;
        
        printf("\n🎉 ANDA MENANG! 🎉\n");
        printf("Anda mendapatkan semua stats lawan!\n");
    } else {
        target->atk += current_hunter.atk;
        target->hp += current_hunter.hp;
        target->def += current_hunter.def;
        
        for (int i = 0; i < shared_data->num_hunters; i++) {
            if (strcmp(shared_data->hunters[i].username, current_hunter.username) == 0) {
                for (int j = i; j < shared_data->num_hunters - 1; j++) {
                    shared_data->hunters[j] = shared_data->hunters[j+1];
                }
                shared_data->num_hunters--;
                break;
            }
        }
        
        printf("\n💀 ANDA KALAH! 💀\n");
        printf("Anda dikeluarkan dari sistem!\n");
        
        unlock();
        shmdt(shared_data);
        exit(0);
    }
```

* Hunter dapat memilih lawan.
* Pemenang mendapatkan semua stat lawan, yang kalah dihapus dari sistem.

### Shutdown dan Logout

```c
/hunter
void cleanup(int sig) {
    printf("\nMembersihkan shared memory...\n");
    shmdt(shared_data);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    exit(0);
}

/system
void logout() {
    shmdt(shared_data); 
    logged_in = 0;
}
```

* Membersihkan shared memory dan semaphore saat program dimatikan.
* Logout melepaskan hubungan dengan shared memory.

### Spesifikasi Berdasarkan Cerita Soal

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

### Cara Compile dan Penggunaan

#### Compile Program

Gunakan perintah berikut untuk meng-compile dua file utama:

```c
gcc -o system system.c -pthread
gcc -o hunter hunter.c -pthread
```

#### Jalankan Program

1. Jalankan `system` terlebih dahulu untuk menginisialisasi shared memory:

```c
./system
```

2. Jalankan `hunter` di terminal lain untuk hunter register/login:

```c
./hunter
```

### menu
```c
/hunter
int main() {
    srand(time(NULL));
    init_shared_memory();
    init_semaphore();
    
    while(1) {
        printf("\n=== HUNTER SYSTEM ===\n");
        printf("1. Register\n");
        printf("2. Login\n");
        printf("3. Exit\n");
        printf("Pilihan: ");
        
        int choice;
        scanf("%d", &choice);
        getchar();
        
        switch(choice) {
            case 1: register_hunter(); break;
            case 2: login_hunter(); break;
            case 3: 
                shmdt(shared_data);
                return 0;
            default: printf("Pilihan tidak valid!\n");
        }
        
        if (logged_in) {
            hunter_menu();
        }
    }
    
    return 0;
}

/system
int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    
    srand(time(NULL));
    init_shared_memory();
    init_semaphore();
    
    while(1) {
        printf("\n=== HUNTER ADMIN SYSTEM ===\n");
        printf("1. Generate Dungeon\n");
        printf("2. Info Dungeon\n");
        printf("3. Info Hunter\n");
        printf("4. Ban/Unban Hunter\n");
        printf("5. Reset Hunter\n");
        printf("6. Exit\n");
        printf("Pilihan: ");
        
        int choice;
        scanf("%d", &choice);
        getchar();
        
        switch(choice) {
            case 1: generate_dungeon(); break;
            case 2: show_dungeons(); break;
            case 3: show_hunters(); break;
            case 4: ban_hunter(); break;
            case 5: reset_hunter(); break;
            case 6: cleanup(0); break;
            default: printf("Pilihan tidak valid!\n");
        }
    }
    
    return 0;
}
```

### Revisi
penambahan
```c
#include <stdatomic.h>
```
