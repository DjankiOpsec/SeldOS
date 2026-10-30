#!/usr/bin/env python3
"""
SeldOS - Humboldt Kernel Project
CCTV Surveillance Matrix Binary Builder (scripts/build_cctv_bin.py)

Compiles a standalone, offline, sovereign binary package (cctv.bin)
containing:
1. Fast offline world cities & postal geocoding index (zero network leaks).
2. Global surveillance & traffic camera index (115,000+ public feeds).
3. Compact string pool for camera identifiers.

GNU AGPLv3 Licensed - Humboldt Underground.
"""

import sys
import os
import json
import struct
import math

MAGIC = 0x53435456  # 'SCTV'
VERSION = 1

# Curated global cities and postal centroids across all continents
WORLD_CITIES = [
    # Top Global Metropolises
    ("tokyo", "JP", 35.6762, 139.6503),
    ("new york", "US", 40.7128, -74.0060),
    ("newyork", "US", 40.7128, -74.0060),
    ("nyc", "US", 40.7128, -74.0060),
    ("london", "GB", 51.5074, -0.1278),
    ("paris", "FR", 48.8566, 2.3522),
    ("berlin", "DE", 52.5200, 13.4050),
    ("sydney", "AU", -33.8688, 151.2093),
    ("melbourne", "AU", -37.8136, 144.9631),
    ("brisbane", "AU", -27.4698, 153.0251),
    ("rome", "IT", 41.9028, 12.4964),
    ("roma", "IT", 41.9028, 12.4964),
    ("milan", "IT", 45.4642, 9.1900),
    ("madrid", "ES", 40.4168, -3.7038),
    ("barcelona", "ES", 41.3851, 2.1734),
    ("toronto", "CA", 43.6532, -79.3832),
    ("vancouver", "CA", 49.2827, -123.1207),
    ("montreal", "CA", 45.5017, -73.5673),
    ("calgary", "CA", 51.0447, -114.0719),
    ("ottawa", "CA", 45.4215, -75.6972),
    ("los angeles", "US", 34.0522, -118.2437),
    ("la", "US", 34.0522, -118.2437),
    ("chicago", "US", 41.8781, -87.6298),
    ("houston", "US", 29.7604, -95.3698),
    ("phoenix", "US", 33.4484, -112.0740),
    ("philadelphia", "US", 39.9526, -75.1652),
    ("san antonio", "US", 29.4241, -98.4936),
    ("san diego", "US", 32.7157, -117.1611),
    ("dallas", "US", 32.7767, -96.7970),
    ("san francisco", "US", 37.7749, -122.4194),
    ("sf", "US", 37.7749, -122.4194),
    ("austin", "US", 30.2672, -97.7431),
    ("seattle", "US", 47.6062, -122.3321),
    ("denver", "US", 39.7392, -104.9903),
    ("boston", "US", 42.3601, -71.0589),
    ("miami", "US", 25.7617, -80.1918),
    ("las vegas", "US", 36.1699, -115.1398),
    ("vegas", "US", 36.1699, -115.1398),
    ("washington", "US", 38.9072, -77.0369),
    ("dc", "US", 38.9072, -77.0369),
    ("atlanta", "US", 33.7490, -84.3880),
    ("detroit", "US", 42.3314, -83.0458),
    ("dubai", "AE", 25.2048, 55.2708),
    ("abu dhabi", "AE", 24.4539, 54.3773),
    ("singapore", "SG", 1.3521, 103.8198),
    ("hong kong", "HK", 22.3193, 114.1694),
    ("hk", "HK", 22.3193, 114.1694),
    ("seoul", "KR", 37.5665, 126.9780),
    ("taipei", "TW", 25.0330, 121.5654),
    ("beijing", "CN", 39.9042, 116.4074),
    ("shanghai", "CN", 31.2304, 121.4737),
    ("guangzhou", "CN", 23.1291, 113.2644),
    ("shenzhen", "CN", 22.5431, 114.0579),
    ("bangkok", "TH", 13.7563, 100.5018),
    ("kuala lumpur", "MY", 3.1390, 101.6869),
    ("kl", "MY", 3.1390, 101.6869),
    ("jakarta", "ID", -6.2088, 106.8456),
    ("manila", "PH", 14.5995, 120.9842),
    ("hanoi", "VN", 21.0285, 105.8542),
    ("ho chi minh", "VN", 10.8231, 106.6297),
    ("delhi", "IN", 28.6139, 77.2090),
    ("new delhi", "IN", 28.6139, 77.2090),
    ("mumbai", "IN", 19.0760, 72.8777),
    ("bengaluru", "IN", 12.9716, 77.5946),
    ("bangalore", "IN", 12.9716, 77.5946),
    ("chennai", "IN", 13.0827, 80.2707),
    ("kolkata", "IN", 22.5726, 88.3639),
    ("hyderabad", "IN", 17.3850, 78.4867),
    ("cairo", "EG", 30.0444, 31.2357),
    ("johannesburg", "ZA", -26.2041, 28.0473),
    ("cape town", "ZA", -33.9249, 18.4241),
    ("nairobi", "KE", -1.2921, 36.8219),
    ("lagos", "NG", 6.5244, 3.3792),
    ("casablanca", "MA", 33.5731, -7.5898),
    ("buenos aires", "AR", -34.6037, -58.3816),
    ("sao paulo", "BR", -23.5505, -46.6333),
    ("rio de janeiro", "BR", -22.9068, -43.1729),
    ("rio", "BR", -22.9068, -43.1729),
    ("bogota", "CO", 4.7110, -74.0721),
    ("lima", "PE", -12.0464, -77.0428),
    ("santiago", "CL", -33.4489, -70.6693),
    ("mexico city", "MX", 19.4326, -99.1332),
    ("cdmx", "MX", 19.4326, -99.1332),
    ("guadalajara", "MX", 20.6597, -103.3496),
    ("monterrey", "MX", 25.6866, -100.3161),
    ("amsterdam", "NL", 52.3676, 4.9041),
    ("brussels", "BE", 50.8503, 4.3517),
    ("vienna", "AT", 48.2082, 16.3738),
    ("prague", "CZ", 50.0755, 14.4378),
    ("warsaw", "PL", 52.2297, 21.0122),
    ("budapest", "HU", 47.4979, 19.0402),
    ("stockholm", "SE", 59.3293, 18.0686),
    ("oslo", "NO", 59.9139, 10.7522),
    ("helsinki", "FI", 60.1699, 24.9384),
    ("copenhagen", "DK", 55.6761, 12.5683),
    ("tallinn", "EE", 59.4372, 24.7573),
    ("estonia", "EE", 59.4372, 24.7573),
    ("eesti", "EE", 59.4372, 24.7573),
    ("riga", "LV", 56.9496, 24.1052),
    ("latvia", "LV", 56.9496, 24.1052),
    ("vilnius", "LT", 54.6872, 25.2797),
    ("lithuania", "LT", 54.6872, 25.2797),
    ("dublin", "IE", 53.3498, -6.2603),
    ("athens", "GR", 37.9838, 23.7275),
    ("lisbon", "PT", 38.7223, -9.1393),
    ("zurich", "CH", 47.3769, 8.5417),
    ("geneva", "CH", 46.2044, 6.1432),
    ("frankfurt", "DE", 50.1109, 8.6821),
    ("munich", "DE", 48.1351, 11.5820),
    ("hamburg", "DE", 53.5511, 9.9937),
    ("istanbul", "TR", 41.0082, 28.9784),
    ("ankara", "TR", 39.9334, 32.8597),
    ("riyadh", "SA", 24.7136, 46.6753),
    ("tel aviv", "IL", 32.0853, 34.7818),
    ("jerusalem", "IL", 31.7683, 35.2137),
    ("doha", "QA", 25.2854, 51.5310),

    # Russian Regional Centers & Postal Centroids
    ("dzerzhinsk", "RU", 56.2457, 43.4883),
    ("606000", "RU", 56.2457, 43.4883),
    ("moscow", "RU", 55.7558, 37.6173),
    ("msk", "RU", 55.7558, 37.6173),
    ("101000", "RU", 55.7558, 37.6173),
    ("saint petersburg", "RU", 59.9343, 30.3351),
    ("st petersburg", "RU", 59.9343, 30.3351),
    ("spb", "RU", 59.9343, 30.3351),
    ("190000", "RU", 59.9343, 30.3351),
    ("nizhny novgorod", "RU", 56.3269, 44.0059),
    ("nnovgorod", "RU", 56.3269, 44.0059),
    ("nizhny", "RU", 56.3269, 44.0059),
    ("nn", "RU", 56.3269, 44.0059),
    ("603000", "RU", 56.3269, 44.0059),
    ("novosibirsk", "RU", 55.0084, 82.9357),
    ("630000", "RU", 55.0084, 82.9357),
    ("yekaterinburg", "RU", 56.8389, 60.6057),
    ("ekaterinburg", "RU", 56.8389, 60.6057),
    ("620000", "RU", 56.8389, 60.6057),
    ("kazan", "RU", 55.8304, 49.0661),
    ("420000", "RU", 55.8304, 49.0661),
    ("samara", "RU", 53.2415, 50.2212),
    ("443000", "RU", 53.2415, 50.2212),
    ("chelyabinsk", "RU", 55.1644, 61.4368),
    ("454000", "RU", 55.1644, 61.4368),
    ("omsk", "RU", 54.9885, 73.3242),
    ("rostov", "RU", 47.2357, 39.7015),
    ("rostov-on-don", "RU", 47.2357, 39.7015),
    ("ufa", "RU", 54.7388, 55.9721),
    ("krasnoyarsk", "RU", 56.0153, 92.8932),
    ("perm", "RU", 58.0105, 56.2502),
    ("voronezh", "RU", 51.6608, 39.2003),
    ("volgograd", "RU", 48.7080, 44.5133),
    ("krasnodar", "RU", 45.0355, 38.9753),
    ("saratov", "RU", 51.5336, 46.0343),
    ("tyumen", "RU", 57.1553, 65.5619),
    ("vladivostok", "RU", 43.1198, 131.8869),
    ("khabarovsk", "RU", 48.4802, 135.0719),
    ("irkutsk", "RU", 52.2870, 104.3050),
    ("sochi", "RU", 43.6028, 39.7342),
    ("kaliningrad", "RU", 54.7104, 20.4522),
    ("murmansk", "RU", 68.9585, 33.0827),
    ("arhangelsk", "RU", 64.5472, 40.5601),
    ("vologda", "RU", 59.2239, 39.8840),
    ("yaroslavl", "RU", 57.6261, 39.8845),
    ("tula", "RU", 54.1961, 37.6182),
    ("ryazan", "RU", 54.6295, 39.7424),
    ("penza", "RU", 53.2273, 45.0000),
    ("orenburg", "RU", 51.7666, 55.1004),
    ("tomsk", "RU", 56.5010, 84.9924),
    ("kemerovo", "RU", 55.3550, 86.0872),
    ("barnaul", "RU", 53.3606, 83.7636),
    ("novokuznetsk", "RU", 53.7596, 87.1216),
    ("astrakhan", "RU", 46.3497, 48.0408),
    ("makhachkala", "RU", 42.9831, 47.5046),
    ("grozny", "RU", 43.3180, 45.6986),
    ("vladikavkaz", "RU", 43.0246, 44.6817),
    ("sevastopol", "RU", 44.6166, 33.5254),
    ("simferopol", "RU", 44.9521, 34.1024),

    # Notable Postal Codes
    ("90210", "US", 34.0736, -118.4004),
    ("10001", "US", 40.7501, -73.9961),
    ("33101", "US", 25.7743, -80.1937),
    ("94102", "US", 37.7813, -122.4167),
    ("98101", "US", 47.6101, -122.3344),
    ("60601", "US", 41.8860, -87.6226),
    ("75201", "US", 32.7884, -96.7997),
    ("20001", "US", 38.9108, -77.0163),
    ("02108", "US", 42.3588, -71.0638),

    # Other CIS and Neighboring Capitals
    ("kyiv", "UA", 50.4501, 30.5234),
    ("kiev", "UA", 50.4501, 30.5234),
    ("minsk", "BY", 53.9045, 27.5615),
    ("astana", "KZ", 51.1694, 71.4491),
    ("almaty", "KZ", 43.2220, 76.8512),
    ("tashkent", "UZ", 41.2995, 69.2401),
    ("baku", "AZ", 40.4093, 49.8671),
    ("tbilisi", "GE", 41.7151, 44.8271),
    ("yerevan", "AM", 40.1792, 44.4991),
    ("bishkek", "KG", 42.8746, 74.5698),
    ("dushanbe", "TJ", 38.5598, 68.7870),
    ("ashgabat", "TM", 37.9601, 58.3261),
    ("chisinau", "MD", 47.0105, 28.8638),
]

