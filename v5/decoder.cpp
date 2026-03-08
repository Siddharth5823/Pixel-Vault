#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <numeric>
#include <algorithm>
#include <cstring> // For memset/memcpy

#include "picosha2.h"
#include "tinyfiledialogs.h"
extern "C" {
    #include "aes.h"
}

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

struct CryptoMaterial {
    uint8_t aesKey[16];
    unsigned int prngSeed;
};

// --- 1. MATCHING HARDENED KDF: 100,000 Rounds of SHA-256 ---
CryptoMaterial deriveHardenedMaterial(const string& password) {
    CryptoMaterial material;
    vector<unsigned char> hash(picosha2::k_digest_size);
    string salt = "PixelVault_v5_Final_Hardened_Salt"; // Must match Encoder exactly
    string stretching = password + salt;

    cout << "[*] Stretching key (100,000 rounds)..." << endl;
    for(int i = 0; i < 100000; i++) {
        picosha2::hash256(stretching.begin(), stretching.end(), hash.begin(), hash.end());
        stretching = string(hash.begin(), hash.end());
    }

    for(int i = 0; i < 16; i++) material.aesKey[i] = hash[i];
    material.prngSeed = (hash[16] << 24) | (hash[17] << 16) | (hash[18] << 8) | hash[19];
    return material;
}

// --- 2. MATCHING DETERMINISTIC SHUFFLE (Fisher-Yates) ---
void deterministicShuffle(vector<int>& vec, unsigned int seed) {
    mt19937 rng(seed);
    for (int i = vec.size() - 1; i > 0; --i) {
        uniform_int_distribution<int> dist(0, i);
        int j = dist(rng);
        swap(vec[i], vec[j]);
    }
}

// Secure Wipe: Overwrites data in RAM with zeros
void secureWipe(string& s) {
    if (!s.empty()) {
        memset(&s[0], 0, s.size());
        s.clear();
    }
}

void processDecoding(string stegoPath, string password) {
    CryptoMaterial keys = deriveHardenedMaterial(password);

    int width, height, channels;
    unsigned char* imgData = stbi_load(stegoPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) { cerr << "[!] Error: Could not load stego image." << endl; return; }

    uint32_t totalBytes = width * height * channels;
    vector<int> allIndices(totalBytes);
    iota(allIndices.begin(), allIndices.end(), 0);
    
    cout << "[*] Rebuilding universal pixel map..." << endl;
    deterministicShuffle(allIndices, keys.prngSeed);

    int bitIdx = 0;

    // 3. Extract Length (Always in the first 32 mapped pixels)
    uint32_t payloadBytes = 0; 
    for (int i = 0; i < 32; i++) {
        payloadBytes = (payloadBytes << 1) | (imgData[allIndices[bitIdx++]] & 1);
    }

    // Safety Bounds Check
    uint32_t maxPossibleBytes = (totalBytes - 32) / 8;
    if (payloadBytes < 52 || payloadBytes > maxPossibleBytes) {
        cout << "\n[!] CRITICAL ERROR: Integrity check failed. (Incorrect password or corrupted image)" << endl;
        stbi_image_free(imgData); return;
    }

    // 4. Extract Encrypted Payload
    vector<uint8_t> finalPayload(payloadBytes);
    for (uint32_t i = 0; i < payloadBytes; i++) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++) {
            byte = (byte << 1) | (imgData[allIndices[bitIdx++]] & 1);
        }
        finalPayload[i] = byte;
    }
    stbi_image_free(imgData);

    // 5. Decrypt AES-CTR
    uint8_t iv[16];
    memcpy(iv, finalPayload.data(), 16);
    
    vector<uint8_t> plaintext(finalPayload.begin() + 16, finalPayload.end());
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, keys.aesKey, iv);
    AES_CTR_xcrypt_buffer(&ctx, plaintext.data(), plaintext.size()); 

    // 6. Verification & Extraction
    string header(plaintext.begin(), plaintext.begin() + 4);
    if (header != "PVLT") {
        cout << "\n[!] ACCESS DENIED: Header mismatch. The key is incorrect." << endl;
        return;
    }

    vector<unsigned char> extractedHash(plaintext.begin() + 4, plaintext.begin() + 36);
    string decryptedText(plaintext.begin() + 36, plaintext.end());

    vector<unsigned char> verificationHash(picosha2::k_digest_size);
    picosha2::hash256(decryptedText.begin(), decryptedText.end(), verificationHash.begin(), verificationHash.end());

    if (extractedHash != verificationHash) {
        cout << "\n[!] SECURITY ALERT: Payload hash mismatch! The data has been altered." << endl;
        return;
    }

    // Final Success Output
    cout << "\n[SUCCESS] Vault Unlocked! Integrity Verified." << endl;
    cout << "[SUCCESS] Secret Message: " << decryptedText << endl;

    // 7. RAM Sanitization
    secureWipe(decryptedText);
    cout << "[*] Sensitive data scrubbed from memory." << endl;
}

int main(int argc, char* argv[]) {
    string stegoPath, password;

    // Parse CLI arguments
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-i" && i + 1 < argc) stegoPath = argv[++i];
        else if (arg == "-p" && i + 1 < argc) password = argv[++i];
    }

    cout << "=== Pixel-Vault (v5.0 - FOOLPROOF DECODER) ===" << endl;

    // GUI Hybrid Fallback
    if (stegoPath.empty()) {
        cout << "[*] Opening GUI to select stego image..." << endl;
        const char* filter[1] = {"*.png"};
        const char* res = tinyfd_openFileDialog("Select Stego Image", "", 1, filter, "PNG Image", 0);
        if (!res) { cout << "[!] No file selected. Exiting." << endl; return 1; }
        stegoPath = res;
    }

    if (password.empty()) {
        cout << "Enter Decryption Password: ";
        getline(cin, password);
    }

    processDecoding(stegoPath, password);
    return 0;
}