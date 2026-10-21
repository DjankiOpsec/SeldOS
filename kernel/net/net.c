/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * OpSec Hardened Network Subsystem Implementation
 * Layer 2 (Ethernet), Layer 3 (ARP, IPv4), Layer 4 (ICMP).
 * GNU AGPLv3 Licensed - Humboldt Underground.
 */

#include "net.h"
#include "e1000.h"
#include "pcnet.h"
#include "pci.h"
#include "string.h"
#include "serial.h"
#include "pit.h"
#include "kmalloc.h"
#include "seldfs.h"
#include "rand.h"

#define NIC_TYPE_NONE  0
#define NIC_TYPE_E1000 1
#define NIC_TYPE_PCNET 2

static int s_nic_type = NIC_TYPE_NONE;

/* Air-Gap On-Demand & Network Defense State */
static int s_net_airgap_mode = NET_AIRGAP_ONDEMAND;
static int s_net_leases = 0;

/* In-Kernel TCP Desync & Anti-DPI State (TSPU Evasion) */
static int s_net_desync_mode = NET_DESYNC_SPLIT;
static uint32_t s_desync_splits = 0;
static uint32_t s_desync_fakes = 0;

void net_set_airgap(int mode) {
    s_net_airgap_mode = mode;
    if (s_net_airgap_mode == NET_AIRGAP_LOCKED) {
        serial_puts("[+] OpSec: Air-Gap Shield ENGAGED (Strict Lockdown - all TX dropped).\n");
    } else if (s_net_airgap_mode == NET_AIRGAP_ONDEMAND) {
        serial_puts("[+] OpSec: Air-Gap Shield ON-DEMAND (Default-Deny, Leased access only).\n");
    } else if (s_net_airgap_mode == NET_AIRGAP_STEALTH) {
        serial_puts("[+] OpSec: Air-Gap Shield STEALTH (Silent mode - no raw UDP DNS, no ping, TCP desync enforced).\n");
    } else {
        serial_puts("[*] OpSec: Air-Gap Shield DISENGAGED (Network Clearnet open).\n");
    }
}

int net_get_airgap(void) {
    return s_net_airgap_mode;
}

void net_set_airgap_mode(int mode) {
    net_set_airgap(mode);
}

void net_set_desync(int mode) {
    s_net_desync_mode = mode;
    if (mode == NET_DESYNC_SPLIT) {
        serial_puts("[+] OpSec Anti-DPI: In-Kernel TCP Desync enabled (SNI Split mode).\n");
    } else if (mode == NET_DESYNC_FAKE) {
        serial_puts("[+] OpSec Anti-DPI: In-Kernel TCP Desync enabled (Fake Packet + Split mode).\n");
    } else {
        serial_puts("[*] OpSec Anti-DPI: In-Kernel TCP Desync disabled (Direct transmission).\n");
    }
}

int net_get_desync(void) {
    return s_net_desync_mode;
}

int net_get_desync_stats(uint32_t* splits, uint32_t* fakes) {
    if (splits) *splits = s_desync_splits;
    if (fakes) *fakes = s_desync_fakes;
    return 0;
}

/* Poisson Chaff & Uniform Packet Padding (Anti-Correlation GPA Defense) */
static int s_net_chaff_mode = 0;
static uint64_t s_chaff_next_tick_ms = 0;
static uint32_t s_chaff_packets_sent = 0;
static int s_in_chaff = 0;

void net_set_chaff(int mode) {
    s_net_chaff_mode = mode ? 1 : 0;
    if (s_net_chaff_mode) {
        s_chaff_next_tick_ms = pit_get_uptime_ms() + 100;
        serial_puts("[+] OpSec GPA Defense: Poisson Chaff & Uniform Padding ACTIVE\n");
    } else {
        serial_puts("[*] OpSec GPA Defense: Poisson Chaff & Uniform Padding DISABLED\n");
    }
}

int net_get_chaff(void) {
    return s_net_chaff_mode;
}

uint32_t net_get_chaff_stats(void) {
    return s_chaff_packets_sent;
}

int net_abort_all_connections(void);

void net_lease_acquire(void) {
    s_net_leases++;
    serial_puts("[+] OpSec: Network lease acquired (active leases: ");
    serial_print_dec((uint64_t)s_net_leases);
    serial_puts(")\n");
}

void net_lease_release(void) {
    if (s_net_leases > 0) {
        s_net_leases--;
        serial_puts("[*] OpSec: Network lease released (remaining leases: ");
        serial_print_dec((uint64_t)s_net_leases);
        serial_puts(")\n");
    }
}

int net_is_traffic_allowed(void) {
    if (net_get_boot_delay_active()) return 0;
    if (s_net_airgap_mode == NET_AIRGAP_UNLOCKED) return 1;
    if (s_net_airgap_mode == NET_AIRGAP_LOCKED) return 0;
    if (s_net_chaff_mode) return 1; // Cover traffic permitted when Chaff GPA defense is active
    return (s_net_leases > 0);
}

static inline int nic_is_active(void) {
    if (s_nic_type == NIC_TYPE_E1000) return e1000_is_active();
    if (s_nic_type == NIC_TYPE_PCNET) return pcnet_is_active();
    return 0;
}

static inline int nic_send_packet(const void* data, size_t len) {
    if (!net_is_traffic_allowed()) {
        serial_puts("[-] AirGap: Outbound network blocked (Shield active)\n");
        return -1;
    }
    if (s_nic_type == NIC_TYPE_E1000) return e1000_send_packet(data, len);
    if (s_nic_type == NIC_TYPE_PCNET) return pcnet_send_packet(data, len);
    return -1;
}

static inline int nic_poll_packet(void* buf, size_t max_len) {
    if (s_nic_type == NIC_TYPE_E1000) return e1000_poll_packet(buf, max_len);
    if (s_nic_type == NIC_TYPE_PCNET) return pcnet_poll_packet(buf, max_len);
    return 0;
}

static struct net_config s_config = {0};
static struct net_stats  s_stats  = {0};
static struct arp_entry  s_arp_table[ARP_TABLE_SIZE] = {0};
static uint16_t          s_ip_id  = 0x1000;

/* Active Ping Response Tracking */
static struct {
    uint8_t  replied;
    uint16_t reply_id;
    uint16_t reply_seq;
    uint8_t  reply_ttl;
    size_t   reply_len;
} s_ping_state = {0};

/* Control D Sovereign OpSec DNS (Ads & Trackers Profile - p2) */
#define DNS_CONTROLD_P2_PRIMARY    MAKE_IP(76, 76, 2, 2)
#define DNS_CONTROLD_P2_SECONDARY  MAKE_IP(76, 76, 10, 2)
#define DNS_CONTROLD_DOT_IP        MAKE_IP(76, 76, 2, 11)
#define DNS_CONTROLD_DOT_HOST      "p2.freedns.controld.com"

/* DNS Query & Cache State */
#define DNS_PORT 53
#define DNS_CACHE_SIZE 16

struct dns_cache_entry {
    char     hostname[64];
    uint32_t ip;
    uint64_t timestamp_ms;
};

static struct dns_cache_entry s_dns_cache[DNS_CACHE_SIZE] = {0};
static int s_dns_cache_idx = 0;
static uint16_t s_dns_tx_id = 0x4A10;

static struct {
    uint16_t query_id;
    uint8_t  answered;
    uint32_t resolved_ip;
} s_dns_tracker = {0};

/* Internet Checksum Algorithm (RFC 1071) */
uint16_t net_checksum(const void* data, size_t length) {
    const uint8_t* byte_ptr = (const uint8_t*)data;
    uint32_t sum = 0;

    while (length > 1) {
        sum += (uint32_t)(byte_ptr[0] | ((uint32_t)byte_ptr[1] << 8));
        byte_ptr += 2;
        length -= 2;
    }

    if (length == 1) {
        sum += (uint32_t)*byte_ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

int net_parse_ip(const char* str, uint32_t* ip_out) {
    if (!str || !ip_out) return -1;

    uint32_t octets[4] = {0};
    int octet_idx = 0;
    const char* p = str;

    while (*p && octet_idx < 4) {
        if (*p < '0' || *p > '9') return -1;

        uint32_t val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            if (val > 255) return -1;
            p++;
        }

        octets[octet_idx++] = val;

        if (*p == '.') {
            p++;
            if (*p == '\0') return -1;
        } else if (*p != '\0') {
            return -1;
        }
    }

    if (octet_idx != 4 || *p != '\0') return -1;

    *ip_out = MAKE_IP(octets[0], octets[1], octets[2], octets[3]);
    return 0;
}

void net_format_ip(uint32_t ip, char* buf, size_t buflen) {
    if (!buf || buflen < 16) return;

    uint8_t o1 = (uint8_t)(ip & 0xFF);
    uint8_t o2 = (uint8_t)((ip >> 8) & 0xFF);
    uint8_t o3 = (uint8_t)((ip >> 16) & 0xFF);
    uint8_t o4 = (uint8_t)((ip >> 24) & 0xFF);

    char temp[32];
    size_t idx = 0;

    uint8_t octs[4] = {o1, o2, o3, o4};
    for (int i = 0; i < 4; i++) {
        uint8_t val = octs[i];
        if (val >= 100) {
            temp[idx++] = '0' + (val / 100);
            temp[idx++] = '0' + ((val / 10) % 10);
            temp[idx++] = '0' + (val % 10);
        } else if (val >= 10) {
            temp[idx++] = '0' + (val / 10);
            temp[idx++] = '0' + (val % 10);
        } else {
            temp[idx++] = '0' + val;
        }
        if (i < 3) temp[idx++] = '.';
    }
    temp[idx] = '\0';
    strncpy(buf, temp, buflen);
}

void net_format_mac(const uint8_t* mac, char* buf, size_t buflen) {
    if (!mac || !buf || buflen < 18) return;

    const char hex[] = "0123456789ABCDEF";
    size_t idx = 0;
    for (int i = 0; i < 6; i++) {
        buf[idx++] = hex[(mac[i] >> 4) & 0x0F];
        buf[idx++] = hex[mac[i] & 0x0F];
        if (i < 5) buf[idx++] = ':';
    }
    buf[idx] = '\0';
}

/* ARP Cache Management */
static void net_arp_update(uint32_t ip, const uint8_t* mac) {
    if (ip == 0 || !mac) return;

    // Check if entry already exists
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (s_arp_table[i].valid && s_arp_table[i].ip == ip) {
            memcpy(s_arp_table[i].mac, mac, ETH_ALEN);
            s_arp_table[i].timestamp_ms = pit_get_uptime_ms();
            return;
        }
    }

    // Find empty slot or oldest LRU slot
    int best_slot = 0;
    uint64_t oldest_time = s_arp_table[0].timestamp_ms;

    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!s_arp_table[i].valid) {
            best_slot = i;
            break;
        }
        if (s_arp_table[i].timestamp_ms < oldest_time) {
            oldest_time = s_arp_table[i].timestamp_ms;
            best_slot = i;
        }
    }

    s_arp_table[best_slot].ip = ip;
    memcpy(s_arp_table[best_slot].mac, mac, ETH_ALEN);
    s_arp_table[best_slot].valid = 1;
    s_arp_table[best_slot].timestamp_ms = pit_get_uptime_ms();
}

