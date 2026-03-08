# Pixel-Vault: The Invisible Data Carrier

Pixel-Vault is a research-grade steganography suite designed for the secure embedding of encrypted data within image carriers. It moves beyond simple LSB replacement to implement a **hardened stegosystem** utilizing authenticated encryption, computational key stretching, and statistical noise injection.

## 🚀 Project Versions & Binaries

| Version | Description | Status | Binaries |
| :--- | :--- | :--- | :--- |
| **v5.0 (Final)** | Hardened Spatial God-Mode | **Stable** | [Encoder](./v5/encoder.exe) / [Decoder](./v5/decoder.exe) |
| **v4.0** | Adaptive Stealth & PRNG | Legacy | [Encoder](./v4/encoder.exe) / [Decoder](./v4/decoder.exe) |
| **v3.0** | Block Cipher Integration | Legacy | [Encoder](./v3/encoder.exe) / [Decoder](./v3/decoder.exe) |

---

## 🛠 Features v5.0

* **100,000-Round Key Stretching:** Utilizes a PBKDF2-style KDF with 100k iterations of SHA-256. This exponentially increases the "Work Factor," making brute-force attacks on even weak passwords mathematically infeasible.
* **Fisher-Yates Scattering:** A custom, deterministic shuffling algorithm ensures the payload is non-linearly distributed across the image carrier, breaking the link between physical pixel proximity and data sequence.
* **Statistical Noise Injection:** After payload embedding, the system fills the remaining 100% of the image map with cryptographic noise. This ensures a uniform entropy signature across the entire file.
* **LSB Matching (+/- 1):** Prevents "Values Stepping" in the image histogram, successfully bypassing Regular/Singular (RS) Steganalysis.
* **Hybrid Interface:** Professional CLI architecture that triggers native OS file dialogs via `tinyfiledialogs` when paths are not specified.
* **Authenticated Encryption:** Combines AES-128 (CTR Mode) with a SHA-256 HMAC checksum to verify data integrity before decryption.

---

## 📖 Theoretical Foundation

### 1. Computational Invisibility (PBKDF2)
Standard hashing is too fast. A modern GPU can test 100 million SHA-256 hashes per second. By looping the hash 100,000 times, we force the attacker's hardware to work 100,000 times harder per guess, effectively "stretching" a simple password into a military-grade key.

### 2. Forensic Invisibility (LSB Matching vs. Replacement)
Traditional LSB replacement creates a "Pair of Values" (PoV) artifact. Pixel-Vault v5 uses **LSB Matching (+/- 1)**, where the pixel value is randomly incremented or decremented. This preserves the natural image histogram and defeats **Regular/Singular (RS) Steganalysis**.



### 3. Statistical Deniability (Noise Padding)
A major weakness in steganography is the "Entropy Edge"—the point where high-entropy hidden data stops and low-entropy natural pixels begin. Pixel-Vault v5 eliminates this edge by padding the entire image with noise, making the existence of a message **statistically deniable**.

---

## 🛡️ The Invisibility Argument (Detection Bypass)

In a **lossless transmission** environment (e.g., sending as a binary "File" on Telegram or via local storage), Pixel-Vault v5 is designed to bypass all primary forms of interception and detection:

### 1. Visual Inspection (The Human Eye)
Human vision is generally incapable of perceiving color shifts below 2-3%. Our +/- 1 LSB matching alters the color value of a pixel by only **0.39%**. Even under high magnification, the changes are indistinguishable from the natural sensor noise present in all digital photography.

### 2. Statistical Detection (Chi-Square & Entropy Analysis)
Forensic tools look for "Entropy Discontinuity"—sharp changes in randomness within a file. By filling the entire image map with noise padding, Pixel-Vault ensures a perfectly uniform entropy signature. To an analyzer, the file appears to be a high-ISO photograph with uniform noise, leaving no signature for a detection algorithm to latch onto.

### 3. Algorithmic Interception (Histogram Analysis)
Basic steganography creates "staircase" effects in color frequency. By using **LSB Matching** (random addition/subtraction) rather than replacement, we preserve the natural curve of the image's colors. To automated scripts, the histogram remains mathematically "natural."

### 4. Brute-Force Immunity
Even if an interceptor extracts the bitstream, they are met with AES-128 CTR encrypted data. The **100,000-round KDF** creates a physical time-barrier. The time required to crack even a moderate password exceeds the operational lifespan of most secret communications.

---

## 📝 Detailed Changelog

### v5.0 — The "Hardened" Release
* **Key Stretching:** Implemented 100k-round SHA-256 KDF to secure passwords.
* **Noise Injection:** Developed full-map padding to defeat entropy-based detection.
* **Cross-Platform Fix:** Migrated from `std::shuffle` to custom Fisher-Yates to ensure identical pixel mapping across GCC, MinGW, and MSVC.
* **Security Sanitization:** Added `memset` memory scrubbing to zero-out plaintext buffers in RAM immediately after use.
* **UX:** Integrated `tinyfiledialogs.c` for a hybrid CLI/GUI workflow.

### v4.0 — The "Stealth" Release
* **Adaptive Embedding:** Introduced +/- 1 LSB matching to prevent histogram artifacts.
* **Scattered Mapping:** Implemented PRNG-based pixel selection to avoid linear discovery.
* **Cipher Upgrade:** Switched to AES-CTR mode to eliminate padding blocks.

---

## 💻 Build & Environment

**Target:** Windows 10/11 (MinGW-w64)  
**Libraries:** `picosha2`, `tinyfiledialogs`, `stb_image`, `tiny-AES-c`.

```bash
# Compile C components
gcc -c aes.c -o aes.o -I.
gcc -c tinyfiledialogs.c -o tinyfiledialogs.o -I.

# Link with C++ Core
g++ encoder.cpp aes.o tinyfiledialogs.o -o encoder.exe -I. -lole32 -lcomdlg32 -luser32
g++ decoder.cpp aes.o tinyfiledialogs.o -o decoder.exe -I. -lole32 -lcomdlg32 -luser32
