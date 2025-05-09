# Laporan Praktikum Modul 3

## Nama Anggota

| Nama                        | NRP        |
| --------------------------- | ---------- |
| Syifa Nurul Alfiah          | 5027241019 |
| Alnico Virendra Kitaro Diaz | 5027241081 |
| Hafiz Ramadhan              | 5027241096 |

## Soal No 1

### Deskripsi

Proyek ini merupakan sistem client-server yang memungkinkan pengguna untuk mengubah file teks rahasia menjadi file JPEG melalui proses dekripsi khusus. Sistem ini terdiri dari dua komponen utama:

- `image_client.c`: Program client yang menyediakan antarmuka pengguna untuk mengunggah file teks, mendownload file JPEG hasil dekripsi, dan melihat daftar file yang tersedia.
- `image_server.c`: Program server yang berjalan sebagai daemon, menerima permintaan dari client, melakukan proses dekripsi, dan menyimpan file hasil dekripsi.

Sistem ini menggunakan socket RPC untuk komunikasi antara client dan server, dengan protokol khusus untuk perintah seperti DECRYPT, DOWNLOAD, dan LIST.

### Struktur File

```
.
├── client
│   ├── [timestamp].jpeg (file hasil download)
│   ├── image_client (binary)
│   └── secrets
│       ├── input_1.txt
│       ├── input_2.txt
│       ├── input_3.txt
│       ├── input_4.txt
│       └── input_5.txt
├── image_client.c (source code client)
├── image_server.c (source code server)
└── server
    ├── database
    │   ├── [timestamp].jpeg (file hasil dekripsi)
    │   └── ...
    ├── image_server (binary)
    └── server.log (log aktivitas server)
```

### Fungsi Utama Client

### Inisialisasi Direktori dan Koneksi

```c
#define PORT 8080
#define BUFFER_SIZE 4096
#define SECRETS_DIR "secrets"

void create_directories() {
    struct stat st = {0};
    if (stat(SECRETS_DIR, &st) == -1) {
        printf("Creating secrets directory...\n");
        if (mkdir(SECRETS_DIR, 0755) != 0) {
            perror("Failed to create secrets directory");
            exit(1);
        }
    }
}

int connect_to_server() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Failed to create socket\n");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Failed to connect to server\n");
        return -1;
    }

    return sock;
}
```

Fungsi-fungsi ini bertanggung jawab untuk:
- Membuat direktori 'secrets' jika belum ada
- Membuat koneksi socket ke server pada port 8080
- Menangani error jika gagal membuat koneksi

### Konversi File ke Hex

```c
void file_to_hex(const char *path, char **hex_output, size_t *hex_len) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror("Error opening file");
        *hex_output = NULL;
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char *file_content = malloc(file_size);
    if (!file_content) {
        perror("Memory allocation failed");
        fclose(file);
        *hex_output = NULL;
        return;
    }

    size_t bytes_read = fread(file_content, 1, file_size, file);
    fclose(file);

    *hex_len = bytes_read * 2 + 1;
    *hex_output = malloc(*hex_len);
    if (!*hex_output) {
        perror("Memory allocation failed");
        free(file_content);
        return;
    }

    char *hex_ptr = *hex_output;
    for (size_t i = 0; i < bytes_read; i++) {
        sprintf(hex_ptr, "%02x", file_content[i]);
        hex_ptr += 2;
    }
    *hex_ptr = '\0';

    free(file_content);
}
```

Fungsi ini:
- Membaca file biner dari path yang diberikan
- Mengkonversi setiap byte menjadi representasi hexadecimal (2 karakter per byte)
- Mengalokasikan memori untuk menyimpan hasil konversi
- Mengembalikan string hex melalui parameter output

### Upload File untuk Dekripsi

```c
void upload_file(int sock, const char *filename) {
    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", SECRETS_DIR, filename);

    printf("Trying to open: %s\n", fullpath);
    char *hex_data;
    size_t hex_len;
    file_to_hex(fullpath, &hex_data, &hex_len);

    if (!hex_data) {
        printf("Failed to convert file to hex\n");
        return;
    }

    printf("Hex data preview: %.40s...\n", hex_data);

    char *command = malloc(strlen(hex_data) + 9);
    if (!command) {
        perror("Memory allocation failed");
        free(hex_data);
        return;
    }

    sprintf(command, "DECRYPT %s", hex_data);
    size_t command_len = strlen(command);

    size_t total_sent = 0;
    while (total_sent < command_len) {
        ssize_t sent = send(sock, command + total_sent, command_len - total_sent, 0);
        if (sent < 0) {
            perror("Error sending data");
            free(hex_data);
            free(command);
            return;
        }
        total_sent += sent;
        printf("Sent %zu of %zu bytes\r", total_sent, command_len);
        fflush(stdout);
    }

    printf("\nAll data sent successfully.\n");
    free(hex_data);
    free(command);

    char response[BUFFER_SIZE];
    int bytes_received = recv(sock, response, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        printf("Error: No response from server\n");
        return;
    }

    response[bytes_received] = '\0';
    if (strncmp(response, "OK", 2) == 0) {
        printf("Success! File saved as: %s\n", response + 3);
    } else {
        printf("Error: %s\n", response);
    }
}
```

Fungsi ini:
- Membaca file dari direktori 'secrets'
- Mengkonversi isi file ke format hex
- Mengirim perintah DECRYPT ke server beserta data hex
- Menangani pengiriman data secara bertahap untuk file besar
- Menerima dan menampilkan respon dari server

### Download File dari Server

