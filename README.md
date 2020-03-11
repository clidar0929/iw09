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
    - [x] Define structure for device abstraction (struct netdev)
    - [x] Support multiple link protocols and network devices
- [x] Protocols
  - [x] Ethernet
  - [x] ARP
  - [x] IP
  - [x] ICMP
  - [x] UDP
  - [x] TCP
- [x] Network Interface
- [x] Socket API
  
## License

xv6: Under the MIT License. See [LICENSE](./LICENSE) file.

Additional code: Under the MIT License. See header of each source code.
