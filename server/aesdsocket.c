#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

static volatile sig_atomic_t g_shutdown = 0;
static int g_server_fd = -1;
static int g_client_fd = -1;

/**
 * Signal handler for SIGINT and SIGTERM
 */
void signal_handler(int sig)
{
    (void)sig;
    g_shutdown = 1;
    syslog(LOG_INFO, "Caught signal, exiting");
}

/**
 * Setup signal handlers
 */
void setup_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/**
 * Cleanup function to close sockets and remove data file
 */
void cleanup(void)
{
    if (g_client_fd >= 0) {
        close(g_client_fd);
        g_client_fd = -1;
    }
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
    unlink(DATA_FILE);
}

/**
 * Get client IP address as string
 */
void get_client_ip(int client_fd, char *ip_str, size_t ip_str_len)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    if (getpeername(client_fd, (struct sockaddr *)&addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &addr.sin_addr, ip_str, ip_str_len);
    } else {
        strncpy(ip_str, "unknown", ip_str_len - 1);
        ip_str[ip_str_len - 1] = '\0';
    }
}

/**
 * Receive data packet until newline is found
 * Returns dynamically allocated buffer with the packet (including newline)
 * Caller must free the buffer
 */
char *receive_packet(int client_fd, size_t *packet_len)
{
    char *buffer = NULL;
    size_t data_len = 0;
    char recv_buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    int newline_found = 0;

    *packet_len = 0;

    while (!newline_found && !g_shutdown) {
        bytes_received = recv(client_fd, recv_buffer, sizeof(recv_buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                // Connection closed by client
                break;
            }
            if (errno == EINTR) {
                // Interrupted by signal, continue
                continue;
            }
            // Error receiving
            break;
        }

        // Null-terminate for string operations
        recv_buffer[bytes_received] = '\0';

        // Check for newline
        char *newline_ptr = strchr(recv_buffer, '\n');
        if (newline_ptr) {
            newline_found = 1;
            // Include the newline in the packet
            size_t bytes_to_copy = newline_ptr - recv_buffer + 1;
            
            // Resize buffer to accommodate new data
            size_t new_size = data_len + bytes_to_copy + 1;
            char *new_buffer = realloc(buffer, new_size);
            if (!new_buffer) {
                syslog(LOG_ERR, "Failed to allocate memory for packet");
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
            memcpy(buffer + data_len, recv_buffer, bytes_to_copy);
            data_len += bytes_to_copy;
            buffer[data_len] = '\0';
        } else {
            // No newline yet, append all received data
            size_t new_size = data_len + bytes_received + 1;
            char *new_buffer = realloc(buffer, new_size);
            if (!new_buffer) {
                syslog(LOG_ERR, "Failed to allocate memory for packet");
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
            memcpy(buffer + data_len, recv_buffer, bytes_received);
            data_len += bytes_received;
            buffer[data_len] = '\0';
        }
    }

    if (buffer && data_len > 0) {
        *packet_len = data_len;
        return buffer;
    }

    free(buffer);
    return NULL;
}

/**
 * Append data to file
 */
int append_to_file(const char *data, size_t data_len)
{
    int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open data file: %s", strerror(errno));
        return -1;
    }

    ssize_t written = write(fd, data, data_len);
    close(fd);

    if (written < 0 || (size_t)written != data_len) {
        syslog(LOG_ERR, "Failed to write to data file: %s", strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * Read entire file and send to client
 * Reads and sends in chunks to handle files larger than available RAM
 */
int send_file_to_client(int client_fd)
{
    FILE *fp = fopen(DATA_FILE, "r");
    if (!fp) {
        syslog(LOG_ERR, "Failed to open data file for reading: %s", strerror(errno));
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    ssize_t bytes_sent;

    // Read and send file in chunks
    while (!g_shutdown && (bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        size_t offset = 0;
        while (offset < bytes_read && !g_shutdown) {
            bytes_sent = send(client_fd, buffer + offset, bytes_read - offset, 0);
            if (bytes_sent < 0) {
                if (errno == EINTR) {
                    continue;
                }
                syslog(LOG_ERR, "Failed to send data to client: %s", strerror(errno));
                fclose(fp);
                return -1;
            }
            offset += bytes_sent;
        }
    }

    fclose(fp);
    return 0;
}

/**
 * Handle client connection
 */
void handle_client(int client_fd)
{
    char client_ip[INET_ADDRSTRLEN];
    get_client_ip(client_fd, client_ip, sizeof(client_ip));
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    while (!g_shutdown) {
        size_t packet_len = 0;
        char *packet = receive_packet(client_fd, &packet_len);

        if (!packet) {
            // Connection closed or error
            break;
        }

        // Append packet to file
        if (append_to_file(packet, packet_len) != 0) {
            free(packet);
            break;
        }

        // Send entire file content back to client
        if (send_file_to_client(client_fd) != 0) {
            free(packet);
            break;
        }

        free(packet);
    }

    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    close(client_fd);
}

int main(int argc, char *argv[])
{
    int opt;
    int daemon_mode = 0;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Open syslog
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Setup signal handlers
    setup_signal_handlers();

    // Create socket
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        closelog();
        return -1;
    }

    // Set socket option to reuse address
    int optval = 1;
    if (setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        syslog(LOG_ERR, "Failed to set socket option: %s", strerror(errno));
        close(g_server_fd);
        closelog();
        return -1;
    }

    // Bind socket to port 9000
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(g_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "Failed to bind socket: %s", strerror(errno));
        close(g_server_fd);
        closelog();
        return -1;
    }

    // Listen for connections
    if (listen(g_server_fd, 5) < 0) {
        syslog(LOG_ERR, "Failed to listen on socket: %s", strerror(errno));
        close(g_server_fd);
        closelog();
        return -1;
    }

    // Fork to daemon if requested
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "Failed to fork: %s", strerror(errno));
            close(g_server_fd);
            closelog();
            return -1;
        }
        if (pid > 0) {
            // Parent process exits
            exit(EXIT_SUCCESS);
        }
        // Child process continues
    }

    // Main loop: accept connections
    while (!g_shutdown) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        g_client_fd = accept(g_server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        
        if (g_client_fd < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, check if we should shutdown
                continue;
            }
            if (!g_shutdown) {
                syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            }
            break;
        }

        // Handle client connection
        handle_client(g_client_fd);
        g_client_fd = -1;
    }

    // Cleanup
    cleanup();
    closelog();

    return 0;
}

