#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    // Open syslog with LOG_USER facility
    openlog("writer", LOG_PID, LOG_USER);
    
    // Check if the number of arguments is correct
    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments");
        printf("Error: Invalid number of arguments.\n");
        printf("Usage: writer <directory-path-to-file> <string>\n");
        closelog();
        exit(1);
    }
    
    char *writefile = argv[1];
    char *writestr = argv[2];
    
    // Try to write to the file
    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Failed to create or write to file: %s", strerror(errno));
        printf("Error: Failed to create or write to file\n");
        closelog();
        exit(1);
    }
    
    // Write the string to the file
    if (fprintf(file, "%s", writestr) < 0) {
        syslog(LOG_ERR, "Failed to write string to file: %s", strerror(errno));
        printf("Error: Failed to create or write to file\n");
        fclose(file);
        closelog();
        exit(1);
    }
    
    // Close the file
    if (fclose(file) != 0) {
        syslog(LOG_ERR, "Failed to close file: %s", strerror(errno));
        printf("Error: Failed to create or write to file\n");
        closelog();
        exit(1);
    }
    
    // Log successful write with LOG_DEBUG level
    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
    
    printf("File created and written successfully\n");
    
    // Close syslog
    closelog();
    exit(0);
}