def build_cctv_matrix(source_json_path: str, output_bin_path: str):
    if not os.path.exists(source_json_path):
        print(f"[-] Source markers JSON not found: {source_json_path}")
        sys.exit(1)

    print(f"[*] Reading source camera markers from {source_json_path}...")
    with open(source_json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    lats = data["lats"]
    lngs = data["lngs"]
    fts = data["fts"]
    ids = data["ids"]

    # Filter valid cameras and live streams (0=image, 1=mjpeg, 3=hls, 5=mp4)
    # Demote iframe/webpage markers (windy, osm, airportwebcams, etc.) to ft=0 so they never hijack video slots
    # STRICTLY BAN YouTube, Vimeo, and proprietary streaming platforms
    valid_records = []
    bad_markers = (
        "vimeo", "youtube", "yt-", "sakura-", "twitch", "dailymotion",
        "openwebcamdb", "webcamtaxi", "camstreamer", "windy", "osm",
        "airportwebcams", "balticlivecam", "skaping", "roundshot"
    )
    fake_video_prefixes = ()
    for i in range(len(fts)):
        ft = int(fts[i])
        cid = ids[i].strip()
        cid_low = cid.lower()
        if not cid or any(bad in cid_low for bad in bad_markers):
            continue
        if any(cid_low.startswith(p) for p in fake_video_prefixes):
            if ft != 0:
                ft = 0  # Reclassify to snapshot so real video feeds are prioritized
        if ft in (0, 1, 3, 5):
            valid_records.append((float(lats[i]), float(lngs[i]), int(ft), cid))

    # Inject verified live municipal street cameras (Moscow / Inetcom)
    inetcom_seeds = [
        (55.751161, 37.987837, 3, "inetcom-1"),
        (55.748540, 37.703319, 3, "inetcom-2"),
        (55.737283, 37.694371, 3, "inetcom-3"),
        (55.705275, 37.763057, 3, "inetcom-4"),
        (55.702101, 37.792518, 3, "inetcom-5"),
        (55.697216, 37.789750, 3, "inetcom-6"),
        (55.692971, 37.735698, 3, "inetcom-7"),
        (55.733498, 37.671022, 3, "inetcom-8"),
        (55.731653, 37.733456, 3, "inetcom-10"),
        (55.707548, 37.768840, 3, "inetcom-11"),
        (55.500223, 37.569539, 3, "inetcom-12"),
    ]
    for lat, lon, ft, cid in inetcom_seeds:
        valid_records.append((lat, lon, ft, cid))

    # Inject verified live municipal street cameras (Dzerzhinsk & Nizhny Novgorod / Volga Region)
    volga_video_seeds = [
        (56.2415, 43.4612, 3, "dz-lenin-square"),
        (56.2231, 43.4880, 3, "dz-okskaya-embankment"),
        (56.2350, 43.4420, 3, "dz-tsiolkovsky-prospekt"),
        (56.2510, 43.4350, 3, "dz-sverdlov-avenue"),
        (56.3218, 43.9452, 3, "nn-moskovsky-railway-hub"),
        (56.3264, 43.9678, 3, "nn-kanavinsky-bridge-traffic"),
        (56.3341, 43.9723, 3, "nn-strelka-volga-confluence"),
        (56.3287, 44.0020, 3, "nn-kremlin-minin-square"),
        (56.3303, 44.0094, 3, "nn-chkalov-stairs-riverport"),
        (56.3142, 43.9928, 3, "nn-gorky-square-metro"),
    ]
    for lat, lon, ft, cid in volga_video_seeds:
        valid_records.append((lat, lon, ft, cid))

    # Inject authentic Raw IP unauthenticated open cameras (ft=1: RAW badge) and municipal infrastructure
    raw_ip_seeds = [
        # Tokyo / Japan Sector: Raw IP feeds & MLIT infrastructure
        (35.6895, 139.6917, 1, "raw-153-198-1-158"),
        (35.6580, 139.7514, 1, "raw-183-77-159-247"),
        (35.7100, 139.8107, 1, "raw-114-184-246-240"),
        (35.6800, 139.7600, 0, "mlit-tokyo-100081001"),
        (35.6600, 139.7700, 0, "mlit-tokyo-100081008"),

        # Berlin / Germany Sector: Raw IP feeds & Asfinag Autobahn
        (52.5200, 13.4050, 1, "raw-31-17-87-107"),
        (52.5160, 13.3770, 1, "raw-95-223-165-58"),
        (52.5000, 13.3500, 1, "raw-79-224-22-232"),
        (52.4800, 13.4200, 0, "asfinag-a001-010"),

        # London / UK Sector: Raw IP feeds & TfL JamCams
        (51.5074, -0.1278, 1, "raw-31-125-95-247"),
        (51.5155, -0.0922, 1, "raw-89-240-255-235"),
        (51.5080, -0.1280, 0, "tfl-jamcam-06502"),
        (51.5100, -0.1340, 0, "tfl-jamcam-07370"),
        (51.5300, -0.1000, 0, "trafficengland-20023"),

        # New York / USA Sector: Raw IP feeds & NY511 DOT HLS
        (40.7128, -74.0060, 1, "raw-208-65-183-226"),
        (40.7580, -73.9855, 1, "raw-108-52-105-4"),
        (40.7300, -73.9950, 3, "ny511-ta159"),
        (40.7400, -73.9800, 3, "ny511-ta095"),

        # Paris / France Sector: Raw IP feeds
        (48.8566, 2.3522, 1, "raw-80-13-184-18"),
        (48.8600, 2.3400, 1, "raw-81-250-228-238"),

        # Rome / Italy Sector: Raw IP feeds
        (41.9028, 12.4964, 1, "raw-93-66-40-62"),
        (41.8900, 12.5000, 1, "raw-82-85-225-70"),

        # Russia Sector: Raw IP feeds
        (55.7558, 37.6173, 1, "raw-85-172-105-181"),
        (56.3269, 44.0059, 1, "raw-109-170-96-131"),
        (55.7600, 37.6000, 1, "raw-5-149-155-88"),
    ]
    for lat, lon, ft, cid in raw_ip_seeds:
        valid_records.append((lat, lon, ft, cid))

    print(f"[+] Total live cameras selected: {len(valid_records)}")

    # Build String Pool and Camera Binary Records
    # string pool is a contiguous null-terminated UTF-8 byte array
    string_pool = bytearray()
    str_cache = {}

    cam_records = bytearray()
    CAM_STRUCT_FMT = "<ffIBB2s"  # lat (4), lon (4), id_offset (4), ft (1), flags (1), reserved (2) = 16 bytes

    for lat, lon, ft, cid in valid_records:
        if cid in str_cache:
            str_off = str_cache[cid]
        else:
            str_off = len(string_pool)
            str_bytes = cid.encode("utf-8") + b"\0"
            string_pool.extend(str_bytes)
            str_cache[cid] = str_off

        rec = struct.pack(CAM_STRUCT_FMT, lat, lon, str_off, ft, 0, b"\0\0")
        cam_records.extend(rec)

    # Build World Cities Table
    # struct sctv_city: char name[24], char country[4], float lat, float lon = 36 bytes
    CITY_STRUCT_FMT = "<24s4sff"
    city_records = bytearray()

    for name, cc, clat, clon in WORLD_CITIES:
        name_bytes = name.lower().strip().encode("utf-8")[:23]
        name_padded = name_bytes + b"\0" * (24 - len(name_bytes))
        cc_bytes = cc.upper().strip().encode("utf-8")[:3]
        cc_padded = cc_bytes + b"\0" * (4 - len(cc_bytes))
        crec = struct.pack(CITY_STRUCT_FMT, name_padded, cc_padded, clat, clon)
        city_records.extend(crec)

    # File Layout:
    # 0..31: Header (32 bytes)
    # Header:
    # magic (4), version (2), flags (2), num_cities (4), num_cameras (4),
    # cities_offset (4), cameras_offset (4), strings_offset (4), strings_size (4)
    HDR_FMT = "<IHH6I"
    hdr_size = struct.calcsize(HDR_FMT)  # 36 bytes (4+2+2+6*4 = 32? No: 4+2+2+24 = 32 bytes)

    cities_offset = hdr_size
    cities_size = len(city_records)

    cameras_offset = cities_offset + cities_size
    cameras_size = len(cam_records)

    strings_offset = cameras_offset + cameras_size
    strings_size = len(string_pool)

    header = struct.pack(
        HDR_FMT,
        MAGIC,
        VERSION,
        0,
        len(WORLD_CITIES),
        len(valid_records),
        cities_offset,
        cameras_offset,
        strings_offset,
        strings_size
    )

    total_size = len(header) + cities_size + cameras_size + strings_size
    print(f"[*] Compiling cctv.bin: Header={hdr_size}b, Cities={cities_size}b, Cameras={cameras_size}b, Strings={strings_size}b")
    print(f"[*] Total Package Size: {total_size} bytes ({total_size / 1024 / 1024:.2f} MiB)")

    with open(output_bin_path, "wb") as f:
        f.write(header)
        f.write(city_records)
        f.write(cam_records)
        f.write(string_pool)

    import hashlib
    sha = hashlib.sha256(open(output_bin_path, "rb").read()).hexdigest()
    print(f"[+] Output written to {output_bin_path}")
    print(f"[+] Cryptographic SHA-256: {sha}")

if __name__ == "__main__":
    src = "/tmp/opencctv_markers.json"
    dst = "cctv.bin"
    if len(sys.argv) > 2:
        src = sys.argv[1]
        dst = sys.argv[2]
    elif len(sys.argv) > 1:
        dst = sys.argv[1]
    build_cctv_matrix(src, dst)
