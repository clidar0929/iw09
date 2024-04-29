#include "types.h"
#include "user.h"
#include "socket.h"

#define SERVER_IP "192.168.0.8" // Change this to your server's IP address
#define SERVER_PORT 5000            // Change this to your server's port

#define NUM_CONNECTIONS_INCREMENT 10 // Change this to the increment of connections to test
#define MAX_CONNECTIONS 100           // Change this to the maximum number of connections to test

int main(int argc, char *argv[]) {
    int soc, ret;
    struct sockaddr_in server_addr;
    ip_addr_t addr;

    // Parse the server IP address
    if (ip_addr_pton(SERVER_IP, &addr) == -1) {
        printf(1, "Error parsing IP address\n");
        exit();
    }

    int time_taken_ms = 0;

    // Loop to create the specified number of connections
    for (int i = 1; i <= MAX_CONNECTIONS; i++) {
        // Create a TCP socket
        soc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (soc == -1) {
            printf(1, "Socket creation failed\n");
            exit();
        }

        // Set up the server address structure
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr = addr;
        server_addr.sin_port = hton16(SERVER_PORT+i);
        // Print desired server address

        // Get start time
        uint64_t start_time = uptime();

        // Connect to the server
        ret = connect(soc, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (ret == -1) {
            printf(1, "Connection failed\n");
            close(soc);
            exit();
        }

        // Get end time
        uint64_t end_time = uptime();

        // Calculate time taken in milliseconds
        time_taken_ms = time_taken_ms + (int)end_time - (int)start_time;
        if (!time_taken_ms){
            printf(1, "UHOH\n");
        }
        printf(1, "Connection %d established in %lu milliseconds\n", i, (unsigned long)time_taken_ms);

        if (i % 10 == 0){
            printf(1, "Time for %d connections: %d", i, time_taken_ms);
        }

    }
}