```c
void download_file(int sock, const char *filename) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "DOWNLOAD %s", filename);
    send(sock, command, strlen(command), 0);

    char response[BUFFER_SIZE];
    int bytes_received = recv(sock, response, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        printf("Error: No response from server\n");
        return;
    }

    response[bytes_received] = '\0';

    printf("Server response: %s\n", response);
    if (strncmp(response, "OK", 2) == 0) {
        long file_size = atol(response + 3);
        printf("Downloading %s (%ld bytes)...\n", filename, file_size);

        char output_path[PATH_MAX];
        snprintf(output_path, sizeof(output_path), "%s", filename);
        FILE *file = fopen(output_path, "wb");
        if (!file) {
            perror("Error creating file");
            return;
        }

        long remaining = file_size;
        long total_received = 0;
        char buffer[BUFFER_SIZE];
        while (remaining > 0) {
            bytes_received = recv(sock, buffer, 
                (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE, 0);
            if (bytes_received <= 0) break;
            fwrite(buffer, 1, bytes_received, file);
            remaining -= bytes_received;
            total_received += bytes_received;
            if (file_size > 0) {
                int percent = (int)(total_received * 100 / file_size);
                printf("\rDownloading: %d%% complete", percent);
                fflush(stdout);
            }
        }
        fclose(file);
        printf("\nDownload complete! Saved as %s\n", filename);
    } else {
        printf("Error: %s\n", response);
    }
}
```

Fungsi ini:
- Mengirim perintah DOWNLOAD ke server dengan nama file yang diminta
- Menerima respon ukuran file dari server
- Menerima data file secara streaming dan menyimpannya ke disk
- Menampilkan progress download dalam persentase

### Fungsi Tambahan Client

```c
void list_server_files(int sock) {
    char command[BUFFER_SIZE];
    strcpy(command, "LIST");
    send(sock, command, strlen(command), 0);

    char response[BUFFER_SIZE];
    int bytes_received = recv(sock, response, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        printf("Error: No response from server\n");
        return;
    }

    response[bytes_received] = '\0';
    if (strncmp(response, "OK", 2) == 0) {
        printf("\nAvailable JPEG files on server:\n%s\n", response + 3);
    } else {
        printf("Error retrieving server files: %s\n", response);
    }
}

void list_secrets_files() {
    printf("\nAvailable files in secrets directory:\n");
    DIR *dir;
    struct dirent *entry;
    dir = opendir(SECRETS_DIR);
    if (!dir) {
        printf("Cannot open secrets directory\n");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            printf("- %s\n", entry->d_name);
        }
    }
    closedir(dir);
}

void create_sample_file() {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/sample.txt", SECRETS_DIR);
    FILE *file = fopen(path, "w");
    if (!file) {
        perror("Failed to create sample file");
        return;
    }

    fprintf(file, "ffd8ffe000104a46494600010100000100010000ffdb004300"
            "0a07070809070a090a0b0c100d0c0b0b0c1513181615131414"
            "1a211e1d1a1d1d2024292c2e2c24272b2a2d2e2d1721323638"
            "332c3632292c2d2bffc0000b08000a000a01010100ffda0008"
            "01010000000000");
    fclose(file);
    printf("Created sample file: %s\n", path);
}
```

Fungsi-fungsi ini menyediakan:
- Daftar file JPEG yang tersedia di server
- Daftar file teks yang tersedia di direktori 'secrets'
- Pembuatan file sampel untuk percobaan

### Fungsi Utama Server

### Inisialisasi Server

```c
#define PORT 8080
#define BUFFER_SIZE 4096
#define LOG_FILE "server.log"
#define DB_DIR "database"

int server_socket = -1;
int run_as_daemon = 0;

void create_directories() {
    struct stat st = {0};
    if (stat(DB_DIR, &st) == -1) {
        if (mkdir(DB_DIR, 0755) != 0) {
            perror("Failed to create database directory");
            exit(1);
        }
    }
}

void cleanup(int sig) {
    if (server_socket >= 0) {
        close(server_socket);
    }
    log_action("Server", "SHUTDOWN", "Server is shutting down");
    exit(0);
}
```

Fungsi-fungsi ini:
- Membuat direktori 'database' untuk menyimpan file hasil dekripsi
- Menangani shutdown server dengan membersihkan sumber daya
- Mengatur server untuk berjalan sebagai daemon

### Logging Aktivitas

```c
void log_action(const char *source, const char *action, const char *info) {
    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        fprintf(log, "[%s][%s]: [%s] [%s]\n", source, timestamp, action, info);
        fclose(log);
    }

    if (!run_as_daemon) {
        printf("[%s][%s]: [%s] [%s]\n", source, timestamp, action, info);
    }
}
```

Fungsi ini mencatat semua aktivitas server ke file log dengan format:
`[Source][Timestamp]: [Action] [Info]`

### Proses Dekripsi