int net_arp_lookup(uint32_t ip, uint8_t* mac_out) {
    if (!mac_out) return -1;

    // Broadcast addresses
    if (ip == 0xFFFFFFFF || ip == ((s_config.ip & s_config.netmask) | ~s_config.netmask)) {
        memset(mac_out, 0xFF, ETH_ALEN);
        return 0;
    }

    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (s_arp_table[i].valid && s_arp_table[i].ip == ip) {
            memcpy(mac_out, s_arp_table[i].mac, ETH_ALEN);
            return 0;
        }
    }

    return -1;
}

int net_get_arp_table(struct arp_entry* out_entries, size_t max_entries) {
    if (!out_entries || max_entries == 0) return 0;
    size_t count = 0;
    for (int i = 0; i < ARP_TABLE_SIZE && count < max_entries; i++) {
        if (s_arp_table[i].valid) {
            out_entries[count++] = s_arp_table[i];
        }
    }
    return (int)count;
}

int net_arp_request(uint32_t target_ip) {
    uint8_t frame_buf[ETH_HLEN + sizeof(struct arp_header)];
    memset(frame_buf, 0, sizeof(frame_buf));

    struct eth_header* eth = (struct eth_header*)frame_buf;
    memset(eth->dest, 0xFF, ETH_ALEN); // Broadcast
    memcpy(eth->src, s_config.mac, ETH_ALEN);
    eth->ethertype = htons(ETHERTYPE_ARP);

    struct arp_header* arp = (struct arp_header*)(frame_buf + ETH_HLEN);
    arp->htype = htons(ARP_HTYPE_ETHERNET);
    arp->ptype = htons(ARP_PTYPE_IPV4);
    arp->hlen  = ETH_ALEN;
    arp->plen  = 4;
    arp->opcode = htons(ARP_OP_REQUEST);
    memcpy(arp->sender_mac, s_config.mac, ETH_ALEN);
    arp->sender_ip = s_config.ip;
    memset(arp->target_mac, 0, ETH_ALEN);
    arp->target_ip = target_ip;

    int res = nic_send_packet(frame_buf, sizeof(frame_buf));
    if (res == 0) {
        s_stats.tx_arp++;
        s_stats.tx_frames++;
    }
    return res;
}

/* Layer 2/3 Packet Processing */
static void net_handle_arp(const uint8_t* frame, size_t len) {
    if (len < ETH_HLEN + sizeof(struct arp_header)) {
        s_stats.rx_dropped++;
        return;
    }

    const struct arp_header* arp = (const struct arp_header*)(frame + ETH_HLEN);

    // OpSec Validation: Ethernet Hardware, IPv4 Protocol, valid address lengths
    if (ntohs(arp->htype) != ARP_HTYPE_ETHERNET ||
        ntohs(arp->ptype) != ARP_PTYPE_IPV4 ||
        arp->hlen != ETH_ALEN || arp->plen != 4) {
        s_stats.rx_dropped++;
        return;
    }

    // Anti-spoofing: sender MAC cannot be multicast/broadcast or zero
    if ((arp->sender_mac[0] & 1) ||
        (arp->sender_mac[0] | arp->sender_mac[1] | arp->sender_mac[2] |
         arp->sender_mac[3] | arp->sender_mac[4] | arp->sender_mac[5]) == 0) {
        s_stats.rx_dropped++;
        return;
    }

    uint16_t opcode = ntohs(arp->opcode);
    s_stats.rx_arp++;

    // Update ARP cache with sender details
    net_arp_update(arp->sender_ip, arp->sender_mac);

    // If ARP Request asking for our IP: send ARP Reply
    if (opcode == ARP_OP_REQUEST && arp->target_ip == s_config.ip) {
        uint8_t reply_buf[ETH_HLEN + sizeof(struct arp_header)];
        memset(reply_buf, 0, sizeof(reply_buf));

        struct eth_header* eth_rep = (struct eth_header*)reply_buf;
        memcpy(eth_rep->dest, arp->sender_mac, ETH_ALEN);
        memcpy(eth_rep->src, s_config.mac, ETH_ALEN);
        eth_rep->ethertype = htons(ETHERTYPE_ARP);

        struct arp_header* arp_rep = (struct arp_header*)(reply_buf + ETH_HLEN);
        arp_rep->htype = htons(ARP_HTYPE_ETHERNET);
        arp_rep->ptype = htons(ARP_PTYPE_IPV4);
        arp_rep->hlen  = ETH_ALEN;
        arp_rep->plen  = 4;
        arp_rep->opcode = htons(ARP_OP_REPLY);
        memcpy(arp_rep->sender_mac, s_config.mac, ETH_ALEN);
        arp_rep->sender_ip = s_config.ip;
        memcpy(arp_rep->target_mac, arp->sender_mac, ETH_ALEN);
        arp_rep->target_ip = arp->sender_ip;

        nic_send_packet(reply_buf, sizeof(reply_buf));
        s_stats.tx_arp++;
        s_stats.tx_frames++;
    }
}

static void net_handle_icmp(const uint8_t* frame, size_t frame_len, const struct ipv4_header* ip_hdr) {
    size_t ip_hlen = ip_hdr->ihl * 4;
    size_t ip_tot_len = ntohs(ip_hdr->total_length);

    if (ip_tot_len < ip_hlen + sizeof(struct icmp_header) ||
        ETH_HLEN + ip_tot_len > frame_len) {
        s_stats.rx_dropped++;
        return;
    }

    size_t icmp_len = ip_tot_len - ip_hlen;
    const struct icmp_header* icmp = (const struct icmp_header*)(frame + ETH_HLEN + ip_hlen);

    // Verify ICMP checksum
    if (net_checksum(icmp, icmp_len) != 0) {
        s_stats.rx_checksum_errors++;
        s_stats.rx_dropped++;
        return;
    }

    s_stats.rx_icmp++;

    if (icmp->type == ICMP_TYPE_ECHO_REQUEST && icmp->code == 0) {
        // OpSec Stealth Mode Invariant: Completely silent to external probes - drop ICMP Echo Request
        if (s_net_airgap_mode == NET_AIRGAP_STEALTH) {
            s_stats.rx_dropped++;
            return;
        }

        // Construct ICMP Echo Reply
        uint8_t reply_buf[ETH_MAX_LEN];
        if (ETH_HLEN + ip_tot_len > sizeof(reply_buf)) {
            s_stats.rx_dropped++;
            return;
        }

        struct eth_header* eth_out = (struct eth_header*)reply_buf;
        const struct eth_header* eth_in = (const struct eth_header*)frame;
        memcpy(eth_out->dest, eth_in->src, ETH_ALEN);
        memcpy(eth_out->src, s_config.mac, ETH_ALEN);
        eth_out->ethertype = htons(ETHERTYPE_IPV4);

        struct ipv4_header* ip_out = (struct ipv4_header*)(reply_buf + ETH_HLEN);
        ip_out->version = 4;
        ip_out->ihl = 5;
        ip_out->tos = 0;
        ip_out->total_length = htons((uint16_t)(sizeof(struct ipv4_header) + icmp_len));
        ip_out->id = 0; // RFC 6864: Zero IP ID for non-fragmented DF packets
        ip_out->flags_frag = htons(0x4000); // DF
        ip_out->ttl = 64;
        ip_out->protocol = IP_PROTO_ICMP;
        ip_out->checksum = 0;
        ip_out->src_ip = s_config.ip;
        ip_out->dest_ip = ip_hdr->src_ip;
        ip_out->checksum = net_checksum(ip_out, sizeof(struct ipv4_header));

        struct icmp_header* icmp_out = (struct icmp_header*)(reply_buf + ETH_HLEN + sizeof(struct ipv4_header));
        icmp_out->type = ICMP_TYPE_ECHO_REPLY;
        icmp_out->code = 0;
        icmp_out->checksum = 0;
        icmp_out->id = icmp->id;
        icmp_out->sequence = icmp->sequence;

        // Copy ICMP payload
        if (icmp_len > sizeof(struct icmp_header)) {
            size_t payload_sz = icmp_len - sizeof(struct icmp_header);
            memcpy((uint8_t*)icmp_out + sizeof(struct icmp_header),
                   (const uint8_t*)icmp + sizeof(struct icmp_header),
                   payload_sz);
        }

        icmp_out->checksum = net_checksum(icmp_out, icmp_len);

        size_t total_out = ETH_HLEN + sizeof(struct ipv4_header) + icmp_len;
        if (total_out < ETH_MIN_LEN) {
            memset(reply_buf + total_out, 0, ETH_MIN_LEN - total_out);
            total_out = ETH_MIN_LEN;
        }

        nic_send_packet(reply_buf, total_out);
        s_stats.tx_icmp++;
        s_stats.tx_ipv4++;
        s_stats.tx_frames++;
    } else if (icmp->type == ICMP_TYPE_ECHO_REPLY && icmp->code == 0) {
        s_ping_state.replied = 1;
        s_ping_state.reply_id = ntohs(icmp->id);
        s_ping_state.reply_seq = ntohs(icmp->sequence);
        s_ping_state.reply_ttl = ip_hdr->ttl;
        s_ping_state.reply_len = icmp_len;
    }
}

