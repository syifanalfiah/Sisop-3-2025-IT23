#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define IP "127.0.0.1"
#define PORT 3010
#define BSZ 2048

void recv_print(int s) {
    char buf[BSZ + 1];
    int n = read(s, buf, BSZ);
    if(n > 0) {
        buf[n] = '\0';
        printf("%s\n", buf);
    }
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv = { .sin_family = AF_INET, .sin_port = htons(PORT) };
    inet_pton(AF_INET, IP, &srv.sin_addr);

    if(connect(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("Gagal koneksi ke server");
        return 1;
    }

    int choice;
    char cmd[64];

    while(1) {
        printf("\n\x1b[95m=== TLD Menu ===\x1b[0m\n");
        printf("\x1b[96m[1]\x1b[0m Status Diri\n");
        printf("\x1b[96m[2]\x1b[0m Toko Senjata\n");
        printf("\x1b[96m[3]\x1b[0m Periksa Inventori & atur perlengkapan\n");
        printf("\x1b[96m[4]\x1b[0m Battle Mode\n");
        printf("\x1b[96m[5]\x1b[0m Keluar dari TLD\n");    
        printf("\x1b[96mChoose:\x1b[0m ");
        scanf("%d", &choice);
        getchar();

        if(choice == 1) {
            strcpy(cmd, "SHOW_STATS");
            send(sock, cmd, strlen(cmd), 0);
            recv_print(sock);
        }
        else if(choice == 2) {
            strcpy(cmd, "SHOW_SHOP");
            send(sock, cmd, strlen(cmd), 0);
            recv_print(sock);

            printf("\x1b[96mPilih senjata untuk dibeli (1-5), 0 untuk batal:\x1b[0m ");
            int b;
            scanf("%d", &b);
            getchar();
            if(b >= 1 && b <= 5) {
                snprintf(cmd, sizeof(cmd), "BUY %d", b);
                send(sock, cmd, strlen(cmd), 0);
                recv_print(sock);
            } else {
                printf("Batal atau invalid.\n");
            }
        }
        else if(choice == 3) {
            strcpy(cmd, "INVENTORY");
            send(sock, cmd, strlen(cmd), 0);
            recv_print(sock);

            printf("\x1b[96mPilih indeks senjata untuk equip (1-n), 0 untuk batal:\x1b[0m ");
            int e;
            scanf("%d", &e);
            getchar();
            if(e > 0) {
                snprintf(cmd, sizeof(cmd), "EQUIP %d", e);
                send(sock, cmd, strlen(cmd), 0);
                recv_print(sock);
            } else if(e == 0) {
                printf("Batal equip.\n");
            } else {
                printf("Pilihan tidak valid.\n");
            }
        }
        else if(choice == 5) {
            strcpy(cmd, "EXIT");
            send(sock, cmd, strlen(cmd), 0);
            printf("\x1b[33mGoodbye, adventurer!\x1b[0m\n");
            break;
        }
        else if(choice == 4) {
            strcpy(cmd, "BATTLE");
            send(sock, cmd, strlen(cmd), 0);
        
            // FIX: hanya satu kali baca output awal battle
            recv_print(sock);
        
            while(1){
                char bcmd[64] = {0};
                printf("\x1b[96mBattle> \x1b[0m");
                scanf("%s", bcmd);
                getchar(); // flush newline
        
                send(sock, bcmd, strlen(bcmd), 0);
                if(!strcmp(bcmd, "exit")) {
                    recv_print(sock); // baca "keluar dari battle mode"
                    break;
                }
        
                // Baca semua respons (damage, bar hp, dll)
                fd_set readfds;
                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 200000;
        
                while (1) {
                    FD_ZERO(&readfds);
                    FD_SET(sock, &readfds);
                    int rv = select(sock + 1, &readfds, NULL, NULL, &tv);
                    if (rv <= 0) break;
                    recv_print(sock);
                }
            }
        }        
        else {
            printf("Invalid option.\n");
        }
    }

    close(sock);
    return 0;
}
