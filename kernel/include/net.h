/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * OpSec Hardened Network Subsystem Interface (Ethernet / ARP / IPv4 / ICMP)
 * GPLv3 Licensed.
 */

#ifndef SELD_NET_H
#define SELD_NET_H

#include <stdint.h>
#include <stddef.h>

#define ETH_ALEN 6
#define ETH_HLEN 14
#define ETH_MIN_LEN 60
#define ETH_MAX_LEN 1518

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806

#define ARP_HTYPE_ETHERNET 1
#define ARP_PTYPE_IPV4     0x0800
#define ARP_OP_REQUEST     1
#define ARP_OP_REPLY       2

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

/* Endianness Conversion */
static inline uint16_t htons(uint16_t val) {
    return (uint16_t)(((val & 0xFF) << 8) | ((val >> 8) & 0xFF));
}

static inline uint16_t ntohs(uint16_t val) {
    return htons(val);
}

static inline uint32_t htonl(uint32_t val) {
    return ((val & 0xFF) << 24) |
           (((val >> 8) & 0xFF) << 16) |
           (((val >> 16) & 0xFF) << 8) |
           ((val >> 24) & 0xFF);
}

static inline uint32_t ntohl(uint32_t val) {
    return htonl(val);
}

#define MAKE_IP(a, b, c, d) \
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

/* Wire Protocol Structures */
struct eth_header {
    uint8_t  dest[ETH_ALEN];
    uint8_t  src[ETH_ALEN];
    uint16_t ethertype; // Big Endian
} __attribute__((packed));

struct arp_header {
    uint16_t htype;      // Big Endian
    uint16_t ptype;      // Big Endian
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t opcode;     // Big Endian
    uint8_t  sender_mac[ETH_ALEN];
    uint32_t sender_ip;  // Wire order
    uint8_t  target_mac[ETH_ALEN];
    uint32_t target_ip;  // Wire order
} __attribute__((packed));

struct ipv4_header {
    uint8_t  ihl : 4;
    uint8_t  version : 4;
    uint8_t  tos;
    uint16_t total_length; // Big Endian
    uint16_t id;           // Big Endian
    uint16_t flags_frag;   // Big Endian
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;     // Big Endian
    uint32_t src_ip;       // Wire order
    uint32_t dest_ip;      // Wire order
} __attribute__((packed));

struct icmp_header {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;     // Big Endian
    uint16_t id;           // Big Endian
    uint16_t sequence;     // Big Endian
} __attribute__((packed));

struct tcp_header {
    uint16_t src_port;     // Big Endian
    uint16_t dest_port;    // Big Endian
    uint32_t seq;          // Big Endian
    uint32_t ack;          // Big Endian
    uint8_t  data_offset;  // High 4 bits = header len in 32-bit dwords
    uint8_t  flags;
    uint16_t window_size;  // Big Endian
    uint16_t checksum;     // Big Endian
    uint16_t urgent_ptr;   // Big Endian
} __attribute__((packed));

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

struct udp_header {
    uint16_t src_port;     // Big Endian
    uint16_t dest_port;    // Big Endian
    uint16_t length;       // Big Endian
    uint16_t checksum;     // Big Endian
} __attribute__((packed));

struct dns_header {
    uint16_t id;           // Big Endian
    uint16_t flags;        // Big Endian
    uint16_t qdcount;      // Big Endian
    uint16_t ancount;      // Big Endian
    uint16_t nscount;      // Big Endian
    uint16_t arcount;      // Big Endian
} __attribute__((packed));

#define ARP_TABLE_SIZE 16

struct arp_entry {
    uint32_t ip;
    uint8_t  mac[ETH_ALEN];
    uint8_t  valid;
    uint64_t timestamp_ms;
};

struct net_config {
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint8_t  mac[ETH_ALEN];
    uint8_t  link_up;
};

struct net_stats {
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_arp;
    uint64_t tx_arp;
    uint64_t rx_ipv4;
    uint64_t tx_ipv4;
    uint64_t rx_icmp;
    uint64_t tx_icmp;
    uint64_t rx_dropped;
    uint64_t rx_checksum_errors;
};

/* OpSec Network Subsystem API */
void net_init(void);
void net_poll(void);
int  net_is_online(void);
struct net_config net_get_config(void);
void net_set_config(uint32_t ip, uint32_t netmask, uint32_t gateway);
struct net_stats  net_get_stats(void);

uint16_t net_checksum(const void* data, size_t length);
int  net_parse_ip(const char* str, uint32_t* ip_out);
void net_format_ip(uint32_t ip, char* buf, size_t buflen);
void net_format_mac(const uint8_t* mac, char* buf, size_t buflen);

int  net_arp_lookup(uint32_t ip, uint8_t* mac_out);
int  net_arp_request(uint32_t target_ip);
int  net_get_arp_table(struct arp_entry* out_entries, size_t max_entries);

int  net_send_ip(uint32_t dest_ip, uint8_t protocol, const void* payload, size_t payload_len);
int  net_ping(uint32_t target_ip, uint16_t seq, uint32_t* rtt_ms);

/* TCP & HTTP Client API */
int  net_http_get(uint32_t server_ip, uint16_t port, const char* path, const char* host,
                  uint8_t* out_buf, size_t max_out_len, size_t* out_len);
int  net_download_to_fs(uint32_t server_ip, uint16_t port, const char* path, const char* local_path);

/* Streaming TCP Socket API */
#define MAX_TCP_SOCKETS 4
int  net_tcp_socket_connect(uint32_t server_ip, uint16_t port);
int  net_tcp_socket_send(int sock_id, const void* data, size_t len);
int  net_tcp_socket_recv(int sock_id, void* out_buf, size_t max_len, uint32_t timeout_ms);
int  net_tcp_socket_close(int sock_id);
int  net_tcp_socket_is_connected(int sock_id);

/* UDP & DNS Resolution API */
int  net_send_udp(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void* payload, size_t payload_len);
int  net_dns_resolve(const char* hostname, uint32_t* ip_out);

#endif /* SELD_NET_H */
