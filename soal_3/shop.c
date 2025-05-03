#include <stdio.h>
#include <string.h>

#define MAX_WEAPONS 5
#define MAX_NAME 32

typedef struct {
    char name[MAX_NAME];
    int price;
    int damage;
    char passive[64];
} Weapon;

Weapon shopWeapons[MAX_WEAPONS] = {
    {"Dark Repulsor", 200, 13, ""},
    {"Elucidator", 200, 12, ""},
    {"Gilta Brille", 250, 14, ""},
    {"Mercenary Twinblade", 150, 9, "Quick Slash"},
    {"Togetsu Waxing", 300, 15, "Thunder Blast"}
};

int get_weapon_count() {return MAX_WEAPONS;}

Weapon get_weapon(int index) {return shopWeapons[index];}