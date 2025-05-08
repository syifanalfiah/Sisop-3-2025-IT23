#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 3010
#define BUF_SZ 2048
#define MAX_NAME 32
#define MAX_INV 10

#include "shop.c"

// --- Struct Definitions ---
typedef struct {
    int gold;
    int base_damage;
    char equipped[MAX_NAME];
    Weapon inv[MAX_INV];
    int inv_count;
    int kills;  // Menambahkan kill count
} Player;

// --- Utility Functions ---
int rand_range(int min, int max) {
    return rand() % (max - min + 1) + min;
}

void print_healthbar(int hp, int max_hp, char* bar) {
    int len = 20;
    int filled = hp * len / max_hp;

    char temp[64] = "";
    for (int i = 0; i < filled; i++) strcat(temp, "#");
    
    char empty[64] = "";
    for (int i = filled; i < len; i++) strcat(empty, "-");

    sprintf(bar, "[\x1b[32m%s\x1b[0m%s]\n", temp, empty);
}

Weapon make_fists() {
    Weapon f; strcpy(f.name, "Fists");
    f.price = 0; f.damage = 5; f.passive[0] = 0;
    return f;
}

int in_inv(Player* p, const char* n) {
    for (int i = 0; i < p->inv_count; i++)
        if (!strcmp(p->inv[i].name, n)) return 1;
    return 0;
}

void add_inv(Player* p, Weapon w) {
    if (p->inv_count < MAX_INV && !in_inv(p, w.name))
        p->inv[p->inv_count++] = w;
}

void show_stats(Player* p, int sock) {
    char passive_line[128] = "";
    for (int i = 0; i < p->inv_count; i++) {
        if (!strcmp(p->equipped, p->inv[i].name) && strlen(p->inv[i].passive)) {
            snprintf(passive_line, sizeof(passive_line), "\x1b[34mPassive: %s\x1b[0m", p->inv[i].passive);
            break;
        }
    }

    char resp[BUF_SZ];
    snprintf(resp, sizeof(resp),
        "\x1b[95m=== Player Stats ===\x1b[0m\n"
        "\x1b[33mGold:\x1b[0m %d    "
        "\x1b[32mEquipped Weapon:\x1b[0m %s |  "
        "\x1b[31mBase Damage:\x1b[0m %d\n"
        "\x1b[36mKills:\x1b[0m %d\n%s\n",
        p->gold, p->equipped, p->base_damage, p->kills, passive_line);

    send(sock, resp, strlen(resp), 0);
}

void show_shop(int sock) {
    char resp[BUF_SZ];
    strcpy(resp, "\x1b[95m=== Weapon Shop ===\x1b[0m\n");
    for (int i = 0; i < get_weapon_count(); i++) {
        Weapon w = get_weapon(i);
        char line[256];
        snprintf(line, sizeof(line), "%d. %s - \x1b[33m%dg\x1b[0m, \x1b[31m%ddmg\x1b[0m",
            i + 1, w.name, w.price, w.damage);
        if (strlen(w.passive)) {
            strcat(line, " [\x1b[34mPassive: "); strcat(line, w.passive); strcat(line, "\x1b[0m]");
        }
        strcat(line, "\n"); strcat(resp, line);
    }
    send(sock, resp, strlen(resp), 0);
}

void handle_inventory(Player* p, int sock) {
    char resp[BUF_SZ];
    strcpy(resp, "\x1b[95m=== YOUR INVENTORY ===\x1b[0m\n");
    for (int i = 0; i < p->inv_count; i++) {
        Weapon w = p->inv[i];
        char line[256];
        snprintf(line, sizeof(line), "[%d] %s | \x1b[31m%ddmg\x1b[0m", i + 1, w.name, w.damage);
        if (strlen(w.passive)) {
            strcat(line, " | \x1b[34mPassive: "); strcat(line, w.passive); strcat(line, "\x1b[0m");
        }
        if (!strcmp(w.name, p->equipped)) strcat(line, " \x1b[32m(EQUIPPED)\x1b[0m");
        strcat(line, "\n"); strcat(resp, line);
    }
    send(sock, resp, strlen(resp), 0);
}