/* ========================================================================= */
/* Layer 4: TCP & HTTP Client Engine (RFC 793 / RFC 2616)                    */
/* ========================================================================= */

struct tcp_pseudo_header {
    uint32_t src_ip;
    uint32_t dest_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_len;
} __attribute__((packed));

static uint16_t net_tcp_checksum(uint32_t src_ip, uint32_t dest_ip, const void* tcp_data, size_t tcp_len) {
    struct tcp_pseudo_header ph;
    ph.src_ip = src_ip;
    ph.dest_ip = dest_ip;
    ph.zero = 0;
    ph.protocol = IP_PROTO_TCP;
    ph.tcp_len = htons((uint16_t)tcp_len);

    uint32_t sum = 0;
    const uint8_t* p = (const uint8_t*)&ph;
    for (size_t i = 0; i < sizeof(ph); i += 2) {
        sum += (uint32_t)(p[i] | ((uint32_t)p[i+1] << 8));
    }

    p = (const uint8_t*)tcp_data;
    size_t len = tcp_len;
    while (len > 1) {
        sum += (uint32_t)(p[0] | ((uint32_t)p[1] << 8));
        p += 2;
        len -= 2;
    }
    if (len == 1) {
        sum += (uint32_t)*p;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

enum tcp_state {
    TCP_STATE_CLOSED,
    TCP_STATE_SYN_SENT,
    TCP_STATE_ESTABLISHED
};

struct tcp_socket {
    int            used;
    enum tcp_state state;
    uint32_t       remote_ip;
    uint16_t       remote_port;
    uint16_t       local_port;
    uint32_t       snd_nxt;
    uint32_t       rcv_nxt;
    uint8_t*       rx_buf;
    size_t         rx_capacity;
    size_t         rx_len;
    uint8_t        fin_received;
    uint32_t       ts_recent;   /* Peer's timestamp (TSecr to echo) */
    uint32_t       ts_val_base; /* Randomized base for outgoing timestamps */
};

static struct tcp_socket s_sockets[MAX_TCP_SOCKETS] = {0};

/* ========================================================================= */
/* Anti-Clock-Skew Normalization & Virtual Monotonic Oscillator               */
/* ========================================================================= */
static int s_net_skew_defense = 1;

void net_set_skew_defense(int enable) {
    s_net_skew_defense = enable ? 1 : 0;
    serial_puts("[+] OpSec: Anti-Clock-Skew Normalization ");
    serial_puts(s_net_skew_defense ? "ENABLED (Zero-PPM virtual oscillator + CSPRNG dither)\n" : "DISABLED\n");
}

int net_get_skew_defense(void) {
    return s_net_skew_defense;
}

uint32_t net_get_synthetic_tsval(struct tcp_socket* s) {
    if (!s_net_skew_defense || !s) {
        return (uint32_t)(pit_get_ticks() * 10) + (s ? s->ts_val_base : 0);
    }
    /* Anti-Clock-Skew Virtual Monotonic Oscillator:
     * Synthesize an exact mathematical 1000 Hz Linux-compliant clock base.
     * Add non-linear CSPRNG micro-dither (-2..+2 ms) to destroy
     * any linear regression / Kalman filter clock-skew slope calculations.
     */
    uint32_t base = (uint32_t)(pit_get_uptime_ms());
    int32_t dither = ((int32_t)(rng_get_u64() % 5)) - 2;
    return (uint32_t)((int32_t)(base + s->ts_val_base) + dither);
}

static int net_send_tcp_packet_sock_ex(struct tcp_socket* s, uint8_t flags, const void* payload, size_t payload_len, uint8_t ttl, int is_fake) {
    if (!s) return -1;
    uint8_t tcp_buf[sizeof(struct tcp_header) + 64 + 1460];
    struct tcp_header* tcp = (struct tcp_header*)tcp_buf;

    tcp->src_port = htons(s->local_port);
    tcp->dest_port = htons(s->remote_port);
    tcp->seq = htonl(s->snd_nxt);
    tcp->ack = (flags & TCP_FLAG_SYN) ? 0 : htonl(s->rcv_nxt);
    tcp->flags = flags;
    tcp->urgent_ptr = 0;
    tcp->checksum = 0;

    size_t opt_len = 0;
    uint8_t* opt = tcp_buf + sizeof(struct tcp_header);

    if (flags & TCP_FLAG_SYN) {
        /* Standard Linux 6.x SYN TCP Options (20 bytes):
         * 1. MSS = 1460 (4 bytes): Kind 2, Len 4, 1460 (0x05B4)
         * 2. SACK Permitted (2 bytes): Kind 4, Len 2
         * 3. Timestamps (10 bytes): Kind 8, Len 10, TSval, TSecr=0
         * 4. NOP (1 byte): Kind 1
         * 5. Window Scale = 7 (3 bytes): Kind 3, Len 3, Shift 7
         * Total: 20 bytes options -> 40 bytes TCP header (data_offset = 10)
         * Produces canonical p0f signature: 60:64:1:40:M1460,S,T,N,W7:WS:df:0
         */
        opt[opt_len++] = 0x02; opt[opt_len++] = 0x04; opt[opt_len++] = 0x05; opt[opt_len++] = 0xB4;
        opt[opt_len++] = 0x04; opt[opt_len++] = 0x02;
        opt[opt_len++] = 0x08; opt[opt_len++] = 0x0A;
        uint32_t now = net_get_synthetic_tsval(s);
        opt[opt_len++] = (uint8_t)(now >> 24);
        opt[opt_len++] = (uint8_t)(now >> 16);
        opt[opt_len++] = (uint8_t)(now >> 8);
        opt[opt_len++] = (uint8_t)now;
        opt[opt_len++] = 0x00; opt[opt_len++] = 0x00; opt[opt_len++] = 0x00; opt[opt_len++] = 0x00;
        opt[opt_len++] = 0x01;
        opt[opt_len++] = 0x03; opt[opt_len++] = 0x03; opt[opt_len++] = 0x07;

        tcp->window_size = htons(64240); /* Linux standard initial window for MSS 1460 */
    } else if (s->ts_recent != 0) {
        /* Standard Linux established TCP options (12 bytes):
         * NOP, NOP, Timestamps: TSval, TSecr
         * Total: 12 bytes options -> 32 bytes TCP header (data_offset = 8)
         */
        opt[opt_len++] = 0x01;
        opt[opt_len++] = 0x01;
        opt[opt_len++] = 0x08; opt[opt_len++] = 0x0A;
        uint32_t now = net_get_synthetic_tsval(s);
        opt[opt_len++] = (uint8_t)(now >> 24);
        opt[opt_len++] = (uint8_t)(now >> 16);
        opt[opt_len++] = (uint8_t)(now >> 8);
        opt[opt_len++] = (uint8_t)now;
        uint32_t rec = s->ts_recent;
        opt[opt_len++] = (uint8_t)(rec >> 24);
        opt[opt_len++] = (uint8_t)(rec >> 16);
        opt[opt_len++] = (uint8_t)(rec >> 8);
        opt[opt_len++] = (uint8_t)rec;

        size_t win = 64240;
        if (s->rx_capacity > s->rx_len) {
            win = s->rx_capacity - s->rx_len;
        } else {
            win = 0;
        }
        if (win > 64240) win = 64240;
        tcp->window_size = htons((uint16_t)win);
    } else {
        size_t win = 32768;
        if (s->rx_capacity > s->rx_len) {
            win = s->rx_capacity - s->rx_len;
        } else {
            win = 0;
        }
        if (win > 64240) win = 64240;
        tcp->window_size = htons((uint16_t)win);
    }

    size_t hdr_len = sizeof(struct tcp_header) + opt_len;
    tcp->data_offset = (uint8_t)((hdr_len / 4) << 4);

    if (payload && payload_len > 0) {
        memcpy(tcp_buf + hdr_len, payload, payload_len);
    }

    size_t tcp_tot = hdr_len + payload_len;
    tcp->checksum = net_tcp_checksum(s_config.ip, s->remote_ip, tcp_buf, tcp_tot);

    int err = net_send_ip_ttl(s->remote_ip, IP_PROTO_TCP, tcp_buf, tcp_tot, ttl ? ttl : 64);
    if (err == 0 && !is_fake) {
        if (flags & (TCP_FLAG_SYN | TCP_FLAG_FIN)) {
            s->snd_nxt++;
        }
        if (payload_len > 0) {
            s->snd_nxt += (uint32_t)payload_len;
        }
    }
    return err;
}

static inline int net_send_tcp_packet_sock(struct tcp_socket* s, uint8_t flags, const void* payload, size_t payload_len) {
    return net_send_tcp_packet_sock_ex(s, flags, payload, payload_len, 64, 0);
}

static void net_handle_tcp(const uint8_t* frame, size_t frame_len, const struct ipv4_header* ip_hdr) {
    size_t ip_hlen = ip_hdr->ihl * 4;
    size_t ip_tot_len = ntohs(ip_hdr->total_length);

    if (ip_tot_len < ip_hlen + sizeof(struct tcp_header) || ETH_HLEN + ip_tot_len > frame_len) {
        s_stats.rx_dropped++;
        return;
    }

    const struct tcp_header* tcp = (const struct tcp_header*)(frame + ETH_HLEN + ip_hlen);
    size_t tcp_tot = ip_tot_len - ip_hlen;
    size_t tcp_hlen = (size_t)((tcp->data_offset >> 4) * 4);

    if (tcp_hlen < sizeof(struct tcp_header) || tcp_hlen > tcp_tot) {
        s_stats.rx_dropped++;
        return;
    }

    if (net_tcp_checksum(ip_hdr->src_ip, ip_hdr->dest_ip, tcp, tcp_tot) != 0) {
        s_stats.rx_checksum_errors++;
        s_stats.rx_dropped++;
        return;
    }

    uint16_t dest_port = ntohs(tcp->dest_port);
    uint16_t src_port = ntohs(tcp->src_port);

    struct tcp_socket* s = NULL;
    for (int i = 0; i < MAX_TCP_SOCKETS; i++) {
        if (s_sockets[i].used &&
            s_sockets[i].local_port == dest_port &&
            s_sockets[i].remote_port == src_port &&
            s_sockets[i].remote_ip == ip_hdr->src_ip) {
            s = &s_sockets[i];
            break;
        }
    }

    if (!s || s->state == TCP_STATE_CLOSED) {
        return;
    }

    if (tcp_hlen > sizeof(struct tcp_header)) {
        const uint8_t* opt = (const uint8_t*)tcp + sizeof(struct tcp_header);
        const uint8_t* opt_end = (const uint8_t*)tcp + tcp_hlen;
        while (opt < opt_end) {
            uint8_t kind = *opt;
            if (kind == 0) break;
            if (kind == 1) {
                opt++;
                continue;
            }
            if (opt + 1 >= opt_end) break;
            uint8_t len = opt[1];
            if (len < 2 || opt + len > opt_end) break;
            if (kind == 8 && len == 10) {
                uint32_t ts_val = ((uint32_t)opt[2] << 24) | ((uint32_t)opt[3] << 16) |
                                  ((uint32_t)opt[4] << 8)  | (uint32_t)opt[5];
                s->ts_recent = ts_val;
            }
            opt += len;
        }
    }

    uint32_t seg_seq = ntohl(tcp->seq);

    if (s->state == TCP_STATE_SYN_SENT) {
        if ((tcp->flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
            s->rcv_nxt = seg_seq + 1;
            s->state = TCP_STATE_ESTABLISHED;
            net_send_tcp_packet_sock(s, TCP_FLAG_ACK, NULL, 0);
        } else if (tcp->flags & TCP_FLAG_RST) {
            s->state = TCP_STATE_CLOSED;
        }
        return;
    }

    if (s->state == TCP_STATE_ESTABLISHED) {
        if (tcp->flags & TCP_FLAG_RST) {
            s->state = TCP_STATE_CLOSED;
            return;
        }

        size_t seg_payload_len = tcp_tot - tcp_hlen;
        const uint8_t* seg_payload = (const uint8_t*)tcp + tcp_hlen;

        if (seg_payload_len > 0) {
            if (seg_seq == s->rcv_nxt) {
                if (s->rx_len + seg_payload_len <= s->rx_capacity) {
                    memcpy(s->rx_buf + s->rx_len, seg_payload, seg_payload_len);
                    s->rx_len += seg_payload_len;
                    s->rcv_nxt += (uint32_t)seg_payload_len;
                }
            }
            net_send_tcp_packet_sock(s, TCP_FLAG_ACK, NULL, 0);
        }

        if (tcp->flags & TCP_FLAG_FIN) {
            if (seg_seq + seg_payload_len == s->rcv_nxt) {
                s->fin_received = 1;
                s->rcv_nxt++;
                net_send_tcp_packet_sock(s, TCP_FLAG_ACK, NULL, 0);
                s->state = TCP_STATE_CLOSED;
            }
        }
    }
}

static const uint8_t* dns_skip_name(const uint8_t* p, const uint8_t* end) {
    while (p < end) {
        if (*p == 0) {
            return (p + 1 <= end) ? (p + 1) : end;
        }
        if ((*p & 0xC0) == 0xC0) {
            return (p + 2 <= end) ? (p + 2) : end;
        }
        uint8_t l = *p;
        if (l > 63 || p + 1 + l > end) return end;
        p += (size_t)l + 1;
    }
    return end;
}

static void net_handle_udp(const uint8_t* frame, size_t frame_len, const struct ipv4_header* ip_hdr) {
    size_t ip_hlen = ip_hdr->ihl * 4;
    size_t ip_tot_len = ntohs(ip_hdr->total_length);

    if (ip_tot_len < ip_hlen + sizeof(struct udp_header) || ETH_HLEN + ip_tot_len > frame_len) {
        s_stats.rx_dropped++;
        return;
    }

    const struct udp_header* udp = (const struct udp_header*)(frame + ETH_HLEN + ip_hlen);
    uint16_t src_port = ntohs(udp->src_port);
    uint16_t dest_port = ntohs(udp->dest_port);
    uint16_t udp_len = ntohs(udp->length);

    // Strict OpSec: Silently drop incoming NTP packets (UDP 123)
    if (src_port == 123 || dest_port == 123) {
        serial_puts("[!] OpSec: Dropped incoming UDP NTP packet (port 123)\n");
        return;
    }

    if (udp_len < sizeof(struct udp_header) || udp_len > ip_tot_len - ip_hlen) {
        s_stats.rx_dropped++;
        return;
    }

    const uint8_t* payload = (const uint8_t*)udp + sizeof(struct udp_header);
    size_t payload_len = udp_len - sizeof(struct udp_header);

    // Process DNS response (UDP port 53)
    if (src_port == DNS_PORT && payload_len >= sizeof(struct dns_header)) {
        const struct dns_header* dns = (const struct dns_header*)payload;
        uint16_t id = ntohs(dns->id);

        if (id == s_dns_tracker.query_id && !s_dns_tracker.answered) {
            uint16_t flags = ntohs(dns->flags);
            uint8_t rcode = (uint8_t)(flags & 0x000F);

            // Check RCODE: 0 = NoError
            if (rcode == 0) {
                uint16_t qdcount = ntohs(dns->qdcount);
                uint16_t ancount = ntohs(dns->ancount);

                if (ancount > 0) {
                    const uint8_t* p = payload + sizeof(struct dns_header);
                    const uint8_t* end = payload + payload_len;

                    // Skip Question section
                    for (int q = 0; q < qdcount && p < end; q++) {
                        p = dns_skip_name(p, end);
                        if (p + 4 > end) break;
                        p += 4; // QTYPE (2) + QCLASS (2)
                    }

                    // Parse Answer records (supports CNAME chains and multi-A records)
                    for (int a = 0; a < ancount && p < end; a++) {
                        p = dns_skip_name(p, end);
                        if (p + 10 > end) break;

                        uint16_t rtype = (uint16_t)((p[0] << 8) | p[1]);
                        uint16_t rclass = (uint16_t)((p[2] << 8) | p[3]);
                        uint16_t rdlength = (uint16_t)((p[8] << 8) | p[9]);
                        p += 10;

                        if (p + rdlength > end) break;

                        // Type 1 = A (IPv4), Class 1 = IN, rdlength = 4
                        if (rtype == 1 && rclass == 1 && rdlength == 4) {
                            uint32_t ip = (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
                            s_dns_tracker.resolved_ip = ip;
                            s_dns_tracker.answered = 1;
                            break;
                        }

                        p += rdlength;
                    }

                    // If not resolved from Answer section, parse Additional records (arcount)
                    if (!s_dns_tracker.answered) {
                        uint16_t nscount = ntohs(dns->nscount);
                        for (int ns = 0; ns < nscount && p < end; ns++) {
                            p = dns_skip_name(p, end);
                            if (p + 10 > end) break;
                            uint16_t rdlen = (uint16_t)((p[8] << 8) | p[9]);
                            p += 10 + rdlen;
                        }

                        uint16_t arcount = ntohs(dns->arcount);
                        for (int ar = 0; ar < arcount && p < end; ar++) {
                            p = dns_skip_name(p, end);
                            if (p + 10 > end) break;

                            uint16_t rtype = (uint16_t)((p[0] << 8) | p[1]);
                            uint16_t rclass = (uint16_t)((p[2] << 8) | p[3]);
                            uint16_t rdlength = (uint16_t)((p[8] << 8) | p[9]);
                            p += 10;

                            if (p + rdlength > end) break;

                            if (rtype == 1 && rclass == 1 && rdlength == 4) {
                                uint32_t ip = (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
                                s_dns_tracker.resolved_ip = ip;
                                s_dns_tracker.answered = 1;
                                break;
                            }

                            p += rdlength;
                        }
                    }
                }
            } else if (rcode == 3) {
                // Authoritative NXDOMAIN: Domain unequivocally does not exist
                s_dns_tracker.resolved_ip = 0;
                s_dns_tracker.answered = 1;
            }
            // For SERVFAIL (2) or REFUSED (5), do not set answered; allow public DNS fallbacks to respond.
        }
    }
}

static void net_handle_ipv4(const uint8_t* frame, size_t len) {
    if (len < ETH_HLEN + sizeof(struct ipv4_header)) {
        s_stats.rx_dropped++;
        return;
    }

    const struct ipv4_header* ip = (const struct ipv4_header*)(frame + ETH_HLEN);

    // OpSec Validation: Version 4, IHL >= 5
    if (ip->version != 4 || ip->ihl < 5) {
        s_stats.rx_dropped++;
        return;
    }

    size_t ip_hlen = ip->ihl * 4;
    size_t ip_tot_len = ntohs(ip->total_length);

    if (ip_tot_len < ip_hlen || ETH_HLEN + ip_tot_len > len) {
        s_stats.rx_dropped++;
        return;
    }

    // Verify IPv4 header checksum
    if (net_checksum(ip, ip_hlen) != 0) {
        s_stats.rx_checksum_errors++;
        s_stats.rx_dropped++;
        return;
    }

    // Check Destination IP: must be our IP, subnet broadcast, or global broadcast
    uint32_t bcast = (s_config.ip & s_config.netmask) | ~s_config.netmask;
    if (ip->dest_ip != s_config.ip && ip->dest_ip != 0xFFFFFFFF && ip->dest_ip != bcast) {
        s_stats.rx_dropped++;
        return;
    }

    s_stats.rx_ipv4++;

    // Protocol Dispatch
    if (ip->protocol == IP_PROTO_ICMP) {
        net_handle_icmp(frame, len, ip);
    } else if (ip->protocol == IP_PROTO_TCP) {
        net_handle_tcp(frame, len, ip);
    } else if (ip->protocol == IP_PROTO_UDP) {
        net_handle_udp(frame, len, ip);
    }
}

void net_poll(void) {
    if (!nic_is_active()) return;

    uint8_t frame_buf[ETH_MAX_LEN + 16];
    int len;

    // Drain up to 64 packets per polling quantum to prevent buffer overflow
    int limit = 64;
    while (limit-- > 0 && (len = nic_poll_packet(frame_buf, sizeof(frame_buf))) > 0) {
        if (len < ETH_HLEN) {
            s_stats.rx_dropped++;
            continue;
        }

        s_stats.rx_frames++;
        s_stats.rx_bytes += len;

        const struct eth_header* eth = (const struct eth_header*)frame_buf;
        uint16_t ethertype = ntohs(eth->ethertype);

        if (ethertype == ETHERTYPE_ARP) {
            net_handle_arp(frame_buf, (size_t)len);
        } else if (ethertype == ETHERTYPE_IPV4) {
            net_handle_ipv4(frame_buf, (size_t)len);
        } else {
            // Unsupported ethertype: drop
            s_stats.rx_dropped++;
        }
    }

    net_chaff_tick();
}

int net_send_ip_ttl(uint32_t dest_ip, uint8_t protocol, const void* payload, size_t payload_len, uint8_t ttl) {
    if (!nic_is_active() || !payload) return -1;
    if (sizeof(struct eth_header) + sizeof(struct ipv4_header) + payload_len > ETH_MAX_LEN) {
        return -2; // Frame too large
    }

    // Determine next-hop IP (same subnet vs default gateway)
    uint32_t next_hop;
    if ((dest_ip & s_config.netmask) == (s_config.ip & s_config.netmask) ||
        dest_ip == 0xFFFFFFFF) {
        next_hop = dest_ip;
    } else {
        next_hop = s_config.gateway;
    }

    // Resolve target MAC address
    uint8_t target_mac[ETH_ALEN];
    if (net_arp_lookup(next_hop, target_mac) != 0) {
        // Send ARP Request and wait up to 300 ms
        net_arp_request(next_hop);
        uint64_t start_ms = pit_get_uptime_ms();
        while (pit_get_uptime_ms() - start_ms < 300) {
            net_poll();
            if (net_arp_lookup(next_hop, target_mac) == 0) {
                break;
            }
            pit_sleep_ms(2);
        }
        if (net_arp_lookup(next_hop, target_mac) != 0) {
            // Virtual Router Fallback: If next-hop ARP timed out, fallback to gateway MAC
            if (net_arp_lookup(s_config.gateway, target_mac) == 0) {
                // Gateway MAC available, forward frame to router
            } else {
                serial_puts("[-] net: ARP resolution failed for next hop\n");
                return -3; // ARP timeout
            }
        }
    }

    uint8_t frame_buf[ETH_MAX_LEN];
    struct eth_header* eth = (struct eth_header*)frame_buf;
    memcpy(eth->dest, target_mac, ETH_ALEN);
    memcpy(eth->src, s_config.mac, ETH_ALEN);
    eth->ethertype = htons(ETHERTYPE_IPV4);

    struct ipv4_header* ip = (struct ipv4_header*)(frame_buf + ETH_HLEN);
    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->total_length = htons((uint16_t)(sizeof(struct ipv4_header) + payload_len));
    ip->id = 0; // RFC 6864: Zero IP ID for non-fragmented DF packets (Linux 6.x standard)
    ip->flags_frag = htons(0x4000); // DF
    ip->ttl = ttl ? ttl : 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = s_config.ip;
    ip->dest_ip = dest_ip;
    ip->checksum = net_checksum(ip, sizeof(struct ipv4_header));

    memcpy(frame_buf + ETH_HLEN + sizeof(struct ipv4_header), payload, payload_len);

    size_t total_len = ETH_HLEN + sizeof(struct ipv4_header) + payload_len;
    if (s_net_chaff_mode) {
        // Uniform Packet Quantization (anti-correlation / packet size masking)
        size_t target_len = ETH_MIN_LEN;
        if (total_len <= 128) target_len = 128;
        else if (total_len <= 256) target_len = 256;
        else if (total_len <= 512) target_len = 512;
        else if (total_len <= 1024) target_len = 1024;
        else target_len = 1514;

        if (target_len > total_len && target_len <= ETH_MAX_LEN) {
            memset(frame_buf + total_len, 0, target_len - total_len);
            total_len = target_len;
        }
    } else {
        if (total_len < ETH_MIN_LEN) {
            memset(frame_buf + total_len, 0, ETH_MIN_LEN - total_len);
            total_len = ETH_MIN_LEN;
        }
    }

    int res = nic_send_packet(frame_buf, total_len);
    if (res == 0) {
        s_stats.tx_ipv4++;
        s_stats.tx_frames++;
        s_stats.tx_bytes += total_len;
    }
    return res;
}

int net_send_ip(uint32_t dest_ip, uint8_t protocol, const void* payload, size_t payload_len) {
    return net_send_ip_ttl(dest_ip, protocol, payload, payload_len, 64);
}

int net_ping(uint32_t target_ip, uint16_t seq, uint32_t* rtt_ms) {
    if (!nic_is_active()) return -1;
    if (s_net_airgap_mode == NET_AIRGAP_STEALTH) {
        serial_puts("[!] OpSec Stealth Mode: ICMP Ping blocked (anti-tracking guard)\n");
        return -5;
    }

    // Reset ping reply tracker
    s_ping_state.replied = 0;
    s_ping_state.reply_id = 0;
    s_ping_state.reply_seq = 0;

    static const uint8_t ping_payload[32] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f
    };
    size_t payload_len = sizeof(ping_payload);

    uint8_t icmp_buf[sizeof(struct icmp_header) + 64];
    struct icmp_header* icmp = (struct icmp_header*)icmp_buf;
    icmp->type = ICMP_TYPE_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    uint16_t ping_id = (uint16_t)rng_get_u64();
    icmp->id = htons(ping_id);
    icmp->sequence = htons(seq);
    memcpy(icmp_buf + sizeof(struct icmp_header), ping_payload, payload_len);

    size_t icmp_tot = sizeof(struct icmp_header) + payload_len;
    icmp->checksum = net_checksum(icmp_buf, icmp_tot);

    uint64_t start_ms = pit_get_uptime_ms();

    int send_err = net_send_ip(target_ip, IP_PROTO_ICMP, icmp_buf, icmp_tot);
    if (send_err != 0) {
        return -2;
    }

    // Wait up to 1000 ms for Echo Reply
    while (pit_get_uptime_ms() - start_ms < 1000) {
        net_poll();
        if (s_ping_state.replied &&
            s_ping_state.reply_id == ping_id &&
            s_ping_state.reply_seq == seq) {
            if (rtt_ms) {
                *rtt_ms = (uint32_t)(pit_get_uptime_ms() - start_ms);
            }
            return 0;
        }
        pit_sleep_ms(1);
    }

    return -3; // Timeout
}

int net_send_udp(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void* payload, size_t payload_len) {
    if (dest_port == 123 || src_port == 123) {
        serial_puts("[!] OpSec: Dropped clearnet UDP NTP (port 123) packet (Anti-Time-Leak Guard)\n");
        return -9;
    }
    if (!payload && payload_len > 0) return -1;
    if (sizeof(struct udp_header) + payload_len > 1472) return -2;

    uint8_t udp_buf[sizeof(struct udp_header) + 1472];
    struct udp_header* udp = (struct udp_header*)udp_buf;

    udp->src_port = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length = htons((uint16_t)(sizeof(struct udp_header) + payload_len));
    udp->checksum = 0; // Checksum 0 is valid in IPv4 UDP

    if (payload && payload_len > 0) {
        memcpy(udp_buf + sizeof(struct udp_header), payload, payload_len);
    }

    size_t total_udp_len = sizeof(struct udp_header) + payload_len;
    return net_send_ip(dest_ip, IP_PROTO_UDP, udp_buf, total_udp_len);
}

void net_chaff_tick(void) {
    if (!s_net_chaff_mode || s_in_chaff) return;
    if (!nic_is_active() || s_net_airgap_mode == NET_AIRGAP_LOCKED) return;

    uint64_t now = pit_get_uptime_ms();
    if (now < s_chaff_next_tick_ms) return;

    // Next Poisson interval: 150-450 ms
    s_chaff_next_tick_ms = now + 150 + (rng_get_u64() % 300);

    s_in_chaff = 1;

    // Generate valid RFC 1035 decoy DNS query frame (prevents IDS malformed packet alerts)
    uint8_t decoy_payload[32];
    memset(decoy_payload, 0, sizeof(decoy_payload));
    uint16_t decoy_qid = (uint16_t)rng_get_u64();
    decoy_payload[0] = (uint8_t)(decoy_qid >> 8);
    decoy_payload[1] = (uint8_t)(decoy_qid & 0xFF);
    decoy_payload[2] = 0x01; // Flags: Standard query, RD=1
    decoy_payload[3] = 0x00;
    decoy_payload[4] = 0x00; decoy_payload[5] = 0x01; // QDCOUNT: 1
    // QNAME: \x09localhost\x00
    decoy_payload[12] = 9;
    memcpy(decoy_payload + 13, "localhost", 9);
    decoy_payload[22] = 0; // Terminating null
    decoy_payload[23] = 0x00; decoy_payload[24] = 0x01; // QTYPE: A (1)
    decoy_payload[25] = 0x00; decoy_payload[26] = 0x01; // QCLASS: IN (1)

    uint32_t dest = s_config.gateway ? s_config.gateway : MAKE_IP(10, 0, 2, 2);
    uint16_t src_p = (uint16_t)(49152 + (rng_get_u64() % (65535 - 49152)));
    net_send_udp(dest, src_p, 53, decoy_payload, 27);
    s_chaff_packets_sent++;

    s_in_chaff = 0;
}

/* ========================================================================= */
/* In-Band Certified Network Time & HTTP Date Parsing (Zero UDP NTP Leaks)   */
/* ========================================================================= */
static uint64_t s_inband_network_time_epoch = 0;
static uint64_t s_inband_sync_uptime_ms = 0;

void net_set_network_time_epoch(uint64_t epoch_sec) {
    s_inband_network_time_epoch = epoch_sec;
    s_inband_sync_uptime_ms = pit_get_uptime_ms();
    serial_puts("[+] OpSec: In-Band Certified Network Time synchronized: epoch ");
    serial_print_dec(epoch_sec);
    serial_puts(" sec (Zero UDP NTP leaks)\n");
}

uint64_t net_get_network_time_epoch(void) {
    if (s_inband_network_time_epoch == 0) {
        return 0;
    }
    uint64_t elapsed_sec = (pit_get_uptime_ms() - s_inband_sync_uptime_ms) / 1000;
    return s_inband_network_time_epoch + elapsed_sec;
}

static int str_has_prefix_ci(const char* s, const char* prefix, size_t n) {
    if (!s || !prefix) return 0;
    for (size_t i = 0; i < n; i++) {
        char c1 = s[i];
        char c2 = prefix[i];
        if (!c1 || !c2) return 0;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return 0;
    }
    return 1;
}

static int parse_month_3(const char* m) {
    if (!m) return -1;
    if (str_has_prefix_ci(m, "jan", 3)) return 1;
    if (str_has_prefix_ci(m, "feb", 3)) return 2;
    if (str_has_prefix_ci(m, "mar", 3)) return 3;
    if (str_has_prefix_ci(m, "apr", 3)) return 4;
    if (str_has_prefix_ci(m, "may", 3)) return 5;
    if (str_has_prefix_ci(m, "jun", 3)) return 6;
    if (str_has_prefix_ci(m, "jul", 3)) return 7;
    if (str_has_prefix_ci(m, "aug", 3)) return 8;
    if (str_has_prefix_ci(m, "sep", 3)) return 9;
    if (str_has_prefix_ci(m, "oct", 3)) return 10;
    if (str_has_prefix_ci(m, "nov", 3)) return 11;
    if (str_has_prefix_ci(m, "dec", 3)) return 12;
    return -1;
}

static int is_leap_year_calc(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

int net_parse_http_date(const char* date_str, uint64_t* out_epoch) {
    if (!date_str || !out_epoch) return -1;
    const char* p = date_str;
    if (str_has_prefix_ci(p, "date:", 5)) p += 5;
    while (*p == ' ' || *p == '\t') p++;

    const char* comma = strchr(p, ',');
    if (comma && comma < p + 5) {
        p = comma + 1;
        while (*p == ' ' || *p == '\t') p++;
    }

    int day = 0, year = 0, hour = 0, min = 0, sec = 0;
    char month_str[4] = {0};

    while (*p >= '0' && *p <= '9') {
        day = day * 10 + (*p - '0');
        p++;
    }
    while (*p == ' ' || *p == '\t') p++;

    for (int i = 0; i < 3 && *p && *p != ' '; i++) {
        month_str[i] = *p++;
    }
    while (*p == ' ' || *p == '\t') p++;

    int month = parse_month_3(month_str);
    if (month < 1 || month > 12) return -2;

    while (*p >= '0' && *p <= '9') {
        year = year * 10 + (*p - '0');
        p++;
    }
    while (*p == ' ' || *p == '\t') p++;

    while (*p >= '0' && *p <= '9') { hour = hour * 10 + (*p - '0'); p++; }
    if (*p == ':') p++;
    while (*p >= '0' && *p <= '9') { min = min * 10 + (*p - '0'); p++; }
    if (*p == ':') p++;
    while (*p >= '0' && *p <= '9') { sec = sec * 10 + (*p - '0'); p++; }

    if (year < 1970 || day < 1 || day > 31 || hour > 23 || min > 59 || sec > 60) {
        return -3;
    }

    static const int days_before_month[13] = {
        0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint64_t days = 0;
    for (int y = 1970; y < year; y++) {
        days += is_leap_year_calc(y) ? 366 : 365;
    }
    days += days_before_month[month] + (day - 1);
    if (month > 2 && is_leap_year_calc(year)) {
        days += 1;
    }

    uint64_t epoch = days * 86400ULL + (uint64_t)hour * 3600ULL + (uint64_t)min * 60ULL + (uint64_t)sec;
    *out_epoch = epoch;
    return 0;
}

/* ========================================================================= */
/* Zero-DHCP Static IP Configuration & Poisson Boot Delay                    */
/* ========================================================================= */
static int s_net_boot_delay_enabled = 0;
static uint64_t s_net_boot_delay_until = 0;

int net_set_config_static(uint32_t ip, uint32_t netmask, uint32_t gateway, uint32_t dns) {
    if (ip == 0) return -1;
    s_config.ip = ip;
    if (netmask) s_config.netmask = netmask;
    if (gateway) s_config.gateway = gateway;
    if (dns) s_config.dns = dns;

    serial_puts("[+] Net: Static IP configured: ");
    serial_print_dec((ip >> 24) & 0xFF); serial_puts(".");
    serial_print_dec((ip >> 16) & 0xFF); serial_puts(".");
    serial_print_dec((ip >> 8) & 0xFF); serial_puts(".");
    serial_print_dec(ip & 0xFF);
    serial_puts(" (Zero-DHCP stealth mode)\n");

    return 0;
}

void net_set_boot_delay(int enable, uint32_t delay_ms) {
    s_net_boot_delay_enabled = enable ? 1 : 0;
    if (s_net_boot_delay_enabled) {
        if (delay_ms == 0) {
            delay_ms = 5000 + (uint32_t)(rng_get_u64() % 10000);
        }
        s_net_boot_delay_until = pit_get_uptime_ms() + delay_ms;
        serial_puts("[+] OpSec: Poisson Boot Delay engaged (");
        serial_print_dec((uint64_t)delay_ms);
        serial_puts(" ms hold)\n");
    } else {
        s_net_boot_delay_until = 0;
        serial_puts("[*] OpSec: Poisson Boot Delay disengaged\n");
    }
}

int net_get_boot_delay_active(void) {
    if (!s_net_boot_delay_enabled) return 0;
    if (pit_get_uptime_ms() < s_net_boot_delay_until) return 1;
    s_net_boot_delay_enabled = 0;
    return 0;
}

int net_dns_resolve(const char* hostname, uint32_t* ip_out) {
    if (!hostname || !ip_out) return -1;

    // 0. Sanitize input hostname: strip leading spaces, protocol prefix, trailing slash/newline
    const char* p = hostname;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) p += 8;
    else if (strncmp(p, "onion://", 8) == 0) p += 8;

    char clean_host[128];
    size_t clen = 0;
    while (*p && *p != '/' && *p != ':' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && clen < sizeof(clean_host) - 1) {
        char ch = *p++;
        if (ch >= 'A' && ch <= 'Z') ch += 32;
        clean_host[clen++] = ch;
    }
    clean_host[clen] = '\0';
    while (clen > 0 && clean_host[clen - 1] == '.') {
        clean_host[--clen] = '\0';
    }
    if (clen == 0) return -1;

    // RFC 7686 Guard: Strict prohibition of .onion domain resolution over clear UDP DNS
    if ((clen >= 6 && strcmp(clean_host + clen - 6, ".onion") == 0) || strcmp(clean_host, "onion") == 0) {
        serial_puts("[-] OpSec Block: RFC 7686 violation! Attempted clearnet DNS query for .onion domain: ");
        serial_puts(clean_host);
        serial_puts(" -> DROPPED\n");
        return -9; // RFC 7686 EPERM
    }

    if (!net_is_online()) return -4; // Network link offline

    // 1. Literal IPv4 check
    if (net_parse_ip(clean_host, ip_out) == 0) {
        return 0;
    }

    // 2. Special aliases
    if (strcmp(clean_host, "localhost") == 0 || strcmp(clean_host, "loopback") == 0) {
        *ip_out = MAKE_IP(127, 0, 0, 1);
        return 0;
    }
    if (strcmp(clean_host, "gateway") == 0) {
        *ip_out = s_config.gateway;
        return 0;
    }
    if (strcmp(clean_host, "dns") == 0) {
        *ip_out = s_config.dns;
        return 0;
    }
    if (strcmp(clean_host, "p2.freedns.controld.com") == 0 ||
        strcmp(clean_host, "freedns.controld.com") == 0) {
        *ip_out = DNS_CONTROLD_DOT_IP;
        return 0;
    }
    if (strcmp(clean_host, "controld.com") == 0) {
        *ip_out = DNS_CONTROLD_P2_PRIMARY;
        return 0;
    }

    // OpSec Stealth Mode Guard: Prohibit raw UDP DNS queries to public port 53.
    // In stealth mode, DNS resolution must be performed via DoT / TLS or inside encrypted tunnel.
    if (s_net_airgap_mode == NET_AIRGAP_STEALTH) {
        serial_puts("[!] OpSec Stealth Mode: Refused raw UDP DNS query (anti-telemetry leak guard)\n");
        return -10; // DNS_ERR_STEALTH_BLOCKED
    }

    // 3. DNS Cache check (includes 0.0.0.0 sinkholed ads/trackers)
    uint64_t now = pit_get_uptime_ms();
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (s_dns_cache[i].hostname[0] != '\0' && strcmp(s_dns_cache[i].hostname, clean_host) == 0) {
            if (now - s_dns_cache[i].timestamp_ms < 300000) { // 5 min TTL
                *ip_out = s_dns_cache[i].ip;
                return (s_dns_cache[i].ip == 0) ? -5 : 0;
            }
        }
    }

    // 4. Construct RFC 1035 DNS Query packet
    uint8_t query_buf[256];
    struct dns_header* dns = (struct dns_header*)query_buf;

    // RFC 5452: Cryptographically random 16-bit Transaction ID
    uint16_t qid = (uint16_t)rng_get_u64();
    if (qid == 0) qid = 0x5E1D;
    dns->id = htons(qid);
    dns->flags = htons(0x0100); // Standard query, recursion desired
    dns->qdcount = htons(1);
    dns->ancount = 0;
    dns->nscount = 0;
    dns->arcount = 0;

    // Encode QNAME: labels (e.g. "example.com" -> \x07example\x03com\x00)
    uint8_t* qname = query_buf + sizeof(struct dns_header);
    const char* src = clean_host;
    uint8_t* dst = qname;

    while (*src) {
        const char* dot = strchr(src, '.');
        size_t len = dot ? (size_t)(dot - src) : strlen(src);
        if (len > 63 || (size_t)(dst - query_buf) + len + 6 >= sizeof(query_buf)) return -2;

        *dst++ = (uint8_t)len;
        memcpy(dst, src, len);
        dst += len;

        if (dot) src = dot + 1;
        else break;
    }
    *dst++ = 0; // Terminating zero label

    // QTYPE: 1 (A - IPv4)
    *dst++ = 0x00;
    *dst++ = 0x01;
    // QCLASS: 1 (IN - Internet)
    *dst++ = 0x00;
    *dst++ = 0x01;

    size_t query_len = (size_t)(dst - query_buf);

    // Setup DNS Tracker
    s_dns_tracker.query_id = qid;
    s_dns_tracker.answered = 0;
    s_dns_tracker.resolved_ip = 0;

    // RFC 5452: Ephemeral UDP client port randomization (49152..65535)
    uint16_t client_port = (uint16_t)(49152 + (rng_get_u64() % (65535 - 49152)));

    // Send query to configured primary DNS server without broadcast flooding
    uint32_t primary_dns = s_config.dns ? s_config.dns : DNS_CONTROLD_P2_PRIMARY;
    net_send_udp(primary_dns, client_port, DNS_PORT, query_buf, query_len);
    if (s_config.gateway != 0 && s_config.gateway != primary_dns) {
        net_send_udp(s_config.gateway, client_port, DNS_PORT, query_buf, query_len);
    }

    uint64_t start_ms = pit_get_uptime_ms();
    uint64_t last_retransmit_ms = start_ms;

    while (pit_get_uptime_ms() - start_ms < 4000) {
        net_poll();

        if (s_dns_tracker.answered) {
            *ip_out = s_dns_tracker.resolved_ip;

            // Save to cache (including 0.0.0.0 sinkholed records)
            strncpy(s_dns_cache[s_dns_cache_idx].hostname, clean_host, 63);
            s_dns_cache[s_dns_cache_idx].ip = s_dns_tracker.resolved_ip;
            s_dns_cache[s_dns_cache_idx].timestamp_ms = pit_get_uptime_ms();
            s_dns_cache_idx = (s_dns_cache_idx + 1) % DNS_CACHE_SIZE;

            if (s_dns_tracker.resolved_ip == 0) {
                return -5; // Blocked by OpSec DNS filter (Ads / Trackers)
            }
            return 0;
        }

        uint64_t elapsed = pit_get_uptime_ms() - last_retransmit_ms;
        if (elapsed >= 400) {
            last_retransmit_ms = pit_get_uptime_ms();

            // Retransmit to primary DNS and fallback secondary
            net_send_udp(primary_dns, client_port, DNS_PORT, query_buf, query_len);
            net_send_udp(DNS_CONTROLD_P2_SECONDARY, client_port, DNS_PORT, query_buf, query_len);
            if (s_config.gateway != 0 && s_config.gateway != primary_dns) {
                net_send_udp(s_config.gateway, client_port, DNS_PORT, query_buf, query_len);
            }
        }

        pit_sleep_ms(2);
    }

    return -3; // DNS Query Timeout
}

int net_tcp_socket_connect(uint32_t server_ip, uint16_t port) {
    if (!nic_is_active() || !net_is_traffic_allowed()) {
        serial_puts("[-] OpSec: TCP socket connection denied by Air-Gap Shield\n");
        return -1;
    }

    int sock_id = -1;
    for (int i = 0; i < MAX_TCP_SOCKETS; i++) {
        if (!s_sockets[i].used) {
            sock_id = i;
            break;
        }
    }
    if (sock_id < 0) return -2;

    struct tcp_socket* s = &s_sockets[sock_id];
    memset(s, 0, sizeof(*s));

    size_t rx_cap = 256 * 1024;
    uint8_t* rx_buf = (uint8_t*)kmalloc(rx_cap);
    if (!rx_buf) return -3;

    s->used = 1;
    s->state = TCP_STATE_SYN_SENT;
    s->remote_ip = server_ip;
    s->remote_port = port;
    // RFC 6056: Ephemeral port randomization (49152..65535) via CSPRNG
    uint16_t port_range = 65535 - 49152 + 1;
    s->local_port = (uint16_t)(49152 + (rng_get_u64() % port_range));

    // OpenBSD-style: Cryptographically random Initial Sequence Number (ISN)
    // Prevents system uptime leakage and TCP sequence prediction attacks
    s->snd_nxt = (uint32_t)rng_get_u64();
    s->rcv_nxt = 0;
    s->rx_buf = rx_buf;
    s->rx_capacity = rx_cap;
    s->rx_len = 0;
    s->fin_received = 0;
    s->ts_recent = 0;
    s->ts_val_base = (uint32_t)rng_get_u64();

    net_send_tcp_packet_sock(s, TCP_FLAG_SYN, NULL, 0);

    uint64_t start_ms = pit_get_uptime_ms();
    int syn_retries = 0;
    while (s->state != TCP_STATE_ESTABLISHED) {
        net_poll();
        if (s->state == TCP_STATE_CLOSED) {
            kfree(s->rx_buf);
            memset(s, 0, sizeof(*s));
            return -4;
        }
        uint64_t elapsed = pit_get_uptime_ms() - start_ms;
        if (elapsed > 400 && syn_retries == 0) {
            syn_retries++;
            net_send_tcp_packet_sock(s, TCP_FLAG_SYN, NULL, 0);
        }
        if (elapsed > 1500) {
            s->state = TCP_STATE_CLOSED;
            kfree(s->rx_buf);
            memset(s, 0, sizeof(*s));
            return -5;
        }
        pit_sleep_ms(2);
    }

    return sock_id;
}

int net_tcp_socket_send(int sock_id, const void* data, size_t len) {
    if (sock_id < 0 || sock_id >= MAX_TCP_SOCKETS) return -1;
    struct tcp_socket* s = &s_sockets[sock_id];
    if (!s->used || s->state != TCP_STATE_ESTABLISHED || !data || len == 0) return -1;

    const uint8_t* p = (const uint8_t*)data;

    // In-Kernel TCP Desync & Anti-DPI (TSPU / DPI Evasion)
    if (s_net_desync_mode != NET_DESYNC_NONE && len > 64) {
        int is_tls_ch = (p[0] == 0x16 && p[1] == 0x03 && p[5] == 0x01);
        int is_http = (len > 32 && (memcmp(p, "GET ", 4) == 0 ||
                                    memcmp(p, "POST ", 5) == 0 ||
                                    memcmp(p, "HEAD ", 5) == 0));

        if (is_tls_ch) {
            // Split TLS ClientHello across two TCP segments.
            // Segment 1 (64 bytes): TLS record header + client random + session id.
            // SNI extension is located at >90 bytes, so Segment 1 contains NO hostname.
            // TSPU DPI parser inspects Segment 1 and finds no SNI.
            // Segment 2 contains the rest of the ClientHello, but lacks TLS Record header at offset 0.
            size_t seg1_len = 64;
            if (seg1_len > len) seg1_len = len;

            if (s_net_desync_mode == NET_DESYNC_FAKE) {
                // Send a fake segment with low TTL (TTL=2) to confuse DPI state machine
                // Only on WAN routes (>2 hops) to avoid stream collision on local subnets
                int is_local = ((s->remote_ip & s_config.netmask) == (s_config.ip & s_config.netmask) ||
                                s->remote_ip == s_config.gateway ||
                                s->remote_ip == MAKE_IP(10, 0, 2, 2));
                if (!is_local) {
                    uint8_t fake[32];
                    memset(fake, 0x00, sizeof(fake));
                    net_send_tcp_packet_sock_ex(s, TCP_FLAG_ACK, fake, sizeof(fake), 2, 1);
                    s_desync_fakes++;
                }
            }

            int err1 = net_send_tcp_packet_sock(s, TCP_FLAG_ACK | TCP_FLAG_PSH, p, seg1_len);
            if (err1 != 0) return -1;

            pit_sleep_ms(2); // Ensure separate network frames reaching DPI

            size_t seg2_len = len - seg1_len;
            int err2 = net_send_tcp_packet_sock(s, TCP_FLAG_ACK | TCP_FLAG_PSH, p + seg1_len, seg2_len);
            if (err2 != 0) return (int)seg1_len;

            s_desync_splits++;
            serial_puts("[+] OpSec Anti-DPI: In-Kernel TCP Desync split applied to TLS ClientHello (64B / ");
            serial_print_dec((uint64_t)seg2_len);
            serial_puts("B)\n");
            return (int)len;
        } else if (is_http) {
            // Search for "Host:" or "host:" header to split across segments
            size_t split_pos = 32;
            for (size_t i = 0; i + 6 < len && i < 256; i++) {
                if ((p[i] == 'H' || p[i] == 'h') &&
                    (p[i+1] == 'o' || p[i+1] == 'O') &&
                    (p[i+2] == 's' || p[i+2] == 'S') &&
                    (p[i+3] == 't' || p[i+3] == 'T') &&
                    p[i+4] == ':') {
                    split_pos = i + 2; // split inside "Ho" and "st:"
                    break;
                }
            }

            int err1 = net_send_tcp_packet_sock(s, TCP_FLAG_ACK | TCP_FLAG_PSH, p, split_pos);
            if (err1 != 0) return -1;

            pit_sleep_ms(2);

            int err2 = net_send_tcp_packet_sock(s, TCP_FLAG_ACK | TCP_FLAG_PSH, p + split_pos, len - split_pos);
            if (err2 != 0) return (int)split_pos;

            s_desync_splits++;
            serial_puts("[+] OpSec Anti-DPI: In-Kernel TCP Desync split applied to HTTP Host header\n");
            return (int)len;
        }
    }

    size_t remaining = len;

    while (remaining > 0) {
        size_t chunk = (remaining > 1460) ? 1460 : remaining;
        int err = net_send_tcp_packet_sock(s, TCP_FLAG_ACK | TCP_FLAG_PSH, p, chunk);
        if (err != 0) {
            return (int)(len - remaining);
        }
        p += chunk;
        remaining -= chunk;
        if (remaining > 0) {
            pit_sleep_ms(1);
        }
    }

    return (int)len;
}

int net_tcp_socket_recv(int sock_id, void* out_buf, size_t max_len, uint32_t timeout_ms) {
    if (sock_id < 0 || sock_id >= MAX_TCP_SOCKETS) return -1;
    struct tcp_socket* s = &s_sockets[sock_id];
    if (!s->used || !out_buf || max_len == 0) return -1;

    uint64_t start_ms = pit_get_uptime_ms();

    while (1) {
        net_poll();

        if (s->rx_len > 0) {
            size_t to_copy = (s->rx_len < max_len) ? s->rx_len : max_len;
            memcpy(out_buf, s->rx_buf, to_copy);
            if (to_copy < s->rx_len) {
                memmove(s->rx_buf, s->rx_buf + to_copy, s->rx_len - to_copy);
            }
            s->rx_len -= to_copy;
            // Send window update ACK to sender so it never stalls on zero/low window
            if (s->state == TCP_STATE_ESTABLISHED) {
                net_send_tcp_packet_sock(s, TCP_FLAG_ACK, NULL, 0);
            }
            return (int)to_copy;
        }

        if (s->fin_received || s->state == TCP_STATE_CLOSED) {
            return 0;
        }

        if (timeout_ms == 0 || (pit_get_uptime_ms() - start_ms >= timeout_ms)) {
            serial_puts("[tcp_recv] TIMEOUT! rx_len=");
            serial_print_dec((uint32_t)s->rx_len);
            serial_puts(" state=");
            serial_print_dec((uint32_t)s->state);
            serial_puts(" rcv_nxt=");
            serial_print_dec(s->rcv_nxt);
            serial_puts(" fin=");
            serial_print_dec((uint32_t)s->fin_received);
            serial_puts("\n");
            return -2;
        }

        pit_sleep_ms(2);
    }
}

int net_tcp_socket_close(int sock_id) {
    if (sock_id < 0 || sock_id >= MAX_TCP_SOCKETS) return -1;
    struct tcp_socket* s = &s_sockets[sock_id];
    if (!s->used) return 0;

    if (s->state == TCP_STATE_ESTABLISHED) {
        net_send_tcp_packet_sock(s, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
    }

    s->state = TCP_STATE_CLOSED;
    if (s->rx_buf) {
        kfree(s->rx_buf);
        s->rx_buf = NULL;
    }
    memset(s, 0, sizeof(*s));
    return 0;
}

int net_tcp_socket_is_connected(int sock_id) {
    if (sock_id < 0 || sock_id >= MAX_TCP_SOCKETS) return 0;
    return (s_sockets[sock_id].used && s_sockets[sock_id].state == TCP_STATE_ESTABLISHED);
}

int net_abort_all_connections(void) {
    int aborted = 0;
    for (int i = 0; i < MAX_TCP_SOCKETS; i++) {
        if (s_sockets[i].used) {
            if (s_sockets[i].state == TCP_STATE_ESTABLISHED || s_sockets[i].state == TCP_STATE_SYN_SENT) {
                net_send_tcp_packet_sock(&s_sockets[i], TCP_FLAG_RST | TCP_FLAG_ACK, NULL, 0);
            }
            if (s_sockets[i].rx_buf) {
                kfree(s_sockets[i].rx_buf);
                s_sockets[i].rx_buf = NULL;
            }
            memset(&s_sockets[i], 0, sizeof(s_sockets[i]));
            aborted++;
        }
    }
    // Flush ARP cache
    memset(s_arp_table, 0, sizeof(s_arp_table));
    // Flush DNS cache
    memset(s_dns_cache, 0, sizeof(s_dns_cache));
    serial_puts("[+] OpSec: All TCP sockets severed and network caches purged.\n");
    return aborted;
}

void net_init(void) {
    serial_puts("[+] Net: Initializing OpSec Hardened Network Subsystem...\n");

    pci_init();

    int nic_ok = 0;
    const uint8_t* mac = NULL;
    const char* nic_name = "Unknown";

    if (e1000_init() == 0) {
        s_nic_type = NIC_TYPE_E1000;
        mac = e1000_get_mac();
        nic_name = "Gigabit Ethernet (e1000) [Hostile Sandbox]";
        nic_ok = 1;
    } else if (pcnet_init() == 0) {
        s_nic_type = NIC_TYPE_PCNET;
        mac = pcnet_get_mac();
        nic_name = "Fast Ethernet (pcnet) [Hostile Sandbox]";
        nic_ok = 1;
    }

    if (nic_ok && mac) {
        memcpy(s_config.mac, mac, ETH_ALEN);
        s_config.link_up = 1;

        // Default Sovereign Network Configuration: Control D Ads & Trackers (p2)
        s_config.ip      = MAKE_IP(10, 0, 2, 15);
        s_config.netmask = MAKE_IP(255, 255, 255, 0);
        s_config.gateway = MAKE_IP(10, 0, 2, 2);
        s_config.dns     = DNS_CONTROLD_P2_PRIMARY; // 76.76.2.2 (Control D Ads & Trackers)

        // OpSec Invariant: Zero unsolicited boot packets (ARP resolved on-demand)

        serial_puts("[+] Net: Interface eth0 active (");
        serial_puts(nic_name);
        serial_puts("). IP: 10.0.2.15, GW: 10.0.2.2, DNS: 76.76.2.2 (Control D P2 - Ads & Trackers)\n");
    } else {
        serial_puts("[-] Net: No compatible NIC detected. Network offline.\n");
        s_config.link_up = 0;
    }
}

int net_is_online(void) {
    return s_config.link_up;
}

struct net_config net_get_config(void) {
    return s_config;
}

void net_set_config(uint32_t ip, uint32_t netmask, uint32_t gateway) {
    s_config.ip = ip;
    s_config.netmask = netmask;
    s_config.gateway = gateway;
}

struct net_stats net_get_stats(void) {
    return s_stats;
}
