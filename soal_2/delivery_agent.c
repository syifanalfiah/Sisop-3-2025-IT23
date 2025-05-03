#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <errno.h>

#define MAX_ORDERS 100

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

SharedData *data;
pthread_mutex_t lock;

void write_log(const char *agent, const char *name, const char *address) {
    FILE *log = fopen("delivery.log", "a");
    if (!log) {
        perror("Gagal membuka delivery.log");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%d/%m/%Y %H:%M:%S", t);

    fprintf(log, "[%s] [%s] Express package delivered to %s in %s\n",
            time_buf, agent, name, address);
    fclose(log);
}

void *agent_thread(void *arg) {
    char *agent_name = (char *) arg;

    while (1) {
        int found = 0;
        pthread_mutex_lock(&lock);
        for (int i = 0; i < data->total_orders; i++) {
            if (strcmp(data->orders[i].type, "Express") == 0 && data->orders[i].sent == 0) {
                data->orders[i].sent = 1; 
                char name[64], address[128];
                strcpy(name, data->orders[i].name);
                strcpy(address, data->orders[i].address);
                pthread_mutex_unlock(&lock);

                printf("%s mengirim ke %s (%s)\n", agent_name, name, address);
                sleep(1); 
                write_log(agent_name, name, address);
                found = 1;
                break;
            }
        }
        if (!found) {
            pthread_mutex_unlock(&lock);
            break; 
        }
    }
    return NULL;
}

int main() {
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

    data = (SharedData *) shmat(shmid, NULL, 0);
    if ((void *)data == (void *) -1) {
        perror("shmat gagal");
        return 1;
    }

    pthread_mutex_init(&lock, NULL);

    pthread_t agents[3];
    char *names[3] = {"AGENT A", "AGENT B", "AGENT C"};

    for (int i = 0; i < 3; i++) {
        pthread_create(&agents[i], NULL, agent_thread, names[i]);
    }

    for (int i = 0; i < 3; i++) {
        pthread_join(agents[i], NULL);
    }

    pthread_mutex_destroy(&lock);
    shmdt(data);

    return 0;
}
