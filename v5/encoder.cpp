#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <numeric>
#include <algorithm>
#include <ctime>

#include "picosha2.h"
#include "tinyfiledialogs.h"
extern "C" {
    #include "aes.h"
}

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;

struct CryptoMaterial {
    uint8_t aesKey[16];
    unsigned int prngSeed;
};

// SHA-256 Key Derivation
CryptoMaterial deriveCryptoMaterial(const string& password) {
    CryptoMaterial material;
    vector<unsigned char> hash(picosha2::k_digest_size);
    picosha2::hash256(password.begin(), password.end(), hash.begin(), hash.end());
    for(int i = 0; i < 16; i++) material.aesKey[i] = hash[i];
    material.prngSeed = (hash[16] << 24) | (hash[17] << 16) | (hash[18] << 8) | hash[19];
    return material;
}

// v5 Deterministic Fisher-Yates (Cross-Platform)
void deterministicShuffle(vector<int>& vec, unsigned int seed) {
    mt19937 rng(seed);
    for (int i = vec.size() - 1; i > 0; --i) {
        uniform_int_distribution<int> dist(0, i);
        int j = dist(rng);
        swap(vec[i], vec[j]);
    }
}

// v5 Stealthy +/- 1 LSB Matching
void embedBitStealthy(unsigned char* imgData, int mappedIdx, int targetBit, mt19937& noiseRng) {
    int currentBit = imgData[mappedIdx] & 1;
    if (currentBit != targetBit) {
        if (imgData[mappedIdx] == 255) imgData[mappedIdx] = 254;
        else if (imgData[mappedIdx] == 0) imgData[mappedIdx] = 1;
        else imgData[mappedIdx] += (noiseRng() % 2 == 0) ? 1 : -1;
    }
}

void processEmbedding(string inputPath, string outputPath, string message, string password) {
    CryptoMaterial keys = deriveCryptoMaterial(password);
    
    vector<unsigned char> msgHash(picosha2::k_digest_size);
    picosha2::hash256(message.begin(), message.end(), msgHash.begin(), msgHash.end());

    vector<uint8_t> plaintext;
    string magic = "PVLT";
    for(char c : magic) plaintext.push_back(c);
    for(int i = 0; i < 32; i++) plaintext.push_back(msgHash[i]);
    for(char c : message) plaintext.push_back(c);

    uint8_t iv[16];
    mt19937 ivRng(time(0)); 
    for (int i = 0; i < 16; i++) iv[i] = ivRng() % 256;

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, keys.aesKey, iv);
    AES_CTR_xcrypt_buffer(&ctx, plaintext.data(), plaintext.size()); 

    vector<uint8_t> finalPayload;
    for (int i = 0; i < 16; i++) finalPayload.push_back(iv[i]);
    for (auto b : plaintext) finalPayload.push_back(b);
    
    uint32_t payloadBytes = finalPayload.size();

    int width, height, channels;
    unsigned char* imgData = stbi_load(inputPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) { cerr << "[!] Failed to load cover image." << endl; return; }

    uint32_t totalBytes = width * height * channels;
    vector<int> indices(totalBytes);
    iota(indices.begin(), indices.end(), 0);
    
    cout << "[*] Running God-Mode Fisher-Yates Scatter..." << endl;
    deterministicShuffle(indices, keys.prngSeed);

    mt19937 noiseRng(time(0));
    int bitIdx = 0;

    for (int i = 0; i < 32; i++) {
        int bit = (payloadBytes >> (31 - i)) & 1;
        embedBitStealthy(imgData, indices[bitIdx++], bit, noiseRng);
    }
    for (uint32_t i = 0; i < payloadBytes; i++) {
        for (int b = 7; b >= 0; b--) {
            int bit = (finalPayload[i] >> b) & 1;
            embedBitStealthy(imgData, indices[bitIdx++], bit, noiseRng);
        }
    }

    stbi_write_png(outputPath.c_str(), width, height, channels, imgData, width * channels);
    stbi_image_free(imgData);
    cout << "[SUCCESS] Invisible vault created at: " << outputPath << endl;
}

int main(int argc, char* argv[]) {
    string inputPath, outputPath, message, password;

    // Parse CLI arguments if provided
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-i" && i + 1 < argc) inputPath = argv[++i];
        else if (arg == "-o" && i + 1 < argc) outputPath = argv[++i];
        else if (arg == "-m" && i + 1 < argc) message = argv[++i];
        else if (arg == "-p" && i + 1 < argc) password = argv[++i];
    }

    cout << "=== Pixel-Vault (v5.0) ===" << endl;

    // --- SMART PATH SELECTION ---
    if (inputPath.empty()) {
        cout << "[*] Opening GUI to select cover image..." << endl;
        const char* filter[2] = {"*.png", "*.bmp"};
        const char* res = tinyfd_openFileDialog("Select Cover Image", "", 2, filter, "Image Files", 0);
        if (!res) { cout << "[!] No file selected. Exiting." << endl; return 1; }
        inputPath = res;
    }

    if (outputPath.empty()) {
        cout << "[*] Opening GUI to select output destination..." << endl;
        const char* filter[1] = {"*.png"};
        const char* res = tinyfd_saveFileDialog("Save Stego Image", "stego.png", 1, filter, "PNG Image");
        if (!res) { cout << "[!] No destination selected. Exiting." << endl; return 1; }
        outputPath = res;
    }

    // --- SECURE CONSOLE INPUTS ---
    if (message.empty()) {
        cout << "Enter Secret Message: ";
        getline(cin, message);
    }
    if (password.empty()) {
        cout << "Enter Encryption Password: ";
        getline(cin, password);
    }

    processEmbedding(inputPath, outputPath, message, password);
    return 0;
}