```c
int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void clean_hex_string(char *str) {
    char *src = str;
    char *dst = str;
    
    while (*src) {
        if (isxdigit((unsigned char)*src)) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

void dump_hex(const unsigned char *data, int len) {
    char debug_info[256] = {0};
    char tmp[8];
    int i;
    
    if (len > 32) len = 32; 
    
    strcpy(debug_info, "Bytes: ");
    for (i = 0; i < len; i++) {
        sprintf(tmp, "%02X ", data[i]);
        strcat(debug_info, tmp);
    }
    
    log_action("Server", "HEX_DUMP", debug_info);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) break;
        
        buffer[bytes_received] = '\0';
        
        if (strncmp(buffer, "DECRYPT", 7) == 0) {
            log_action("Client", "DECRYPT", "Text data");
            char *text_data = buffer + 8;

            char debug_msg[100];
            snprintf(debug_msg, sizeof(debug_msg), "Received %ld bytes of data", strlen(text_data));
            log_action("Server", "RECEIVED", debug_msg);

            int len = strlen(text_data);
            char *reversed_data = malloc(len + 1);
            if (!reversed_data) {
                log_action("Server", "ERROR", "Failed to allocate memory for reversed data");
                strcpy(response, "ERROR Memory allocation failed");
                send(client_socket, response, strlen(response), 0);
                continue;
            }
            
            for (int i = 0; i < len; i++) {
                reversed_data[i] = text_data[len - i - 1];
            }
            reversed_data[len] = '\0';

            char preview[50] = {0};
            strncpy(preview, reversed_data, sizeof(preview)-1);
            log_action("Server", "REVERSED_DATA", preview);

            clean_hex_string(reversed_data);

            char cleaned_preview[50] = {0};
            strncpy(cleaned_preview, reversed_data, sizeof(cleaned_preview)-1);
            log_action("Server", "CLEANED", cleaned_preview);

            len = strlen(reversed_data);
            int max_decoded_len = len / 2;
            unsigned char *decoded = (unsigned char *)malloc(max_decoded_len);
            if (!decoded) {
                strcpy(response, "ERROR Memory allocation failed");
                send(client_socket, response, strlen(response), 0);
                log_action("Server", "ERROR", "Memory allocation failed");
                free(reversed_data);
                continue;
            }
            
            int decoded_len = 0;
            for (int i = 0; i < len; i += 2) {
                if (i + 1 >= len) break;
                
                int high = hex_to_int(reversed_data[i]);
                int low = hex_to_int(reversed_data[i+1]);
                
                if (high == -1 || low == -1) continue; 
                
                decoded[decoded_len++] = (high << 4) | low;
            }
            
            free(reversed_data);
        
            char len_str[20];
            sprintf(len_str, "%d", decoded_len);
            log_action("Server", "DECODED_LEN", len_str);
            dump_hex(decoded, decoded_len);

            if (decoded_len >= 2 && decoded[0] == 0xFF && decoded[1] == 0xD8) {
                log_action("Server", "INFO", "Valid JPEG signature detected");
            } else {
                log_action("Server", "WARNING", "No valid JPEG signature found in decoded data");
            }

            time_t now = time(NULL);
            char filename[50];
            snprintf(filename, sizeof(filename), "%s/%ld.jpeg", DB_DIR, now);
            
            FILE *file = fopen(filename, "wb");
            if (file) {
                fwrite(decoded, 1, decoded_len, file);
                fclose(file);
                snprintf(response, sizeof(response), "OK %ld.jpeg", now);
                log_action("Server", "SAVE", filename);
            } else {
                strcpy(response, "ERROR Failed to save file");
                log_action("Server", "ERROR", "Failed to save file");
            }
            free(decoded);
            send(client_socket, response, strlen(response), 0);
        }
        else if (strncmp(buffer, "DOWNLOAD", 8) == 0) {
            char *filename = buffer + 9;
            log_action("Client", "DOWNLOAD", filename);
            
            char filepath[256];
            snprintf(filepath, sizeof(filepath), "%s/%s", DB_DIR, filename);
            
            FILE *file = fopen(filepath, "rb");
            if (file) {
                fseek(file, 0, SEEK_END);
                long file_size = ftell(file);
                fseek(file, 0, SEEK_SET);
                
                snprintf(response, sizeof(response), "OK %ld", file_size);
                send(client_socket, response, strlen(response), 0);

                usleep(100000);
                
                char file_buffer[BUFFER_SIZE];
                size_t bytes_read;
                while ((bytes_read = fread(file_buffer, 1, BUFFER_SIZE, file)) > 0) {
                    ssize_t sent = send(client_socket, file_buffer, bytes_read, 0);
                    if (sent < 0) {
                        log_action("Server", "ERROR", "Failed to send file data");
                        break;
                    }
                    usleep(10000);
                }
                fclose(file);
                log_action("Server", "UPLOAD", filename);
            } else {
                strcpy(response, "ERROR File not found");
                send(client_socket, response, strlen(response), 0);
                log_action("Server", "ERROR", "File not found");
            }
        }
        else if (strncmp(buffer, "LIST", 4) == 0) {
            log_action("Client", "LIST", "Requesting file list");
            char *file_list = list_database_files();
            snprintf(response, sizeof(response), "OK %s", file_list);
            send(client_socket, response, strlen(response), 0);
            log_action("Server", "LIST", "Sent file list");
        }
        else if (strncmp(buffer, "EXIT", 4) == 0) {
            log_action("Client", "EXIT", "Client requested to exit");
            break;
        }
    }
    close(client_socket);
}
```

Proses dekripsi terdiri dari:
1. Menerima data hex dari client
2. Membalik urutan string hex
3. Membersihkan karakter non-hex
4. Mengkonversi hex ke binary
5. Menyimpan hasil konversi sebagai file JPEG dengan nama timestamp

### Fungsi Tambahan Server

```c
char* list_database_files() {
    static char file_list[BUFFER_SIZE] = {0};
    memset(file_list, 0, sizeof(file_list));
    
    DIR *dir;
    struct dirent *entry;
    dir = opendir(DB_DIR);
    if (!dir) {
        return "No files found";
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".jpeg") == 0) {
                strcat(file_list, entry->d_name);
                strcat(file_list, "\n");
            }
        }
    }
    closedir(dir);
    return file_list;
}
```

Fungsi ini mengembalikan daftar file JPEG yang tersedia di direktori 'database'

### Cara Kompilasi dan Penggunaan

