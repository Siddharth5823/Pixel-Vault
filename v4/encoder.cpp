#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <numeric>
#include <algorithm>
#include <ctime>

// --- Cryptographic Headers ---
#include "picosha2.h"
extern "C" {
    #include "aes.h"
}

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;

// Holds our securely generated materials
struct CryptoMaterial {
    uint8_t aesKey[16];
    unsigned int prngSeed;
};

// v4 SHA-256 Key Derivation Function
CryptoMaterial deriveCryptoMaterial(const string& password) {
    CryptoMaterial material;
    
    // 1. Generate 32-byte SHA-256 Hash of the password
    vector<unsigned char> hash(picosha2::k_digest_size);
    picosha2::hash256(password.begin(), password.end(), hash.begin(), hash.end());

    // 2. Extract Bytes [0-15] for the AES-128 Key
    for(int i = 0; i < 16; i++) material.aesKey[i] = hash[i];

    // 3. Extract Bytes [16-19] to construct a robust 32-bit Seed
    material.prngSeed = 0;
    material.prngSeed |= (hash[16] << 24);
    material.prngSeed |= (hash[17] << 16);
    material.prngSeed |= (hash[18] << 8);
    material.prngSeed |= (hash[19]);

    return material;
}

// v4 LSB Matching (+/- 1) to defeat RS Steganalysis
void embedBitStealthy(unsigned char* imgData, int mappedIdx, int targetBit, mt19937& noiseRng) {
    int currentBit = imgData[mappedIdx] & 1;
    if (currentBit != targetBit) {
        if (imgData[mappedIdx] == 255) {
            imgData[mappedIdx] = 254; // Prevent overflow
        } else if (imgData[mappedIdx] == 0) {
            imgData[mappedIdx] = 1;   // Prevent underflow
        } else {
            // Randomly add 1 or subtract 1 to flip the bit invisibly
            imgData[mappedIdx] += (noiseRng() % 2 == 0) ? 1 : -1;
        }
    }
}

void embedLSB(const string& inputPath, const string& outputPath, string message, const string& password) {
    CryptoMaterial keys = deriveCryptoMaterial(password);

    // 1. Generate Checksum (SHA-256 of the Secret Message)
    vector<unsigned char> msgHash(picosha2::k_digest_size);
    picosha2::hash256(message.begin(), message.end(), msgHash.begin(), msgHash.end());

    // 2. Assemble Secure Payload: [PVLT] + [32-byte Checksum] + [Message]
    vector<uint8_t> plaintext;
    string magic = "PVLT";
    for(char c : magic) plaintext.push_back(c);
    for(int i = 0; i < 32; i++) plaintext.push_back(msgHash[i]);
    for(char c : message) plaintext.push_back(c);

    int cipherLen = plaintext.size();

    // 3. Generate Random IV and Encrypt with AES-CTR
    uint8_t iv[16];
    mt19937 ivRng(time(0)); 
    for (int i = 0; i < 16; i++) iv[i] = ivRng() % 256;

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, keys.aesKey, iv);
    AES_CTR_xcrypt_buffer(&ctx, plaintext.data(), cipherLen); 

    // 4. Assemble Final Package: [IV] + [Ciphertext]
    vector<uint8_t> finalPayload;
    for (int i = 0; i < 16; i++) finalPayload.push_back(iv[i]);
    for (int i = 0; i < cipherLen; i++) finalPayload.push_back(plaintext[i]);
    
    uint32_t payloadBytes = finalPayload.size();

    // 5. Load Image
    int width, height, channels;
    unsigned char* imgData = stbi_load(inputPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) { cerr << "Error: Could not load image!" << endl; return; }

    uint32_t totalBytes = width * height * channels;
    if (totalBytes < 32 + (payloadBytes * 8)) {
        cerr << "Error: Image is too small!" << endl;
        stbi_image_free(imgData); return;
    }

    // 6. Generate PRNG Scattering Map
    cout << "[*] Hashing password and generating cryptographic pixel map..." << endl;
    vector<int> indices(totalBytes);
    iota(indices.begin(), indices.end(), 0);
    mt19937 prng(keys.prngSeed);
    shuffle(indices.begin(), indices.end(), prng);

    // A separate RNG for the noise (+/- 1) so it doesn't mess up our spatial map
    mt19937 noiseRng(time(0));
    int bitIdx = 0;

    // 7. Embed the Length Prefix securely using +/- 1
    for (int i = 0; i < 32; i++) {
        int bit = (payloadBytes >> (31 - i)) & 1;
        embedBitStealthy(imgData, indices[bitIdx], bit, noiseRng);
        bitIdx++;
    }

    // 8. Embed the Payload securely using +/- 1
    for (uint32_t i = 0; i < payloadBytes; i++) {
        for (int b = 7; b >= 0; b--) {
            int bit = (finalPayload[i] >> b) & 1;
            embedBitStealthy(imgData, indices[bitIdx], bit, noiseRng);
            bitIdx++;
        }
    }

    stbi_write_png(outputPath.c_str(), width, height, channels, imgData, width * channels);
    stbi_image_free(imgData);
    cout << "[+] Success: V4 Ultimate Payload secured and hidden in " << outputPath << endl;
}

int main() {
    string coverPath = "../images/cover.bmp";
    string stegoPath = "../images/stego.png";
    string secret, password;

    cout << "=== Pixel-Vault Encoder (v4.0 - Ultimate) ===" << endl;
    cout << "Enter Secret Message: ";
    getline(cin, secret);
    cout << "Set Encryption Password: ";
    getline(cin, password);

    embedLSB(coverPath, stegoPath, secret, password);
    return 0;
}