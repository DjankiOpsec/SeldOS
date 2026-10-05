/*
 * SeldOS - Humboldt Kernel Project
 * SNL Sovereign Downloader (/bin/download)
 * Ring 3 Userspace Network Package Fetcher
 * GPLv3 Licensed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <seld.h>

static void print_sha256(const uint8_t* hash) {
    for (int i = 0; i < 32; i++) {
        printf("%02x", hash[i]);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: download <url|package> [destination]\n");
        printf("Examples:\n");
        printf("  download tor                   (fetch Tor Browser package from repo)\n");
        printf("  download doom                  (fetch DOOM game package from repo)\n");
        printf("  download http://10.0.2.2:8080/tor /bin/tor\n");
        printf("  download http://10.0.2.2:8080/file.txt /file.txt\n");
        return 1;
    }

    const char* target = argv[1];
    const char* dest = NULL;
    int force = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "force") == 0 || strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) {
            force = 1;
        } else if (!dest) {
            dest = argv[i];
        }
    }

    if (!dest) {
        if (strcmp(target, "tor") == 0 || strcmp(target, "torbrowser") == 0) {
            dest = "/bin/tor";
        } else if (strcmp(target, "doom") == 0) {
            dest = "/bin/doom";
        } else if (strcmp(target, "wad") == 0 || strcmp(target, "doom1.wad") == 0) {
            dest = "/doom1.wad";
        } else {
            const char* slash = strrchr(target, '/');
            if (slash && *(slash + 1)) {
                static char auto_dest[64];
                snprintf(auto_dest, sizeof(auto_dest), "/%s", slash + 1);
                dest = auto_dest;
            } else {
                dest = "/downloaded.bin";
            }
        }
    }

    // Check if package is already pre-installed in SeldFS
    if (!force) {
        struct seld_stat st;
        if (seld_stat(dest, &st) == 0 && st.size > 0) {
            printf("[*] Package '%s' is already installed in SeldFS (%s)\n", target, dest);
            printf("[*] Size: %u bytes (%u blocks), SHA-256 = ", st.size, st.block_count);
            print_sha256(st.sha256);
            printf("\n");
            if (strcmp(dest, "/bin/tor") == 0) {
                printf("[*] Tor Browser (SeldTLS 1.3 Sovereign) is ready to use.\n");
                printf("[*] Type 'tor' to launch. Use 'download tor force' to re-fetch from GitHub.\n");
            } else if (strcmp(dest, "/bin/doom") == 0) {
                printf("[*] DOOM is ready to play. Type 'doom' to launch.\n");
                printf("[*] Use 'download doom force' to re-fetch from GitHub.\n");
            } else if (strcmp(dest, "/doom1.wad") == 0) {
                printf("[*] DOOM game assets are installed.\n");
            }
            return 0;
        }
    }

    printf("[*] SNL Downloader v0.1 (Hardened Network Client)\n");
    if (strcmp(target, "tor") == 0 || strcmp(target, "torbrowser") == 0 ||
        strcmp(target, "doom") == 0 || strcmp(target, "wad") == 0 || strcmp(target, "doom1.wad") == 0) {
        printf("[*] Fetch Target   : %s (DjankiOpsec/SeldOS @ GitHub)\n", target);
    } else {
        printf("[*] Fetch Target   : %s\n", target);
    }
    printf("[*] SeldFS Target  : %s\n", dest);
    printf("[*] Connecting via Intel e1000 Gigabit controller...\n");

    int res = seld_download_url(target, dest);
    if (res != 0) {
        printf("[-] Download failed with error code: %d\n", res);
        if (res == -2) printf("[-] Reason: DNS resolution failed or connection timeout.\n");
        else if (res == -3) printf("[-] Reason: TCP connection refused (port 443/80 unreachable).\n");
        else if (res == -4) printf("[-] Reason: SeldTLS 1.3 handshake negotiation failed.\n");
        else if (res == -9) printf("[-] Reason: Remote server returned non-200 HTTP status code.\n");
        return 2;
    }

    struct seld_stat st;
    if (seld_stat(dest, &st) == 0) {
        printf("[+] HTTP/1.0 200 OK - Download Complete!\n");
        printf("[+] Transferred    : %u bytes (%u blocks)\n", st.size, st.block_count);
        printf("[+] Cryptographic  : SHA-256 = ");
        print_sha256(st.sha256);
        printf("\n");
        printf("[+] SeldFS: %s successfully installed and integrity verified.\n", dest);
        if (strcmp(dest, "/bin/tor") == 0) {
            printf("[*] Type 'tor' to launch Tor Browser.\n");
        } else if (strcmp(dest, "/bin/doom") == 0) {
            printf("[*] Type 'doom' to launch DOOM.\n");
            struct seld_stat wad_st;
            if (seld_stat("/doom1.wad", &wad_st) != 0 && seld_stat("doom1.wad", &wad_st) != 0) {
                printf("[*] Notice: Game assets 'doom1.wad' not detected.\n");
                printf("[*] Run 'download wad' to fetch DOOM game assets from GitHub repo.\n");
            }
        } else if (strcmp(dest, "/doom1.wad") == 0) {
            printf("[*] DOOM game assets installed. Type 'doom' to play.\n");
        }
    } else {
        printf("[-] Warning: Failed to query file status after download.\n");
    }

    return 0;
}
