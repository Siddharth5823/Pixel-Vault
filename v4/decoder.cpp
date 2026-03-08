#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <numeric>
#include <algorithm>

#include "picosha2.h"
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

CryptoMaterial deriveCryptoMaterial(const string& password) {
    CryptoMaterial material;
    vector<unsigned char> hash(picosha2::k_digest_size);
    picosha2::hash256(password.begin(), password.end(), hash.begin(), hash.end());

    for(int i = 0; i < 16; i++) material.aesKey[i] = hash[i];

    material.prngSeed = 0;
    material.prngSeed |= (hash[16] << 24);
    material.prngSeed |= (hash[17] << 16);
    material.prngSeed |= (hash[18] << 8);
    material.prngSeed |= (hash[19]);

    return material;
}

void decodeLSB(const string& stegoPath, const string& password) {
    CryptoMaterial keys = deriveCryptoMaterial(password);

    int width, height, channels;
    unsigned char* imgData = stbi_load(stegoPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) { cerr << "Error: Could not load stego image!" << endl; return; }

    uint32_t totalBytes = width * height * channels;

    // 1. Rebuild Map
    vector<int> indices(totalBytes);
    iota(indices.begin(), indices.end(), 0);
    mt19937 prng(keys.prngSeed);
    shuffle(indices.begin(), indices.end(), prng);

    int bitIdx = 0;

    // 2. Extract Length
    uint32_t payloadBytes = 0; 
    for (int i = 0; i < 32; i++) {
        int mappedIdx = indices[bitIdx];
        int bit = imgData[mappedIdx] & 1;
        payloadBytes = (payloadBytes << 1) | bit;
        bitIdx++;
    }

    uint32_t maxPossibleBytes = (totalBytes - 32) / 8;
    if (payloadBytes < 52 || payloadBytes > maxPossibleBytes) { // Min size: 16(IV) + 4(PVLT) + 32(Hash)
        cout << "\n[!] CRITICAL ERROR: Integrity check failed. Incorrect password or corrupted image." << endl;
        stbi_image_free(imgData); return;
    }

    // 3. Extract Payload
    vector<uint8_t> finalPayload;
    try {
        finalPayload.resize(payloadBytes);
    } catch (const bad_alloc& e) {
        cout << "\n[!] CRITICAL ERROR: Payload allocation failed." << endl;
        stbi_image_free(imgData); return;
    }

    for (uint32_t i = 0; i < payloadBytes; i++) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++) {
            if (bitIdx >= totalBytes) {
                cout << "\n[!] CRITICAL ERROR: Map out of bounds." << endl;
                stbi_image_free(imgData); return;
            }
            int mappedIdx = indices[bitIdx];
            int bit = imgData[mappedIdx] & 1;
            byte = (byte << 1) | bit;
            bitIdx++;
        }
        finalPayload[i] = byte;
    }
    stbi_image_free(imgData);

    // 4. Decrypt AES-CTR
    uint8_t iv[16];
    for (int i = 0; i < 16; i++) iv[i] = finalPayload[i];
    
    int cipherLen = payloadBytes - 16;
    vector<uint8_t> plaintext(finalPayload.begin() + 16, finalPayload.end());

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, keys.aesKey, iv);
    AES_CTR_xcrypt_buffer(&ctx, plaintext.data(), cipherLen); 

    // 5. Verify Magic Bytes
    string header = "";
    for (int i = 0; i < 4; i++) header += (char)plaintext[i];
    
    if (header != "PVLT") {
        cout << "\n[!] CRITICAL ERROR: Incorrect password." << endl;
        cout << "[!] Access Denied." << endl;
        return;
    }

    // 6. Extract the Checksum and Message
    vector<unsigned char> extractedHash(plaintext.begin() + 4, plaintext.begin() + 36);
    string decryptedText(plaintext.begin() + 36, plaintext.end());

    // 7. Verification: Re-hash the message to check for tampering
    vector<unsigned char> verificationHash(picosha2::k_digest_size);
    picosha2::hash256(decryptedText.begin(), decryptedText.end(), verificationHash.begin(), verificationHash.end());

    if (extractedHash != verificationHash) {
        cout << "\n[!] FATAL ALERT: The payload decrypted, but the checksums do not match." << endl;
        cout << "[!] The image has been corrupted or maliciously tampered with." << endl;
        return;
    }

    cout << "\n[SUCCESS] Integrity Verified! Checksum match." << endl;
    cout << "[SUCCESS] Decoded Message: " << decryptedText << endl;
}

int main() {
    string stegoPath = "../images/stego.png";
    string password;

    cout << "=== Pixel-Vault Decoder (v4.0 - Ultimate) ===" << endl;
    cout << "Enter Decryption Password: ";
    getline(cin, password);

    decodeLSB(stegoPath, password);
    return 0;
}