#include "shm_common.h"
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <poll.h>
#include <unistd.h>

#define SEM_KEY 5678

int shmid;
struct SystemData *shared_data;
int semid;
struct Hunter current_hunter;
int logged_in = 0;

atomic_int notification_on = 0;
pthread_t notif_thread;

void init_shared_memory();
void init_semaphore();
void lock();
void unlock();
void* notification_thread(void* arg);
void toggle_notification();
void show_available_dungeons();
void raid_dungeon();
void battle_hunter();
void register_hunter();
void login_hunter();
void hunter_menu();

void init_shared_memory() {
    key_t key = get_system_key();
    shmid = shmget(key, sizeof(struct SystemData), 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    shared_data = (struct SystemData *)shmat(shmid, NULL, 0);
    if (shared_data == (void *)-1) {
        perror("shmat failed");
        exit(1);
    }
}

void init_semaphore() {
    semid = semget(SEM_KEY, 1, 0666);
    if (semid == -1) {
        perror("semget failed");
        exit(1);
    }
}

void lock() {
    struct sembuf sb = {0, -1, 0};
    semop(semid, &sb, 1);
}

void unlock() {
    struct sembuf sb = {0, 1, 0};
    semop(semid, &sb, 1);
}

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
    
    for (int i = 0; i < shared_data->num_hunters; i++) {
        if (strcmp(shared_data->hunters[i].username, current_hunter.username) == 0) {
            shared_data->hunters[i] = current_hunter;
            break;
        }
    }
    
    printf("\nStats Anda sekarang:\n");
    printf("🎚 Level: %d | 🌟 EXP: %d/500\n", current_hunter.level, current_hunter.exp);
    printf("⚔️ ATK: %d | ❤️ HP: %d | 🛡️ DEF: %d\n", 
           current_hunter.atk, current_hunter.hp, current_hunter.def);
    
    unlock();
}

void register_hunter() {
    struct Hunter new_hunter;
    
    printf("\n=== REGISTRASI HUNTER ===\n");
    printf("Username: ");
    fgets(new_hunter.username, 50, stdin);
    new_hunter.username[strcspn(new_hunter.username, "\n")] = 0;
    
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
            printf("Username sudah digunakan!\n");
            unlock();
            return;
        }
    }
    
    if (shared_data->num_hunters >= MAX_HUNTERS) {
        printf("Kapasitas hunter penuh!\n");
        unlock();
        return;
    }
    
    shared_data->hunters[shared_data->num_hunters++] = new_hunter;
    
    printf("\nRegistrasi berhasil!\n");
    printf("Selamat datang, @%s\n", new_hunter.username);
    printf("🔑 Key Anda: %d\n", new_hunter.shm_key);
    printf("Stats awal:\n");
    printf("🎚 Level: 1 | 🌟 EXP: 0/500\n");
    printf("⚔️ ATK: 10 | ❤️ HP: 100 | 🛡️ DEF: 5\n");
    
    current_hunter = new_hunter;
    logged_in = 1;
    
    unlock();
}

void login_hunter() {
    char username[50];
    
    printf("\n=== LOGIN HUNTER ===\n");
    printf("Username: ");
    fgets(username, 50, stdin);
    username[strcspn(username, "\n")] = 0;
    
    lock();
    
    for (int i = 0; i < shared_data->num_hunters; i++) {
        if (strcmp(shared_data->hunters[i].username, username) == 0) {
            
            if (shared_data->hunters[i].banned) {
                printf("Akun ini diban! Tidak bisa login.\n");
                unlock();
                return;
            }
            
            current_hunter = shared_data->hunters[i];
            logged_in = 1;
            
            printf("\nLogin berhasil!\n");
            printf("Selamat datang kembali, @%s\n", current_hunter.username);
            printf("🎚 Level: %d | 🌟 EXP: %d/500\n", current_hunter.level, current_hunter.exp);
            printf("⚔️ ATK: %d | ❤️ HP: %d | 🛡️ DEF: %d\n", 
                   current_hunter.atk, current_hunter.hp, current_hunter.def);
            
            unlock();
            return;
        }
    }
    
    printf("Username tidak ditemukan!\n");
    unlock();
}

void hunter_menu() {
    while(logged_in) {
        if (!atomic_load(&notification_on)) {
            printf("\n=== HUNTER SYSTEM (@%s) ===\n", current_hunter.username);
            printf("🎚 Level: %d | 🌟 EXP: %d/500\n", current_hunter.level, current_hunter.exp);
            printf("⚔️ ATK: %d | ❤️ HP: %d | 🛡️ DEF: %d\n", 
                   current_hunter.atk, current_hunter.hp, current_hunter.def);
            printf("1. List Dungeon\n");
            printf("2. Raid Dungeon\n");
            printf("3. Battle Hunter\n");
            printf("4. Toggle Notification\n");
            printf("5. Logout\n");
            printf("Pilihan: ");
            
            int choice;
            scanf("%d", &choice);
            getchar();
            
            switch(choice) {
                case 1: show_available_dungeons(); break;
                case 2: raid_dungeon(); break;
                case 3: battle_hunter(); break;
                case 4: toggle_notification(); break;
                case 5: logged_in = 0; break;
                default: printf("Pilihan tidak valid!\n");
            }
        } else {
            struct pollfd fd = {STDIN_FILENO, POLLIN, 0};
            int ret = poll(&fd, 1, 3000);
            
            if (ret > 0 && (fd.revents & POLLIN)) {
                char buf[2];
                read(STDIN_FILENO, buf, sizeof(buf));
                if (buf[0] == '\n') {
                    toggle_notification();
                }
            }
        }
    }
}

void logout() {
    shmdt(shared_data); 
    logged_in = 0;
}

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
