#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define MAX_ORDERS 100
#define CSV_FILE "delivery_order.csv"
#define CSV_URL "https://drive.usercontent.google.com/u/0/uc?id=1OJfRuLgsBnIBWtdRXbRsD2sG6NhMKOg9&export=download"

typedef struct {
    char name[64];
    char address[128];
    char type[16];
    int sent;
} Order;

typedef struct {
    int total_orders;
    Order orders[MAX_ORDERS];
} SharedData;

void write_log(const char *agent, const char *name, const char *address, const char *type) {
    FILE *log = fopen("delivery.log", "a");
    if (!log) {
        perror("Gagal membuka delivery.log");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%d/%m/%Y %H:%M:%S", t);

    fprintf(log, "[%s] [AGENT %s] %s package delivered to %s in %s\n",
            time_buf, agent, type, name, address);
    fclose(log);
}

int load_csv_to_shared_memory() {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "wget -q \"%s\" -O %s", CSV_URL, CSV_FILE);
    if (system(cmd) != 0) {
        fprintf(stderr, "Gagal mengunduh file CSV\n");
        return -1;
    }

    FILE *file = fopen(CSV_FILE, "r");
    if (!file) {
        perror("Gagal membuka file CSV");
        return -1;
    }

    key_t key = ftok(".", 65);
    if (key == -1) {
        perror("ftok gagal");
        return -1;
    }

    int shmid = shmget(key, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget gagal");
        return -1;
    }

    SharedData *data = (SharedData *) shmat(shmid, NULL, 0);
    if ((void *)data == (void *) -1) {
        perror("shmat gagal");
        return -1;
    }

    char line[256];
    int count = 0;
    fgets(line, sizeof(line), file); // skip header

    while (fgets(line, sizeof(line), file) && count < MAX_ORDERS) {
        sscanf(line, "%[^,],%[^,],%s",
               data->orders[count].name,
               data->orders[count].address,
               data->orders[count].type);
        data->orders[count].sent = 0;
        count++;
    }

    data->total_orders = count;
    fclose(file);
    shmdt(data);

    printf("%d order berhasil dimuat ke shared memory.\n", count);
    return 0;
}

int deliver_order(const char *target_name) {
    const char *username = "Niko pro valo";  // Nama pengirim manual

    key_t key = ftok(".", 65);
    if (key == -1) {
        perror("ftok gagal");
        return 1;
    }

    int shmid = shmget(key, sizeof(SharedData), 0666);
    if (shmid == -1) {
        perror("shmget gagal");
        return 1;
    }

    SharedData *data = (SharedData *) shmat(shmid, NULL, 0);
    if ((void *)data == (void *) -1) {
        perror("shmat gagal");
        return 1;
    }

    int found = 0;
    for (int i = 0; i < data->total_orders; i++) {
        if (strcmp(data->orders[i].name, target_name) == 0 &&
            strcmp(data->orders[i].type, "Reguler") == 0 &&
            data->orders[i].sent == 0) {

            data->orders[i].sent = 1;
            printf("Agent %s mengantar paket Reguler untuk %s (%s)\n",
                   username, data->orders[i].name, data->orders[i].address);
            sleep(1);
            write_log(username, data->orders[i].name, data->orders[i].address, "Reguler");
            found = 1;
            break;
        }
    }

    shmdt(data);

    if (!found) {
        printf("Tidak ditemukan order Reguler aktif untuk \"%s\"\n", target_name);
    }

    return 0;
}

void check_status(const char *target_name) {
    key_t key = ftok(".", 65);
    if (key == -1) {
        perror("ftok gagal");
        return;
    }

    int shmid = shmget(key, sizeof(SharedData), 0666);
    if (shmid == -1) {
        perror("shmget gagal");
        return;
    }

    SharedData *data = (SharedData *) shmat(shmid, NULL, 0);
    if ((void *)data == (void *) -1) {
        perror("shmat gagal");
        return;
    }

    int found = 0;
    for (int i = 0; i < data->total_orders; i++) {
        if (strcmp(data->orders[i].name, target_name) == 0) {
            found = 1;
            if (data->orders[i].sent == 0) {
                printf("Status for %s: Pending\n", target_name);
            } else {
                FILE *log = fopen("delivery.log", "r");
                if (!log) {
                    printf("Status for %s: Delivered\n", target_name);
                    break;
                }

                char line[256];
                char agent[64] = "UNKNOWN";
                while (fgets(line, sizeof(line), log)) {
                    if (strstr(line, target_name)) {
                        char *agent_start = strstr(line, "[AGENT ");
                        if (agent_start) {
                            agent_start += 7;
                            char *agent_end = strchr(agent_start, ']');
                            if (agent_end) {
                                size_t len = agent_end - agent_start;
                                strncpy(agent, agent_start, len);
                                agent[len] = '\0';
                            }
                        }
                    }
                }
                fclose(log);
                printf("Status for %s: Delivered by %s\n", target_name, agent);
            }
            break;
        }
    }

    if (!found) {
        printf("Status for %s: Not Found\n", target_name);
    }

    shmdt(data);
}

void list_orders() {
    key_t key = ftok(".", 65);
    if (key == -1) {
        perror("ftok gagal");
        return;
    }

    int shmid = shmget(key, sizeof(SharedData), 0666);
    if (shmid == -1) {
        perror("shmget gagal");
        return;
    }

    SharedData *data = (SharedData *) shmat(shmid, NULL, 0);
    if ((void *)data == (void *) -1) {
        perror("shmat gagal");
        return;
    }

    printf("Order List:\n");
    for (int i = 0; i < data->total_orders; i++) {
        printf("%2d. %-10s - %s\n", i + 1,
               data->orders[i].name,
               data->orders[i].sent ? "Delivered" : "Pending");
    }

    shmdt(data);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        return load_csv_to_shared_memory();
    }

    if (argc == 3 && strcmp(argv[1], "-deliver") == 0) {
        return deliver_order(argv[2]);
    }

    if (argc == 3 && strcmp(argv[1], "-status") == 0) {
        check_status(argv[2]);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "-list") == 0) {
        list_orders();
        return 0;
    }

    fprintf(stderr, "Penggunaan salah.\nGunakan:\n");
    fprintf(stderr, "  ./dispatcher                      # load data CSV ke shared memory\n");
    fprintf(stderr, "  ./dispatcher -deliver <Nama>     # kirim Reguler (manual) oleh Niko pro valo\n");
    fprintf(stderr, "  ./dispatcher -status <Nama>      # cek status pesanan\n");
    fprintf(stderr, "  ./dispatcher -list               # tampilkan semua order dan status\n");
    return 1;
}
