#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <dirent.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define SECRETS_DIR "secrets"

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

void list_files() {
    list_secrets_files();

    int sock = connect_to_server();
    if (sock >= 0) {
        list_server_files(sock);
        send(sock, "EXIT", 4, 0);
        close(sock);
    } else {
        printf("\nAvailable JPEG files to download from server:\n");
        printf("(Cannot connect to server to retrieve file list)\n");
    }
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

int main() {
    printf("Current directory: ");
    system("pwd");
    create_directories();

    DIR *dir = opendir(SECRETS_DIR);
    if (dir) {
        struct dirent *entry;
        int has_files = 0;
        
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                has_files = 1;
                break;
            }
        }
        
        closedir(dir);
        
        if (!has_files) {
            create_sample_file();
        }
    }
    
    while (1) {
        printf("\n=== The Legend of Rootkids ===\n");
        printf("1. Upload and decrypt file\n");
        printf("2. Download JPEG file\n");
        printf("3. List available files\n");
        printf("4. Create sample file\n");
        printf("5. Exit\n");
        printf("Choose option: ");
        
        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        while (getchar() != '\n');
        
        if (choice == 5) break;
        
        int sock;
        
        switch (choice) {
            case 1: {
                sock = connect_to_server();
                if (sock < 0) continue;
                
                list_secrets_files();
                
                printf("Enter filename from secrets directory: ");
                char filename[100];
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\n")] = '\0';
                upload_file(sock, filename);
                break;
            }
            case 2: {
                sock = connect_to_server();
                if (sock < 0) continue;
                
                list_server_files(sock);
                
                printf("Enter JPEG filename to download: ");
                char filename[100];
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\n")] = '\0';
                download_file(sock, filename);
                break;
            }
            case 3:
                list_files();
                continue;
            case 4:
                create_sample_file();
                continue;
            default:
                printf("Invalid choice\n");
                continue;
        }
        
        send(sock, "EXIT", 4, 0);
        close(sock);
    }
    return 0;
}
