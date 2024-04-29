#include "types.h"
#include "user.h"
#include "socket.h"



// Main function to start a TCP Echo Server
int main(int argc, char *argv[]) {
    int soc, acc, peerlen, ret; // Socket descriptors and variables for peer information
    struct sockaddr_in self, peer; // Structs to hold server and client socket addresses
    unsigned char *addr; // Pointer to hold the address information
    char buf[2048]; // Buffer to hold received data

    // Print a message indicating the server is starting
    printf(1, "Starting TCP Echo Server\n");

    // Create a new TCP socket
    soc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (soc == -1) {
        // Socket creation failed
        printf(1, "socket: failure\n");
        exit();
    }
    // Socket creation successful, print the socket descriptor
    printf(1, "socket: success, soc=%d\n", soc);

    // Set up the server address structure
    self.sin_family = AF_INET;
    self.sin_addr = INADDR_ANY;
    ip_addr_pton("172.16.100.2", &self.sin_addr);
    self.sin_port = hton16(7); // Port 7 (Echo Protocol)

    // Bind the socket to the server address
    if (bind(soc, (struct sockaddr *)&self, sizeof(self)) == -1) {
        // Binding failed
        printf(1, "bind: failure\n");
        close(soc);
        exit();
    }
    // Binding successful, print the server address
    addr = (unsigned char *)&self.sin_addr;
    printf(1, "bind: success, self=%d.%d.%d.%d:%d\n", addr[0], addr[1], addr[2], addr[3], ntoh16(self.sin_port));

    // Start listening for incoming connections
    listen(soc, 100);
    printf(1, "waiting for connection...\n");

    // Accept an incoming connection
    peerlen = sizeof(peer);
    acc = accept(soc, (struct sockaddr *)&peer, &peerlen);
    if (acc == -1) {
        // Accepting connection failed
        printf(1, "accept: failure\n");
        close(soc);
        exit();
    }
    // Accepting connection successful, print client address
    addr = (unsigned char *)&peer.sin_addr;
    printf(1, "accept: success, peer=%d.%d.%d.%d:%d\n", addr[0], addr[1], addr[2], addr[3], ntoh16(peer.sin_port));

    // Receive and print the TCP handshake packets
    ret = recv(acc, buf, sizeof(buf));
    if (ret <= 0) {
        // End of file or error occurred
        printf(1, "EOF\n");
        close(soc);
        exit();
    }


    // Main loop to receive and echo data
    printf(1, "Echoing data...\n");
    while (1) {
        // Receive data from the client
        ret = recv(acc, buf, sizeof(buf));
        if (ret <= 0) {
            // End of file or error occurred
            printf(1, "EOF\n");
            break;
        }
        // Data received successfully, print received data and echo it back
        printf(1, "recv: %d bytes data received\n", ret);
        hexdump(buf, ret);
        send(acc, buf, ret);
    }

    // Close the socket and exit
    close(soc);
    exit();
}
