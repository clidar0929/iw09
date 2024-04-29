#include "types.h"
#include "user.h"
#include "socket.h"

#define SERVER_IP "192.168.0.8" // Change this to your server's IP address
#define SERVER_PORT 5000            // Change this to your server's port

#define NUM_CONNECTIONS_INCREMENT 10 // Change this to the increment of connections to test
#define MAX_CONNECTIONS 100           // Change this to the maximum number of connections to test

int main(int argc, char *argv[]) {
    int soc, ret;
    struct sockaddr_in server_addr, peer;
    ip_addr_t addr;

    // Parse the server IP address
    if (ip_addr_pton(SERVER_IP, &addr) == -1) {
        printf(1, "Error parsing IP address\n");
        exit();
    }
    // Loop to create the specified number of connections
    for (int i = 1; i <= MAX_CONNECTIONS; i++) {
        // Create a TCP socket
        soc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (soc == -1) {
            printf(1, "Socket creation failed\n");
            exit();
        }
        printf(1, "\nSocket Success");

        // Set up the server address structure
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr = addr;
        server_addr.sin_port = hton16(SERVER_PORT+i);

        if (bind(soc, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            printf(1, "bind: failure\n");
            close(soc);
            exit();
        }

        listen(soc, 100);
       
        
    }
}


