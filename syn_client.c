#include "types.h"
#include "user.h"
#include "socket.h"

int main(int argc, char *argv[]) {
    int soc, ret;
    struct sockaddr_in server_addr;
    char buf[2048];
    ip_addr_t addr = INADDR_ANY;
    unsigned char * address;

    // Create a TCP socket
    soc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (soc == -1) {
        printf(1, "socket: failure\n");
        exit();
    }
    printf(1, "socket: success, soc=%d\n", soc);


    if(ip_addr_pton("192.168.0.8", &addr) == -1) {
        printf(1, "error parsing ip\n");
        exit();
    }

    // Set up the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = addr;
    server_addr.sin_port = hton16(7); // Port 7 (Echo Protocol)

    address = (unsigned char *)&addr;
    // Print desired server address
    printf(1, "Attempting to connect to %d.%d.%d.%d:%d\n", address[0], address[1], address[2], address[3], ntoh16(server_addr.sin_port));

    // Connect to the server
    ret = connect(soc, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret == -1) {
        printf(1, "connect: failure\n");
        close(soc);
        exit();
    }

    // Send data to the server
    printf(1, "Sending data to server...\n");
    char *message = "Hello, this is a test message.";
    send(soc, message, strlen(message));

    // Receive echoed data from the server
    printf(1, "Receiving echoed data from server...\n");
    ret = recv(soc, buf, sizeof(buf));
    if (ret <= 0) {
        printf(1, "EOF\n");
        close(soc);
        exit();
    }
    buf[ret] = '\0'; // Null-terminate the received data
    printf(1, "Received: %s\n", buf);

    // Close the socket and exit
    close(soc);
    exit();
}
