#!/usr/bin/env python3
"""
Opsec Oracle - Source Updater / Scraper
Pulls latest documentation from anonymousplanet, privacyguides, and whos-zycher/opsec-guide.
Runs headlessly without printing text to terminal.
"""

import subprocess
import sys
from pathlib import Path

REPOSITORIES = {
    "anon-planet": "https://github.com/Anon-Planet/thgtoa.git",
    "privacyguides": "https://github.com/PrivacyGuides/privacyguides.org.git",
    "zycher": "https://github.com/whos-zycher/opsec-guide.git"
}

def sync_repositories(raw_dir: Path):
    raw_dir.mkdir(parents=True, exist_ok=True)
    for name, repo_url in REPOSITORIES.items():
        target = raw_dir / name
        if (target / ".git").exists():
            print(f"Updating {name}...")
            subprocess.run(["git", "-C", str(target), "pull", "--quiet", "--depth", "1"], check=False)
        else:
            print(f"Cloning {name}...")
            subprocess.run(["git", "clone", "--depth", "1", "--quiet", repo_url, str(target)], check=True)
    print("Documentation sources synchronized successfully.")

if __name__ == "__main__":
    base_dir = Path(__file__).resolve().parent
    raw_dir = base_dir / "data" / "raw"
    sync_repositories(raw_dir)
