#include "shm_common.h"
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>

#define SEM_KEY 5678

int shmid;
struct SystemData *shared_data;
int semid;

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

void cleanup(int sig) {
    printf("\nMembersihkan shared memory...\n");
    shmdt(shared_data);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    exit(0);
}

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
