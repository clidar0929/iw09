xv6-net
=======
This project is implement TCP/IP Network Stack on xv6.

## Features

- [x] Network device
  - [x] PCI
    - [x] Bus scan
    - [x] Find device driver
  - [x] Intel 8254x (e1000) driver
    - [x] Initialization
    - [x] Basic operation of RX/TX with DMA
    - [x] Interrupt trap
    - [x] Detect interrupt souce (if multiple NICs)
  - [x] Device abstraction
    - [x] Define structure for physical device abstraction (struct netdev)
    - [x] Support multiple link protocols and physical devices
- [x] Protocols
  - [x] Ethernet
  - [x] ARP
  - [x] IP
  - [x] ICMP
  - [x] UDP
  - [x] TCP
- [x] Network Interface
  - [x] Interface abstraction
    - [x] Define structure for logical interface abstraction (struct netif)
    - [x] Support multiple address family and logical interfaces
  - [x] Configuration
    - [x] Makeshift systemcalls and commands
      - [x] ifget
      - [x] ifset
      - [x] ifup
      - [x] ifdown
- [x] Socket API
  - [x] Systemcalls
    - [x] socket
    - [x] bind
    - [x] connect
    - [x] listen
    - [x] accept
    - [x] recv
    - [x] send
    - [x] recvfrom
    - [x] sendto
  - [x] Socket descriptor (compatible with File descriptor)
  - [x] Socket address (struct sockaddr)

## Task

- [ ] ARP resolution waiting queue (Currently discards data)
- [ ] TCP timer (Currently retransmission timer is not working)
- [ ] DHCP client
- [ ] DNS stub resolver

## License

xv6: Under the MIT License. See [LICENSE](./LICENSE) file.

Additional code: Under the MIT License. See header of each source code.