### Kompilasi Program

```bash
gcc -o image_client image_client.c
gcc -o image_server image_server.c
```

### Menjalankan Server

```bash
./image_server
```

### Menjalankan Client

```bash
./image_client
```

### Menu Client

Client menyediakan menu interaktif dengan pilihan:
1. Upload dan decrypt file
2. Download JPEG file
3. List available files
4. Create sample file
5. Exit

### Spesifikasi Berdasarkan Cerita Soal

| Fitur | Deskripsi |
|-------|-----------|
| **Dekripsi File** | Mengubah file teks hex menjadi file JPEG melalui proses reverse dan decode hex |
| **Download File** | Mengambil file JPEG hasil dekripsi dari server |
| **Daftar File** | Menampilkan file teks yang tersedia untuk dekripsi dan file JPEG hasil dekripsi |
| **Logging** | Mencatat semua aktivitas client dan server ke file log |
| **Error Handling** | Menangani berbagai error seperti koneksi gagal, file tidak ditemukan, dll |
| **Mode Daemon** | Server dapat berjalan di background sebagai daemon |

### Alur Kerja Sistem

1. Client membaca file teks dari direktori 'secrets'
2. Client mengkonversi isi file ke format hex
3. Client mengirim perintah DECRYPT beserta data hex ke server
4. Server memproses data dengan:
   - Membalik urutan string hex
   - Membersihkan karakter non-hex
   - Mengkonversi hex ke binary
5. Server menyimpan hasil konversi sebagai file JPEG dengan nama timestamp
6. Client dapat mendownload file JPEG hasil dekripsi dari server

### Revisi
Tidak terdecrypt menjadi jpeg (hasilnya tidak terformat dengan benar dan memutuskan tidak bisa di revisi)

### Kendala
Hasil tidak dapat diselesaikan sebagai jpeg asumsi saya tidak terdecrypt dengan benar, sudah berusaha mengubah buffer size, mengubah function handle client tetap tidak berhasil.

