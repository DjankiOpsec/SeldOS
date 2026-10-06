/*
 * SeldOS - Humboldt Kernel Project
 * /bin/stealth: Sovereign Anti-DPI & Stealth Network Controller
 * Controls In-Kernel TCP Desync, Air-Gap Stealth Mode, and Native VLESS Engine
 * GPLv3 Licensed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <seld.h>
#include <seld_vless.h>

static void format_ip(uint32_t ip, char* buf, size_t sz) {
    uint8_t o1 = (uint8_t)(ip & 0xFF);
    uint8_t o2 = (uint8_t)((ip >> 8) & 0xFF);
    uint8_t o3 = (uint8_t)((ip >> 16) & 0xFF);
    uint8_t o4 = (uint8_t)((ip >> 24) & 0xFF);
    snprintf(buf, sz, "%u.%u.%u.%u", o1, o2, o3, o4);
}

static void format_mac(const uint8_t* mac, char* buf, size_t sz) {
    if (!mac || !buf || sz < 18) return;
    const char hex[] = "0123456789ABCDEF";
    size_t idx = 0;
    for (int i = 0; i < 6; i++) {
        buf[idx++] = hex[(mac[i] >> 4) & 0x0F];
        buf[idx++] = hex[mac[i] & 0x0F];
        if (i < 5) buf[idx++] = ':';
    }
    buf[idx] = '\0';
}

static void print_status(void) {
    int lock_mode = seld_net_get_lock();
    int desync_mode = seld_net_get_desync();

    printf("\n=======================================================================\n");
    printf(" SeldOS Sovereign Stealth & Anti-DPI Shield Status\n");
    printf("=======================================================================\n");

    printf(" [*] Air-Gap Shield Mode : ");
    if (lock_mode == SELD_AIRGAP_STEALTH) {
        printf("STEALTH (Silent - Raw UDP DNS Blocked, No Pings)\n");
    } else if (lock_mode == SELD_AIRGAP_LOCKED) {
        printf("LOCKED (Strict Air-Gap Lockdown - All TX Dropped)\n");
    } else if (lock_mode == SELD_AIRGAP_ONDEMAND) {
        printf("ON-DEMAND (Default-Deny, Auto-Leased for Tor/Utilities)\n");
    } else {
        printf("UNLOCKED (Clearnet Open Mode)\n");
    }

    printf(" [*] In-Kernel TCP Desync: ");
    if (desync_mode == SELD_DESYNC_SPLIT) {
        printf("ACTIVE (SNI Segmentation / TLS ClientHello Split 64B)\n");
    } else if (desync_mode == SELD_DESYNC_FAKE) {
        printf("ACTIVE (Fake Packet Injection + SNI Split)\n");
    } else {
        printf("DISABLED (Direct Transmission)\n");
    }

    struct seld_net_info info;
    if (seld_net_info(&info) == 0 && info.link_up) {
        char ip_str[16], gw_str[16], mac_str[18];
        format_ip(info.ip, ip_str, sizeof(ip_str));
        format_ip(info.gateway, gw_str, sizeof(gw_str));
        format_mac(info.mac, mac_str, sizeof(mac_str));

        printf(" [*] Hardware MAC Address: %s (Ephemeral / Random Spoofed)\n", mac_str);
        printf(" [*] IP / Gateway        : %s / %s\n", ip_str, gw_str);
        printf(" [*] RFC 7686 Guard      : ACTIVE (.onion Clearnet DNS Blocked)\n");
    }
    printf("=======================================================================\n\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2 || strcmp(argv[1], "status") == 0) {
        print_status();
        return 0;
    }

    if (strcmp(argv[1], "on") == 0) {
        seld_net_lock(SELD_AIRGAP_STEALTH);
        seld_net_set_desync(SELD_DESYNC_SPLIT);
        printf("[+] Sovereign Stealth Mode ACTIVATED:\n");
        printf("    - Air-Gap Shield: STEALTH (Raw UDP port 53 DNS blocked, ICMP pings silent)\n");
        printf("    - In-Kernel TCP Desync: ACTIVE (TLS ClientHello SNI split across 2 TCP segments)\n");
        printf("    - Zero-Emission: 0 packets emitted until explicit user requests.\n");
        return 0;
    }

    if (strcmp(argv[1], "off") == 0) {
        seld_net_lock(SELD_AIRGAP_ONDEMAND);
        printf("[*] Stealth Mode deactivated (Restored to default ON-DEMAND mode).\n");
        return 0;
    }

    if (strcmp(argv[1], "desync") == 0) {
        if (argc < 3) {
            printf("Usage: stealth desync <split | fake | off>\n");
            return 1;
        }
        if (strcmp(argv[2], "split") == 0) {
            seld_net_set_desync(SELD_DESYNC_SPLIT);
            printf("[+] TCP Desync mode set to: SNI SPLIT (TLS ClientHello fragmented at 64B).\n");
        } else if (strcmp(argv[2], "fake") == 0) {
            seld_net_set_desync(SELD_DESYNC_FAKE);
            printf("[+] TCP Desync mode set to: FAKE + SPLIT (Injected fake segment + SNI split).\n");
        } else if (strcmp(argv[2], "off") == 0) {
            seld_net_set_desync(SELD_DESYNC_NONE);
            printf("[*] TCP Desync disabled.\n");
        } else {
            printf("[-] Unknown desync mode: %s\n", argv[2]);
            return 1;
        }
        return 0;
    }

    if (strcmp(argv[1], "vless") == 0) {
        // Usage: stealth vless <server_ip> <port> <uuid> <target_host> <target_port> [sni]
        if (argc < 7) {
            printf("Usage: stealth vless <server_ip> <port> <uuid_hex> <target_host> <target_port> [sni]\n");
            printf("Example: stealth vless 10.0.2.2 443 27848739-7e62-4138-9fd3-098a63964b6b example.com 80 dl.google.com\n");
            return 1;
        }

        uint32_t server_ip = 0;
        if (seld_dns_resolve(argv[2], &server_ip) != 0 || server_ip == 0) {
            printf("[-] Failed to resolve server IP: %s\n", argv[2]);
            return 1;
        }

        uint16_t server_port = (uint16_t)atoi(argv[3]);
        uint8_t uuid[16];
        if (seld_vless_parse_uuid(argv[4], uuid) != 0) {
            printf("[-] Invalid UUID format: %s (expected 32 hex chars with hyphens)\n", argv[4]);
            return 1;
        }

        const char* target_host = argv[5];
        uint16_t target_port = (uint16_t)atoi(argv[6]);
        const char* sni = (argc >= 8) ? argv[7] : target_host;

        printf("[*] Connecting to VLESS endpoint via Sovereign C99 Engine...\n");
        printf("    Server : %s:%u\n", argv[2], server_port);
        printf("    Target : %s:%u\n", target_host, target_port);
        printf("    SNI    : %s\n", sni);

        seld_net_lease_acquire();
        struct seld_vless_conn conn;
        int err = seld_vless_connect(&conn, server_ip, server_port, uuid, target_host, target_port, sni, 1);
        if (err != 0) {
            printf("[-] VLESS connection failed (error %d)\n", err);
            seld_net_lease_release();
            return 1;
        }

        printf("[+] VLESS Tunnel ESTABLISHED! Sending HTTP probe request...\n");
        char req[256];
        snprintf(req, sizeof(req), "GET / HTTP/1.1\r\nHost: %s\r\nUser-Agent: SeldOS-Stealth/1.0\r\nConnection: close\r\n\r\n", target_host);

        seld_vless_send(&conn, req, strlen(req));

        char resp[512];
        int rlen = seld_vless_recv(&conn, resp, sizeof(resp) - 1, 3000);
        if (rlen > 0) {
            resp[rlen] = '\0';
            printf("[+] Received %d bytes from destination:\n", rlen);
            // Print first line of response
            char* nl = strchr(resp, '\r');
            if (nl) *nl = '\0';
            printf("    %s\n", resp);
        } else {
            printf("[*] No immediate response (timeout / empty payload)\n");
        }

        seld_vless_close(&conn);
        seld_net_lease_release();
        printf("[+] VLESS session closed cleanly.\n");
        return 0;
    }

    printf("Usage: stealth <status | on | off | desync | vless>\n");
    return 1;
}
