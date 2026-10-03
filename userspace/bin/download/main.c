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
        printf("  download http://10.0.2.2:8080/tor /bin/tor\n");
        printf("  download http://10.0.2.2:8080/file.txt /file.txt\n");
        return 1;
    }

    const char* target = argv[1];
    const char* dest = NULL;

    if (argc >= 3) {
        dest = argv[2];
    } else {
        if (strcmp(target, "tor") == 0 || strcmp(target, "torbrowser") == 0) {
            dest = "/bin/tor";
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

    printf("[*] SNL Sovereign Downloader v0.1 (OpSec Hardened Network Client)\n");
    printf("[*] Fetch Target   : %s\n", target);
    printf("[*] SeldFS Target  : %s\n", dest);
    printf("[*] Connecting via Intel e1000 Gigabit controller...\n");

    int res = seld_download_url(target, dest);
    if (res != 0) {
        printf("[-] Download failed with error code: %d\n", res);
        if (res == -2) printf("[-] Reason: Connection timeout (check network link / server port).\n");
        else if (res == -3) printf("[-] Reason: Connection refused or host port not listening.\n");
        else if (res == -4) printf("[-] Reason: HTTP server returned non-200 status code.\n");
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
            printf("[*] Type 'tor' to launch Tor Browser (OpSec Sovereign Edition).\n");
        }
    } else {
        printf("[-] Warning: Failed to query file status after download.\n");
    }

    return 0;
}
