#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <numeric>
#include <algorithm>

extern "C" {
    #include "aes.h"
}

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

void deriveKey(string password, uint8_t* key) {
    uint8_t salt[16] = {0x50, 0x69, 0x78, 0x65, 0x6C, 0x56, 0x61, 0x75, 0x6C, 0x74, 0x32, 0x30, 0x32, 0x36, 0x21, 0x23};
    for (int i = 0; i < 16; i++) {
        if (i < password.length()) key[i] = (uint8_t)password[i] ^ salt[i];
        else key[i] = salt[i];
    }
}

unsigned int generateSeed(const string& password) {
    unsigned int seed = 0;
    for (char c : password) {
        seed = (seed * 31) + c; 
    }
    return seed;
}

void decodeLSB(const string& stegoPath, const uint8_t* key, string password) {
    int width, height, channels;
    unsigned char* imgData = stbi_load(stegoPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) { cerr << "Error: Could not load stego image!" << endl; return; }

    int totalBytes = width * height * channels;

    // --- MISSING BLOCK RESTORED: Rebuild the PRNG Scattering Map ---
    vector<int> indices(totalBytes);
    iota(indices.begin(), indices.end(), 0);
    
    mt19937 prng(generateSeed(password));
    shuffle(indices.begin(), indices.end(), prng);

    int bitIdx = 0;
    // ---------------------------------------------------------------

    // 1. Strict Type Enforcement: Use unsigned 32-bit int
    uint32_t payloadBytes = 0; 
    for (int i = 0; i < 32; i++) {
        int mappedIdx = indices[bitIdx];
        int bit = imgData[mappedIdx] & 1;
        payloadBytes = (payloadBytes << 1) | bit;
        bitIdx++;
    }

    // 2. Absolute Bounds Checking
    uint32_t maxPossibleBytes = (totalBytes - 32) / 8;

    if (payloadBytes < 16 || payloadBytes > maxPossibleBytes) {
        cout << "\n[!] CRITICAL ERROR: Integrity check failed. Incorrect password or corrupted image." << endl;
        stbi_image_free(imgData); 
        return;
    }

    // 3. Memory Allocation Guard (try-catch)
    vector<uint8_t> finalPayload;
    try {
        finalPayload.resize(payloadBytes);
    } catch (const bad_alloc& e) {
        cout << "\n[!] CRITICAL ERROR: Payload allocation failed. Incorrect password." << endl;
        stbi_image_free(imgData); 
        return;
    }

    // 4. Safe Payload Extraction with Loop Guards
    for (uint32_t i = 0; i < payloadBytes; i++) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++) {
            // Ultimate Guard Rail: Physically prevents the index out-of-bounds assertion
            if (bitIdx >= totalBytes) {
                cout << "\n[!] CRITICAL ERROR: Reached end of map unexpectedly." << endl;
                stbi_image_free(imgData); 
                return;
            }
            int mappedIdx = indices[bitIdx];
            int bit = imgData[mappedIdx] & 1;
            byte = (byte << 1) | bit;
            bitIdx++;
        }
        finalPayload[i] = byte;
    }
    stbi_image_free(imgData);

    // 5. Separate the IV and the Ciphertext
    uint8_t iv[16];
    for (int i = 0; i < 16; i++) iv[i] = finalPayload[i];
    
    int cipherLen = payloadBytes - 16;
    vector<uint8_t> ciphertext(finalPayload.begin() + 16, finalPayload.end());

    // 6. Decrypt using AES-CTR mode
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    AES_CTR_xcrypt_buffer(&ctx, ciphertext.data(), cipherLen); 

    // 7. Verify Magic Bytes
    string header = "";
    if (cipherLen >= 4) {
        for (int i = 0; i < 4; i++) header += (char)ciphertext[i];
    }

    if (header != "PVLT") {
        cout << "\n[!] CRITICAL ERROR: Incorrect password or corrupted file." << endl;
        cout << "[!] Access Denied." << endl;
        return;
    }

    // 8. Extract the final message
    string decryptedText(ciphertext.begin() + 4, ciphertext.end());
    cout << "\n[SUCCESS] Decoded Message: " << decryptedText << endl;
}

int main() {
    string stegoPath = "../images/stego.png";
    string password;

    cout << "=== Pixel-Vault Decoder (v3.0 - PRNG Scatter) ===" << endl;
    cout << "Enter Decryption Password: ";
    getline(cin, password);

    uint8_t key[16];
    deriveKey(password, key);

    decodeLSB(stegoPath, key, password);
    return 0;
}