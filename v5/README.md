# Pixel-Vault: The Invisible Data Carrier

Pixel-Vault is a research-grade steganography suite designed for the secure embedding of encrypted data within image carriers. Moving beyond simple LSB replacement, Pixel-Vault utilizes authenticated encryption, computational key stretching, and statistical noise injection to ensure hidden data remains mathematically uncrackable and statistically undetectable.

## 🚀 Project Versions & Binaries

| Version | Description | Binaries |
| :--- | :--- | :--- |
| **v5.0 (Current)** | Hardened Spatial God-Mode | [Encoder](./v5/encoder.exe) / [Decoder](./v5/decoder.exe) |
| **v4.0** | AES-CTR + PRNG Scattering | [Encoder](./v4/encoder.exe) / [Decoder](./v4/decoder.exe) |
| **v3.0** | Basic LSB + AES-ECB | [Encoder](./v3/encoder.exe) / [Decoder](./v3/decoder.exe) |

---

## 🛠 Features (v5.0 God-Mode)

* **100,000-Round Key Stretching:** Implements a PBKDF2-style Key Derivation Function with 100k iterations of SHA-256 to protect against GPU-accelerated brute-force attacks.
* **Fisher-Yates Scattering:** A custom, deterministic shuffling algorithm ensures bits are scattered across the entire image carrier, maintaining 100% cross-platform compatibility.
* **Statistical Noise Injection:** Fills the entire carrier map with randomized noise padding to maintain a uniform entropy signature, defeating Chi-Square analysis.
* **LSB Matching (+/- 1):** Prevents "Values Stepping" in the image histogram, successfully bypassing Regular/Singular (RS) Steganalysis.
* **Hybrid Interface:** Professional CLI architecture that triggers native OS file dialogs via `tinyfiledialogs` when paths are not specified.
* **Authenticated Encryption:** Combines AES-128 (CTR Mode) with a SHA-256 HMAC checksum to verify data integrity before decryption.

---

## 📊 Metrics Table

| Metric | v1.0 - v3.0 | v4.0 | v5.0 (Hardened) |
| :--- | :--- | :--- | :--- |
| **Cipher Mode** | AES-ECB | AES-CTR | AES-CTR + HMAC |
| **Key Derivation** | Direct XOR | Simple Hash | 100,000x SHA-256 |
| **Data Mapping** | Linear | PRNG Shuffle | Fisher-Yates (Deterministic) |
| **Steganalysis Resistance**| Low | Moderate | High (Noise Injection) |
| **Memory Security** | None | None | **Secure RAM Scrubbing** |
| **Interface** | Interactive Console | Interactive Console | **Hybrid CLI/GUI Dialogs** |

---

## 📝 Changelog

### v5.0 — The "Hardened" Release
- **Added:** Iterative key stretching (100k rounds) to mathematically secure weak passwords.
- **Added:** Full-image noise padding to achieve statistical uniformity.
- **Added:** Native Windows/Linux file explorer integration for enhanced UX.
- **Fixed:** Replaced `std::shuffle` with custom Fisher-Yates to fix cross-compiler map desync.
- **Security:** Implemented `memset` memory scrubbing to wipe plaintext from RAM immediately after use.

### v4.0 — The "Stealth" Release
- **Added:** SHA-256 hashing for AES key and PRNG seed derivation.
- **Added:** LSB Matching (+/- 1) to prevent histogram artifacts.
- **Added:** PRNG pixel scattering to prevent linear discovery.

### v3.0 — The "Crypto" Release
- **Added:** Integration of AES-128 encryption.
- **Added:** `PVLT` magic byte header for password verification.

---

## 🛡️ Technical Architecture & Invisibility

Pixel-Vault v5 achieves invisibility through **The Trinity of Hardening**:

1. **Computational Invisibility:** The 100k-round KDF ensures that the cost of brute-forcing the vault exceeds the value of the data, even for high-performance computing clusters.
2. **Forensic Invisibility:** By using LSB Matching (+/- 1), the natural histogram of the image is preserved, leaving no traces for RS Steganalysis to detect.
3. **Statistical Invisibility:** Noise injection ensures that the entropy signature of the "stego" image is perfectly uniform, making the actual payload indistinguishable from the carrier's natural noise.

---

## 💻 Compilation

To build from source (requires MinGW-w64 on Windows):

```bash
# 1. Compile C components
gcc -c aes.c -o aes.o -I.
gcc -c tinyfiledialogs.c -o tinyfiledialogs.o -I.

# 2. Link with C++ Encoder/Decoder
g++ encoder.cpp aes.o tinyfiledialogs.o -o encoder.exe -I. -lole32 -lcomdlg32 -luser32
g++ decoder.cpp aes.o tinyfiledialogs.o -o decoder.exe -I. -lole32 -lcomdlg32 -luser32