#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define LOG_FILE "server.log"
#define DB_DIR "database"

int server_socket = -1;

int run_as_daemon = 0;

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
            // Check for jpeg extension
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

int main(int argc, char *argv[]) {
    run_as_daemon = 1;
    if (argc > 1 && strcmp(argv[1], "-f") == 0) {
        run_as_daemon = 0;
    }

    signal(SIGTERM, cleanup);
    signal(SIGINT, cleanup);
    
    create_directories();

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket error");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt error");
        close(server_socket);
        exit(1);
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind error");
        close(server_socket);
        exit(1);
    }
    
    if (listen(server_socket, 10) < 0) {
        perror("Listen error");
        close(server_socket);
        exit(1);
    }
    
    if (run_as_daemon) {
        pid_t pid = fork();
        if (pid < 0) {
            close(server_socket);
            exit(1);
        }
        if (pid > 0) {
            printf("Server started as daemon with PID: %d\n", pid);
            exit(0);
        }
        umask(0);
        setsid();

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    } else {
        printf("Server running in foreground mode on port %d\n", PORT);
    }
    
    log_action("Server", "START", "Server started");

    signal(SIGCHLD, SIG_IGN);
    
    if (!run_as_daemon) {
        printf("Waiting for connections...\n");
    }
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
        
        if (client_socket < 0) {
            log_action("Server", "ERROR", "Accept failed");
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        log_action("Server", "CONNECT", client_ip);
        
        pid_t child_pid = fork();
        if (child_pid == 0) {
            close(server_socket);
            handle_client(client_socket);
            exit(0);
        } else if (child_pid > 0) {
            close(client_socket);
        } else {
            log_action("Server", "ERROR", "Fork failed");
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}