void handle_battle(Player* P, int sock) {
    int monster_hp = rand_range(50, 200);
    int monster_max = monster_hp;
    int battle_active = 1;
    char msg[BUF_SZ], bar[64];
    int offset = 0;

    offset += snprintf(msg + offset, sizeof(msg) - offset,
        "\x1b[95m=== Battle Mode ===\x1b[0m\n"
        "Ketik \x1b[92mattack\x1b[0m untuk menyerang atau \x1b[93mexit\x1b[0m untuk keluar.\n");
    print_healthbar(monster_hp, monster_max, bar);
    offset += snprintf(msg + offset, sizeof(msg) - offset,
        "Musuh HP: %s (%d/%d)\n", bar, monster_hp, monster_max);

    send(sock, msg, offset, 0);

    while (battle_active) {
        char input[64] = {0};
        int r = read(sock, input, sizeof(input) - 1);
        if (r <= 0) break;
        input[strcspn(input, "\r\n")] = 0;

        if (!strcmp(input, "exit")) {
            send(sock, "Keluar dari Battle Mode.\n", 26, 0);
            break;
        }
        else if (!strcmp(input, "attack")) {
            char output[BUF_SZ]; int offset = 0;
            int dmg = P->base_damage + rand() % 4 + 1;
            int crit = (rand() % 100 < 15);
            if (crit) dmg *= 2;

            int passive_bonus = 0, extra_hit = 0;
            for (int i = 0; i < P->inv_count; i++) {
                if (!strcmp(P->equipped, P->inv[i].name)) {
                    if (!strcmp(P->inv[i].passive, "Thunder Blast") && rand() % 100 < 15) {
                        passive_bonus += 10;
                        offset += snprintf(output + offset, BUF_SZ - offset, "\x1b[34mPassive Thunder Blast aktif! +10 damage\x1b[0m\n");
                    }
                    else if (!strcmp(P->inv[i].passive, "Quick Slash") && rand() % 100 < 20) {
                        extra_hit = 1;
                        offset += snprintf(output + offset, BUF_SZ - offset, "\x1b[34mPassive Quick Slash aktif! Serangan tambahan!\x1b[0m\n");
                    }
                    break;
                }
            }

            int total_dmg = dmg + passive_bonus;
            if (crit)
                offset += snprintf(output + offset, BUF_SZ - offset, "\x1b[35mCritical Hit!\x1b[0m\n");
            monster_hp -= total_dmg;
            offset += snprintf(output + offset, BUF_SZ - offset, "Kamu menyerang dan memberi %d damage.\n", total_dmg);

            if (extra_hit) {
                int extra_dmg = P->base_damage + rand() % 4 + 1;
                if (rand() % 100 < 15) {
                    extra_dmg *= 2;
                    offset += snprintf(output + offset, BUF_SZ - offset, "\x1b[35mCritical Hit (Quick Slash)!\x1b[0m\n");
                }
                monster_hp -= extra_dmg;
                offset += snprintf(output + offset, BUF_SZ - offset, "Quick Slash memberikan %d damage tambahan.\n", extra_dmg);
            }

            if (monster_hp < 0) monster_hp = 0;
            print_healthbar(monster_hp, monster_max, bar);
            offset += snprintf(output + offset, BUF_SZ - offset, "Musuh HP: %s (%d/%d)\n", bar, monster_hp, monster_max);

            if (monster_hp <= 0) {
                int reward = rand_range(50, 100);
                P->gold += reward;
                P->kills++;
            
                offset += snprintf(output + offset, BUF_SZ - offset,
                    "\x1b[92mMusuh dikalahkan!\x1b[0m Kamu mendapat \x1b[33m%d gold\x1b[0m.\n\n", reward);
            
                monster_hp = rand_range(50, 200);
                monster_max = monster_hp;
                print_healthbar(monster_hp, monster_max, bar);
                offset += snprintf(output + offset, BUF_SZ - offset,
                    "\x1b[95mMusuh baru muncul!\x1b[0m\nMusuh HP: %s (%d/%d)\n", bar, monster_hp, monster_max);
            }

            send(sock, output, offset, 0);
        }
        else send(sock, "Perintah tidak dikenal dalam Battle Mode.\n", 42, 0);
    }
}

void handle_client(int sock) {
    char buf[BUF_SZ];
    Player P = {500, 5, "", {}, 0, 0};
    Weapon fists = make_fists();
    strcpy(P.equipped, fists.name);
    add_inv(&P, fists);
    srand(time(NULL));

    while (1) {
        memset(buf, 0, sizeof(buf));
        int r = read(sock, buf, BUF_SZ - 1);
        if (r <= 0) break;
        buf[strcspn(buf, "\r\n")] = 0;

        if (!strcmp(buf, "EXIT")) break;
        else if (!strcmp(buf, "SHOW_STATS")) show_stats(&P, sock);
        else if (!strcmp(buf, "SHOW_SHOP")) show_shop(sock);
        else if (!strncmp(buf, "BUY ", 4)) {
            int idx = atoi(buf + 4) - 1;
            char resp[BUF_SZ];
            if (idx < 0 || idx >= get_weapon_count())
                snprintf(resp, sizeof(resp), "Invalid weapon selection.\n");
            else {
                Weapon w = get_weapon(idx);
                if (P.gold >= w.price) {
                    P.gold -= w.price;
                    P.base_damage = w.damage;
                    strcpy(P.equipped, w.name);
                    add_inv(&P, w);
                    snprintf(resp, sizeof(resp), "Berhasil membeli %s!\nGold sisa: %d\n", w.name, P.gold);
                } else snprintf(resp, sizeof(resp), "Gold tidak cukup untuk membeli %s.\n", w.name);
            }
            send(sock, resp, strlen(resp), 0);
        }
        else if (!strcmp(buf, "INVENTORY")) handle_inventory(&P, sock);
        else if (!strncmp(buf, "EQUIP ", 6)) {
            int idx = atoi(buf + 6) - 1;
            char resp[BUF_SZ];
            if (idx >= 0 && idx < P.inv_count) {
                Weapon w = P.inv[idx];
                strcpy(P.equipped, w.name);
                P.base_damage = w.damage;
                snprintf(resp, sizeof(resp), "Senjata %s telah di-equip.\n", w.name);
            } else snprintf(resp, sizeof(resp), "Pilihan senjata tidak valid.\n");
            send(sock, resp, strlen(resp), 0);
        }
        else if (!strcmp(buf, "BATTLE")) handle_battle(&P, sock);
        else send(sock, "Unknown command.\n", 18, 0);
    }
    close(sock);
}

int main() {
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {AF_INET, htons(PORT), INADDR_ANY};
    bind(sd, (struct sockaddr*)&a, sizeof(a));
    listen(sd, 3);
    printf("Dungeon server berjalan di port %d...\n", PORT);
    while (1) {
        int cs = accept(sd, NULL, NULL);
        if (cs >= 0) handle_client(cs);
    }
    return 0;
}
