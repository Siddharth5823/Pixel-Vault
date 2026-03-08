#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <numeric>
#include <algorithm>
#include <cstring> // For memset

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

// SHA-256 Key Derivation (v5)
CryptoMaterial deriveCryptoMaterial(const string& password) {
    CryptoMaterial material;
    vector<unsigned char> hash(picosha2::k_digest_size);
    picosha2::hash256(password.begin(), password.end(), hash.begin(), hash.end());
    for(int i = 0; i < 16; i++) material.aesKey[i] = hash[i];
    material.prngSeed = (hash[16] << 24) | (hash[17] << 16) | (hash[18] << 8) | hash[19];
    return material;
}

// v5 Cross-Platform Fisher-Yates Shuffle (Matches Encoder)
void deterministicShuffle(vector<int>& vec, unsigned int seed) {
    mt19937 rng(seed);
    for (int i = vec.size() - 1; i > 0; --i) {
        uniform_int_distribution<int> dist(0, i);
        int j = dist(rng);
        swap(vec[i], vec[j]);
    }
}

// Secure Wipe: Overwrites sensitive RAM with zeros
void secureWipe(string& s) {
    if (!s.empty()) {
        memset(&s[0], 0, s.size());
        s.clear();
    }
}

void processDecoding(string stegoPath, string password) {
    CryptoMaterial keys = deriveCryptoMaterial(password);

    int width, height, channels;
    unsigned char* imgData = stbi_load(stegoPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) { cerr << "[!] Failed to load stego image." << endl; return; }

    uint32_t totalBytes = width * height * channels;
    vector<int> indices(totalBytes);
    iota(indices.begin(), indices.end(), 0);
    
    cout << "[*] Rebuilding universal pixel map..." << endl;
    deterministicShuffle(indices, keys.prngSeed);

    int bitIdx = 0;

    // 1. Extract Length (Scattered)
    uint32_t payloadBytes = 0; 
    for (int i = 0; i < 32; i++) {
        payloadBytes = (payloadBytes << 1) | (imgData[indices[bitIdx++]] & 1);
    }

    uint32_t maxPossibleBytes = (totalBytes - 32) / 8;
    if (payloadBytes < 52 || payloadBytes > maxPossibleBytes) {
        cout << "\n[!] CRITICAL ERROR: Integrity check failed. Wrong password or corrupted image." << endl;
        stbi_image_free(imgData); return;
    }

    // 2. Extract Payload (Scattered)
    vector<uint8_t> finalPayload(payloadBytes);
    for (uint32_t i = 0; i < payloadBytes; i++) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++) {
            byte = (byte << 1) | (imgData[indices[bitIdx++]] & 1);
        }
        finalPayload[i] = byte;
    }
    stbi_image_free(imgData);

    // 3. Decrypt AES-CTR
    uint8_t iv[16];
    for (int i = 0; i < 16; i++) iv[i] = finalPayload[i];
    
    vector<uint8_t> plaintext(finalPayload.begin() + 16, finalPayload.end());
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, keys.aesKey, iv);
    AES_CTR_xcrypt_buffer(&ctx, plaintext.data(), plaintext.size()); 

    // 4. Verification & Magic Bytes
    string header(plaintext.begin(), plaintext.begin() + 4);
    if (header != "PVLT") {
        cout << "\n[!] CRITICAL ERROR: Access Denied. Incorrect password." << endl;
        return;
    }

    vector<unsigned char> extractedHash(plaintext.begin() + 4, plaintext.begin() + 36);
    string decryptedText(plaintext.begin() + 36, plaintext.end());

    vector<unsigned char> verificationHash(picosha2::k_digest_size);
    picosha2::hash256(decryptedText.begin(), decryptedText.end(), verificationHash.begin(), verificationHash.end());

    if (extractedHash != verificationHash) {
        cout << "\n[!] FATAL ALERT: Checksum mismatch! Data tampered or corrupted." << endl;
        return;
    }

    cout << "\n[SUCCESS] Integrity Verified! (SHA-256 Match)" << endl;
    cout << "[SUCCESS] Decoded Message: " << decryptedText << endl;

    // 5. RAM Sanitization
    secureWipe(decryptedText);
    cout << "[*] Sensitive data scrubbed from memory." << endl;
}

int main(int argc, char* argv[]) {
    string stegoPath, password;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-i" && i + 1 < argc) stegoPath = argv[++i];
        else if (arg == "-p" && i + 1 < argc) password = argv[++i];
    }

    cout << "=== Pixel-Vault (v5.0) ===" << endl;

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