![image](https://github.com/user-attachments/assets/85d3fade-25a2-496e-a71e-e63c7d02409a)


### Dokumentasi

![image](https://github.com/user-attachments/assets/46a482bc-b54e-405f-8a28-9bdedebd0ba9)

![image](https://github.com/user-attachments/assets/661320aa-23a6-4edb-9969-48919e344a52)

![image](https://github.com/user-attachments/assets/5411069a-fdff-49c2-b3d8-7f0adfa17400)

![image](https://github.com/user-attachments/assets/d9906f36-f0a3-42b6-b9d3-5ccbda58c520)

![image](https://github.com/user-attachments/assets/730b180d-4b1b-4b13-8f05-a367ae2cd105)

![image](https://github.com/user-attachments/assets/0c68ecf6-94f4-4e67-9cfa-ade7ffcd2cd0)

![image](https://github.com/user-attachments/assets/cf478b45-a321-4ab4-ad53-c7320174c1ae)

![image](https://github.com/user-attachments/assets/5809b552-ce83-4455-81c2-80515ff68997)

![image](https://github.com/user-attachments/assets/b39d24b3-807f-4ef5-a3b6-cce19931b7b6)

![image](https://github.com/user-attachments/assets/402809b7-2c88-494a-9f46-d3512af22f64)

![image](https://github.com/user-attachments/assets/d9806c9f-4ba5-491d-92ef-05e7fa5d6cee)

![image](https://github.com/user-attachments/assets/c83aa9c8-992d-45a9-864a-50c1b7f1b1ee)

![image](https://github.com/user-attachments/assets/55b5aa7b-9497-40fd-a282-ca23aae34661)


## Soal No 2

dispatcher.c

```c
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

```
gcc dispatcher.c -o dispatcher
./dispatcher             # Load data dari CSV ke shared memory
./dispatcher -deliver Farida
./dispatcher -status Farida
./dispatcher -list

- mendownload file
- melakukan pengiriman secara manual lalu mencatat di delivery.log
- melihat status semua paket

delivery_agent.c

```c
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

```
  gcc delivery_agent.c -o delivery_agent -lpthread
./delivery_agent

- melakukan pengiriman secara expres

![image](https://github.com/user-attachments/assets/a1d40dab-e1dc-48e7-aa5d-f730a4263cc8)

melihat delivery.log 
![image](https://github.com/user-attachments/assets/0ba7c125-9bde-49e1-8418-ea39828c53ce)

## Soal No 3

Pada soal ini, diminta untuk mengimplementasikan sebuah permainan. Permainan ini menggunakan arsitektur **client-server** di mana `dungeon.c` berfungsi sebagai server yang menangani logika permainan, sedangkan `player.c` bertindak sebagai client yang digunakan pemain untuk berinteraksi dengan dunia dungeon.

Permainan ini memiliki beberapa fitur utama, yaitu:

* **Menjelajahi Dungeon:** Pemain dapat mengeksplorasi dunia dungeon dan berinteraksi dengan berbagai elemen, seperti toko senjata dan musuh.
* **Toko Senjata:** Pemain dapat membeli senjata dengan berbagai kemampuan passive yang unik.
* **Inventori:** Senjata yang dibeli dapat disimpan dan digunakan melalui inventori.
* **Battle Mode:** Pemain dapat memasuki mode pertempuran untuk melawan musuh dengan sistem health bar dan damage.

Sebagian besar **permasalahan/error** yang saya alami ketika mengerjakan soal ini ada pada `bagian battle mode`, seperti health bar yang terkadang formatnya error, dan tidak muncul tiba tiba. Juga input yang terkadang tidak terbaca, dan loop untuk input yang berhenti tiba-tiba ditengah pertarungan battlemode. 

---

### **dungeon.c**


### **1. Inisialisasi Server**

```c
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
```

- `socket(AF_INET, SOCK_STREAM, 0)`  
  - Membuat socket TCP. `AF_INET` menunjukkan penggunaan IPv4, `SOCK_STREAM` untuk komunikasi berbasis koneksi, dan `0` untuk protokol default (TCP).

- `bind()`  
  - Mengaitkan socket dengan alamat dan port yang ditentukan (`PORT 3010`). Port ini akan menjadi endpoint server.

- `listen(sd, 3)`  
  - Mengatur socket agar siap menerima koneksi. `3` menunjukkan jumlah maksimum antrian koneksi yang dapat diterima secara bersamaan.

- `accept()`  
  - Menerima koneksi dari client. Jika berhasil (`cs >= 0`), maka koneksi tersebut akan diproses melalui `handle_client()`.


### **2. Struktur Data Player**

```c
typedef struct {
    int gold;
    int base_damage;
    char equipped[MAX_NAME];
    Weapon inv[MAX_INV];
    int inv_count;
    int kills;
} Player;
```

- **gold**: Menyimpan jumlah uang yang dimiliki oleh pemain. Defaultnya adalah 500 gold saat awal permainan.
- **base_damage**: Damage dasar pemain tanpa senjata. Defaultnya adalah 5 damage.
- **equipped**: Nama senjata yang sedang dipakai. Awalnya adalah "Fists".
- **inv[]**: Array dari tipe `Weapon` yang menyimpan senjata yang dimiliki oleh pemain. Maksimal 10 senjata.
- **inv_count**: Jumlah senjata dalam inventori. Dimulai dari `1` (hanya "Fists").
- **kills**: Jumlah musuh yang telah dikalahkan pemain. Dimulai dari `0`.


### **3. Utility Functions**

#### 3.1 **rand_range()**
```c
int rand_range(int min, int max) {
    return rand() % (max - min + 1) + min;
}
```

- Fungsi ini menghasilkan bilangan acak antara `min` dan `max`.
- Menggunakan modulus (`%`) untuk memastikan nilai tetap dalam rentang tersebut.
- Digunakan untuk menentukan **HP musuh** dan **reward gold**.


#### 3.2 **print_healthbar()**
```c
void print_healthbar(int hp, int max_hp, char* bar) {
    int len = 20;
    int filled = hp * len / max_hp;

    char temp[64] = "";
    for (int i = 0; i < filled; i++) strcat(temp, "#");

    char empty[64] = "";
    for (int i = filled; i < len; i++) strcat(empty, "-");

    sprintf(bar, "[%s%s]\n", temp, empty);
}
```

- Fungsi untuk mencetak health bar dengan panjang tetap (`len = 20`).
- **filled**: Jumlah karakter `#` yang merepresentasikan HP saat ini.
- **empty**: Karakter `-` yang menunjukkan sisa HP yang telah hilang.

Contoh:
- Jika `hp = 10` dan `max_hp = 20`, maka health bar akan terlihat seperti:  
  ```
  [##########----------]
  ```


### **4. Battle Mode - handle_battle()**

```c
void handle_battle(Player* P, int sock) {
    int monster_hp = rand_range(50, 200);
    int monster_max = monster_hp;
    char msg[BUF_SZ], bar[64];

    print_healthbar(monster_hp, monster_max, bar);
    snprintf(msg, sizeof(msg), "Musuh HP: %s (%d/%d)\n", bar, monster_hp, monster_max);
    send(sock, msg, strlen(msg), 0);

    while (1) {
        char input[64] = {0};
        int r = read(sock, input, sizeof(input) - 1);
        input[strcspn(input, "\r\n")] = 0;

        if (!strcmp(input, "exit")) {
            send(sock, "Keluar dari Battle Mode.\n", 26, 0);
            break;
        } else if (!strcmp(input, "attack")) {
            int dmg = P->base_damage + rand_range(1, 4);
            int crit = (rand() % 100 < 15) ? 2 : 1;
            int total_dmg = dmg * crit;

            monster_hp -= total_dmg;

            if (monster_hp <= 0) {
                int reward = rand_range(50, 100);
                P->gold += reward;
                P->kills++;
                snprintf(msg, sizeof(msg), "Musuh dikalahkan! Anda mendapat %d gold.\n", reward);
                send(sock, msg, strlen(msg), 0);
                break;
            }
        }
    }
}
```

- **Inisialisasi Musuh:**
  - HP musuh di-generate secara acak (`50-200`).
  - Health bar di-generate menggunakan `print_healthbar()`.

- **Input Pemain:**
  - Jika `exit`, maka keluar dari Battle Mode.
  - Jika `attack`, pemain menyerang musuh.

- **Damage Calculation:**
  - **Base Damage** ditambah acak (`1-4`).
  - **Critical Hit:** 15% kemungkinan double damage (`crit = 2`).

- **Reward & Kills:**
  - Jika musuh kalah, pemain akan mendapat reward gold (`50-100`).
  - Jumlah kill bertambah (`P->kills++`).


### **5. Weapon Shop - show_shop()**

```c
void show_shop(int sock) {
    char resp[BUF_SZ];
    strcpy(resp, "=== Weapon Shop ===\n");
    
    for (int i = 0; i < get_weapon_count(); i++) {
        Weapon w = get_weapon(i);
        char line[256];
        snprintf(line, sizeof(line), "%d. %s - %d gold, %d dmg\n", i + 1, w.name, w.price, w.damage);
        strcat(resp, line);
    }

    send(sock, resp, strlen(resp), 0);
}
```

- Menampilkan senjata yang tersedia di toko.
- Data senjata diambil dari `shop.c` melalui fungsi `get_weapon()`.


### **6. Inventory Management - handle_inventory()**

```c
void handle_inventory(Player* p, int sock) {
    char resp[BUF_SZ];
    strcpy(resp, "=== YOUR INVENTORY ===\n");

    for (int i = 0; i < p->inv_count; i++) {
        Weapon w = p->inv[i];
        char line[256];
        snprintf(line, sizeof(line), "[%d] %s | %ddmg\n", i + 1, w.name, w.damage);
        strcat(resp, line);
    }

    send(sock, resp, strlen(resp), 0);
}
```

- Menampilkan semua senjata yang dimiliki pemain.
- Jika senjata memiliki passive, maka informasi tersebut juga akan ditampilkan.


### **7. Equip Weapon - handle_equip()**

```c
else if (!strncmp(buf, "EQUIP ", 6)) {
    int idx = atoi(buf + 6) - 1;
    char resp[BUF_SZ];

    if (idx >= 0 && idx < P->inv_count) {
        Weapon w = P->inv[idx];
        strcpy(P->equipped, w.name);
        P->base_damage = w.damage;
        snprintf(resp, sizeof(resp), "Senjata %s telah di-equip.\n", w.name);
    } else {
        snprintf(resp, sizeof(resp), "Pilihan senjata tidak valid.\n");
    }

    send(sock, resp, strlen(resp), 0);
}
```

- Memproses perintah `EQUIP`.
- Jika senjata ada di inventori (`idx < inv_count`), maka senjata tersebut akan dipakai (`equipped`).


### **8. Player Status - show_stats()**

```c
void show_stats(Player* p, int sock) {
    char resp[BUF_SZ];
    snprintf(resp, sizeof(resp),
        "Gold: %d\nEquipped Weapon: %s\nBase Damage: %d\nKills: %d\n",
        p->gold, p->equipped, p->base_damage, p->kills);

    send(sock, resp, strlen(resp), 0);
}
```

- Menampilkan status pemain, termasuk gold, senjata yang dipakai, base damage, dan jumlah kill.

---

### **player.c**


### **1. Inisialisasi Client**

```c
int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv = { .sin_family = AF_INET, .sin_port = htons(PORT) };
    inet_pton(AF_INET, IP, &srv.sin_addr);

    if (connect(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("Gagal koneksi ke server");
        return 1;
    }

    int choice;
    char cmd[64];
```

- `socket(AF_INET, SOCK_STREAM, 0)`  
  - Membuat socket untuk komunikasi TCP.

- `inet_pton(AF_INET, IP, &srv.sin_addr)`  
  - Mengubah alamat IP dari string (`127.0.0.1`) menjadi format biner.

- `connect(sock, (struct sockaddr*)&srv, sizeof(srv))`  
  - Menghubungkan client ke server. Jika koneksi gagal, maka program akan keluar.


### **2. Fungsi recv_print()**

```c
void recv_print(int s) {
    char buf[BSZ + 1];
    int n = read(s, buf, BSZ);
    if (n > 0) {
        buf[n] = '\0';
        printf("%s\n", buf);
    }
}
```

- **Fungsi Utama:** Menerima data dari server dan mencetaknya ke layar.

- **Buffer `buf`** digunakan untuk menampung data sementara yang dibaca dari socket:
  - Karena data dari server bisa panjang dan tidak langsung ditampilkan ke layar satu per satu, maka digunakan buffer (penampung sementara) untuk menyimpan semua data yang dibaca dari socket agar bisa diproses sekaligus.
  - `BSZ` (2048 byte) adalah batas maksimal ukuran data yang bisa diterima dalam satu kali baca.
  - Setelah data diterima dan diakhiri dengan `\0` (null-terminator), `printf()` mencetak seluruh isi buffer ke layar.


### **3. Main Menu Loop**

```c
while (1) {
    printf("\n\x1b[95m=== TLD Menu ===\x1b[0m\n");
    printf("\x1b[96m[1]\x1b[0m Status Diri\n");
    printf("\x1b[96m[2]\x1b[0m Toko Senjata\n");
    printf("\x1b[96m[3]\x1b[0m Periksa Inventori & Atur Perlengkapan\n");
    printf("\x1b[96m[4]\x1b[0m Battle Mode\n");
    printf("\x1b[96m[5]\x1b[0m Keluar dari TLD\n");    
    printf("\x1b[96mChoose:\x1b[0m ");
    
    scanf("%d", &choice);
    getchar();  // Membersihkan newline di buffer
```

- Menu utama terdiri dari 5 opsi:
  1. Status Diri (`SHOW_STATS`)
  2. Toko Senjata (`SHOW_SHOP`)
  3. Inventori (`INVENTORY` dan `EQUIP`)
  4. Battle Mode (`BATTLE`)
  5. Keluar (`EXIT`)


### **4. Menampilkan Status Pemain (SHOW_STATS)**

```c
if (choice == 1) {
    strcpy(cmd, "SHOW_STATS");
    send(sock, cmd, strlen(cmd), 0);
    recv_print(sock);
}
```

- Mengirim perintah `"SHOW_STATS"` ke server.
- Menerima respons dari server menggunakan `recv_print()`.


### **5. Toko Senjata (SHOW_SHOP & BUY)**

```c
else if (choice == 2) {
    strcpy(cmd, "SHOW_SHOP");
    send(sock, cmd, strlen(cmd), 0);
    recv_print(sock);

    printf("\x1b[96mPilih senjata untuk dibeli (1-5), 0 untuk batal:\x1b[0m ");
    int b;
    scanf("%d", &b);
    getchar();

    if (b >= 1 && b <= 5) {
        snprintf(cmd, sizeof(cmd), "BUY %d", b);
        send(sock, cmd, strlen(cmd), 0);
        recv_print(sock);
    } else {
        printf("Batal atau pilihan tidak valid.\n");
    }
}
```

- Pemain melihat daftar senjata yang tersedia dan dapat membeli jika cukup gold.


### **6. Inventori & Equip Weapon (INVENTORY & EQUIP)**

```c
else if (choice == 3) {
    strcpy(cmd, "INVENTORY");
    send(sock, cmd, strlen(cmd), 0);
    recv_print(sock);

    printf("\x1b[96mPilih indeks senjata untuk equip (1-n), 0 untuk batal:\x1b[0m ");
    int e;
    scanf("%d", &e);
    getchar();

    if (e > 0) {
        snprintf(cmd, sizeof(cmd), "EQUIP %d", e);
        send(sock, cmd, strlen(cmd), 0);
        recv_print(sock);
    } else {
        printf("Batal equip.\n");
    }
}
```

- Pemain dapat melihat inventori dan memilih senjata untuk digunakan.


### **7. Battle Mode (BATTLE)**

```c
else if (choice == 4) {
    strcpy(cmd, "BATTLE");
    send(sock, cmd, strlen(cmd), 0);
    recv_print(sock);

    while (1) {
        char bcmd[64] = {0};
        printf("\x1b[96mBattle> \x1b[0m");
        scanf("%s", bcmd);
        getchar();

        send(sock, bcmd, strlen(bcmd), 0);
        if (!strcmp(bcmd, "exit")) {
            recv_print(sock);
            break;
        }
        recv_print(sock);
    }
}
```

- Pemain masuk ke mode pertempuran dan dapat mengetik `attack` atau `exit`.
- Respons akan ditampilkan langsung melalui `recv_print()`.


### **8. Keluar dari Program (EXIT)**

```c
else if (choice == 5) {
    strcpy(cmd, "EXIT");
    send(sock, cmd, strlen(cmd), 0);
    printf("\x1b[33mGoodbye, adventurer!\x1b[0m\n");
    break;
}
```

- Mengirim perintah `"EXIT"` dan keluar dari permainan.


### **9. Error Handling**

- Input menu yang tidak valid akan menghasilkan pesan:
  ```
  Pilihan tidak valid.
  ```

- Battle Mode juga menangani input salah dengan memberi pesan dari server.

---

### **shop.c**


### **1. Struktur Data Weapon**

```c
#define MAX_WEAPONS 5
#define MAX_NAME 32

typedef struct {
    char name[MAX_NAME];
    int price;
    int damage;
    char passive[64];
} Weapon;
```

- **MAX_WEAPONS**: Jumlah maksimal senjata di toko (`5` senjata).
- **MAX_NAME**: Panjang maksimal nama senjata (`32` karakter).

**Struktur Weapon** terdiri dari:
- `name`: Nama senjata (contoh: "Dark Repulsor").
- `price`: Harga senjata dalam gold.
- `damage`: Damage dasar yang diberikan senjata.
- `passive`: Passive skill yang dimiliki senjata. Jika tidak ada passive, maka string kosong (`""`).

---

### **2. Inisialisasi Senjata di Toko**

```c
Weapon shopWeapons[MAX_WEAPONS] = {
    {"Dark Repulsor", 200, 13, ""},
    {"Elucidator", 200, 12, ""},
    {"Gilta Brille", 250, 14, ""},
    {"Mercenary Twinblade", 150, 9, "Quick Slash"},
    {"Togetsu Waxing", 300, 15, "Thunder Blast"}
};
```

- **Dark Repulsor**: Harga `200 gold`, damage `13`, tanpa passive.
- **Elucidator**: Harga `200 gold`, damage `12`, tanpa passive.
- **Gilta Brille**: Harga `250 gold`, damage `14`, tanpa passive.
- **Mercenary Twinblade**: Harga `150 gold`, damage `9`, passive `"Quick Slash"`.
  - `"Quick Slash"`: Kemungkinan serangan tambahan.
- **Togetsu Waxing**: Harga `300 gold`, damage `15`, passive `"Thunder Blast"`.
  - `"Thunder Blast"`: Tambahan damage jika passive aktif.

**Penjelasan:**
- Senjata yang memiliki passive akan memberikan efek tambahan ketika passive tersebut aktif. Efek ini akan diproses saat battle di `dungeon.c`.


### **3. Fungsi get_weapon_count()**

```c
int get_weapon_count() {
    return MAX_WEAPONS;
}
```

- Fungsi ini hanya mengembalikan jumlah senjata yang ada di toko (`5` senjata).
- Digunakan di `dungeon.c` untuk menampilkan jumlah senjata saat pemain membuka toko (`SHOW_SHOP`).


### **4. Fungsi get_weapon()**

```c
Weapon get_weapon(int index) {
    return shopWeapons[index];
}
```

- Fungsi ini digunakan untuk mengambil data senjata berdasarkan indeks (`index`).
- **Parameter:**
  - `index`: Indeks senjata (`0` hingga `MAX_WEAPONS - 1`).

- **Return Value:**  
  - Mengembalikan struktur `Weapon` yang sesuai dengan indeks yang diberikan.

**Contoh Penggunaan:**
- Jika `get_weapon(1)` dipanggil, maka akan mengembalikan senjata `"Elucidator"` dengan harga `200 gold` dan damage `12`.


### **5. Mekanisme Pembelian Senjata di dungeon.c**

Meskipun fungsi pembelian senjata (`BUY`) tidak langsung diimplementasikan di `shop.c`, namun `shop.c` berfungsi sebagai **penyedia data senjata**. Berikut adalah mekanisme pembelian senjata di `dungeon.c` menggunakan fungsi dari `shop.c`:

```c
else if (!strncmp(buf, "BUY ", 4)) {
    int idx = atoi(buf + 4) - 1;
    char resp[BUF_SZ];

    if (idx < 0 || idx >= get_weapon_count()) {
        snprintf(resp, sizeof(resp), "Pilihan senjata tidak valid.\n");
    } else {
        Weapon w = get_weapon(idx);

        if (P->gold >= w.price) {
            P->gold -= w.price;
            add_inv(P, w);
            snprintf(resp, sizeof(resp), "Berhasil membeli %s!\nGold sisa: %d\n", w.name, P->gold);
        } else {
            snprintf(resp, sizeof(resp), "Gold tidak cukup untuk membeli %s.\n", w.name);
        }
    }
    send(sock, resp, strlen(resp), 0);
}
```

**Penjelasan:**
- Perintah `"BUY <index>"` diproses dengan mengambil senjata menggunakan `get_weapon()`.
- Jika `gold` pemain cukup (`P->gold >= w.price`), maka senjata tersebut akan ditambahkan ke inventori.
- Jika tidak cukup, maka pesan `"Gold tidak cukup"` akan dikirimkan ke client.


### **6. Passive Weapon Implementation**

- Senjata `"Mercenary Twinblade"` memiliki passive `"Quick Slash"`:
  - `"Quick Slash"` memungkinkan pemain menyerang dua kali secara acak.
  - Passive ini akan dicek saat battle di `dungeon.c`.

- Senjata `"Togetsu Waxing"` memiliki passive `"Thunder Blast"`:
  - `"Thunder Blast"` menambahkan `10 damage` secara acak.
  - Passive ini juga diimplementasikan di battle mode (`dungeon.c`).

---

### Revisi

Untuk revisi sendiri, saya hanya menambahkan kill count pada menu **status player**

![image](https://github.com/user-attachments/assets/69617114-cf06-4b8e-bc4e-41779cd6d7fe)

Awalnya tidak ada.

![image](https://github.com/user-attachments/assets/8d1af7ef-b8b1-4805-91dd-348bd3ad6309)

sekarang **sudah ada**.


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
// system
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
// hunter
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
// hunter
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
// system
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
// system
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
// hunter
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
// hunter
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
// hunter
void cleanup(int sig) {
    printf("\nMembersihkan shared memory...\n");
    shmdt(shared_data);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    exit(0);
}

// system
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

```bash
gcc -o system system.c -pthread
gcc -o hunter hunter.c -pthread
```

#### Jalankan Program

1. Jalankan `system` terlebih dahulu untuk menginisialisasi shared memory:

```bash
./system
```

2. Jalankan `hunter` di terminal lain untuk hunter register/login:

```bash
./hunter
```

### menu
```c
// hunter
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

// system
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
penambahan pada hunter.c
```c
#include <stdatomic.h>
```

### Dokumentasi

![image](https://github.com/user-attachments/assets/29372b09-534d-40e9-8db9-ea29b1d16927)

![image](https://github.com/user-attachments/assets/3c629f08-31f8-4f3f-b0c4-292d93f306d7)

![image](https://github.com/user-attachments/assets/2c5a6d2d-65f8-41de-ae78-9b07d75dac2a)

![image](https://github.com/user-attachments/assets/33b2c735-2fe6-431b-bfce-707cf16b231d)

![image](https://github.com/user-attachments/assets/6cd62911-009f-49cc-9bd6-8a23c9002f4e)

![image](https://github.com/user-attachments/assets/de87b879-0de8-416b-8f7f-9201a75f20f1)

![image](https://github.com/user-attachments/assets/f6416e6e-3199-48b3-bb8d-ec7b5dfe96cf)

![image](https://github.com/user-attachments/assets/ee070c1e-0ff9-4680-bf22-a86b701fda30)

![image](https://github.com/user-attachments/assets/8b7e0c6d-38e7-4d30-9ad5-d2c528bd6d24)

![image](https://github.com/user-attachments/assets/2cbd05ab-3c79-4a38-b764-e1373350de9d)

![image](https://github.com/user-attachments/assets/f8793fef-bfb7-4d0f-8f98-b537c4a4498d)

![image](https://github.com/user-attachments/assets/4af83bef-97ce-479d-9e27-fd4b60e626a3)

![image](https://github.com/user-attachments/assets/aee87ef2-c0b0-4c9c-a0b5-7c52a48bd71f)

![image](https://github.com/user-attachments/assets/3942c7db-d3b1-4015-a1d3-5279085df72a)

![image](https://github.com/user-attachments/assets/89bc53a2-17b4-4f56-95d1-142b5cef161f)

![image](https://github.com/user-attachments/assets/2a2abb0d-ffd9-49d8-8018-53117798a7ec)

![image](https://github.com/user-attachments/assets/48cd1c1a-8f4d-411f-95c1-4b1f7c2ae0b4)

![image](https://github.com/user-attachments/assets/36171f9b-facc-4829-bb77-f1e25389de8d)

![image](https://github.com/user-attachments/assets/bdec2374-a149-4c58-9964-9da830afc197)

![image](https://github.com/user-attachments/assets/ce3b5446-8551-4c6f-9751-e52c77c9195b)

![image](https://github.com/user-attachments/assets/c048e37c-92d7-489c-b710-8413f9529d1a)
