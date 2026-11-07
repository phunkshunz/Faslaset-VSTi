#!/usr/bin/env python3
"""MASKINHALLEN license keys for one machine.

Writes two things:
  1. a key list (plain text, one key per line) to upload to Payhip  -> NEVER commit it
  2. src/LicensPool.h with only truncated SHA-256 hashes of the keys, plus the
     shared bundle hashes copied from another machine's LicensPool.h

The hash scheme matches src/Licens.h (shared by all machines):
  sha256("MASKINHALLEN-LICENS-v1:" + normalised key)[:10]

Usage:
  python3 tools/licens_gen.py --product "MOTSTÅND" --count 2000 \
      --bundle-from ../fras-vsti/src/LicensPool.h \
      --keys-out ~/MOTSTAND-licensnycklar.txt \
      --buy-url https://payhip.com/MASKINHALLEN \
      --demo-url https://phunkshunz.github.io/maskinhallen/#motstand
"""
import argparse, hashlib, re, secrets, sys

ALPHABET = "0123456789ABCDEFGHJKMNPQRSTUVWXYZ"   # no I, L, O (Licens.h maps them to 1/1/0)
HASH_BYTES = 10

def new_key():
    raw = "".join(secrets.choice(ALPHABET) for _ in range(16))
    return "-".join(raw[i:i + 4] for i in range(0, 16, 4))

def key_hash(key):
    norm = key.replace("-", "").upper()
    return hashlib.sha256(("MASKINHALLEN-LICENS-v1:" + norm).encode("utf-8")).digest()[:HASH_BYTES]

def read_bundle(path):
    src = open(path, encoding="utf-8").read()
    n = int(re.search(r"kBundleN\s*=\s*(\d+)", src).group(1))
    body = re.search(r"kBundle\[\]\s*=\s*\{(.*?)\};", src, re.S).group(1)
    data = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body))
    if len(data) != n * HASH_BYTES:
        sys.exit(f"bundle pool has {len(data)} bytes, expected {n * HASH_BYTES}")
    return n, data

def c_array(name, data):
    rows = [", ".join(f"0x{b:02x}" for b in data[i:i + HASH_BYTES]) for i in range(0, len(data), HASH_BYTES)]
    return f"inline constexpr uint8_t {name}[] = {{\n    " + ",\n    ".join(rows) + "\n};\n"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--product", required=True)
    ap.add_argument("--count", type=int, default=2000)
    ap.add_argument("--bundle-from", required=True)
    ap.add_argument("--keys-out", required=True)
    ap.add_argument("--pool-out", default="src/LicensPool.h")
    ap.add_argument("--buy-url", required=True)
    ap.add_argument("--demo-url", required=True)
    a = ap.parse_args()

    keys = set()
    while len(keys) < a.count:
        keys.add(new_key())
    keys = sorted(keys)
    hashes = sorted(key_hash(k) for k in keys)
    if len(set(hashes)) != len(hashes):
        sys.exit("hash collision, run again")
    bundle_n, bundle = read_bundle(a.bundle_from)
    if set(hashes) & {bundle[i:i + HASH_BYTES] for i in range(0, len(bundle), HASH_BYTES)}:
        sys.exit("a new key collides with a bundle key, run again")

    with open(a.keys_out, "w", encoding="utf-8") as f:
        f.write("\n".join(keys) + "\n")

    with open(a.pool_out, "w", encoding="utf-8") as f:
        f.write("#pragma once\n// Skapad av tools/licens_gen.py. Ändra inte för hand.\n"
                "// Bara SHA-256-hashar (de 10 första byten) av nycklarna, inte nycklarna själva.\n"
                "#include <cstdint>\n\nnamespace licens\n{\n")
        f.write(f'inline constexpr const char* kProdukt = "{a.product}";\n')
        f.write(f'inline constexpr const char* kKopUrl = "{a.buy_url}";\n')
        f.write(f'inline constexpr const char* kDemoUrl = "{a.demo_url}";\n')
        f.write(f"inline constexpr int kHashBytes = {HASH_BYTES};\n")
        f.write(f"inline constexpr int kPoolN = {len(hashes)};\n")
        f.write(f"inline constexpr int kBundleN = {bundle_n};\n")
        f.write(c_array("kPool", b"".join(hashes)))
        f.write(c_array("kBundle", bundle))
        f.write("} // namespace licens\n")
    print(f"{len(keys)} keys -> {a.keys_out}\n{a.pool_out}: {len(hashes)} machine hashes + {bundle_n} bundle hashes")

if __name__ == "__main__":
    main()
