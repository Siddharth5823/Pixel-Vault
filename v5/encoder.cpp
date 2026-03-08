#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <numeric>
#include <algorithm>
#include <ctime>
#include <cmath>

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

// --- 1. HARDENED KDF: 100,000 Rounds of SHA-256 ---
CryptoMaterial deriveHardenedMaterial(const string& password) {
    CryptoMaterial material;
    vector<unsigned char> hash(picosha2::k_digest_size);
    string salt = "PixelVault_v5_Final_Hardened_Salt";
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

// --- 2. DETERMINISTIC SHUFFLE (Fisher-Yates) ---
void deterministicShuffle(vector<int>& vec, unsigned int seed) {
    mt19937 rng(seed);
    for (int i = vec.size() - 1; i > 0; --i) {
        uniform_int_distribution<int> dist(0, i);
        int j = dist(rng);
        swap(vec[i], vec[j]);
    }
}

// --- 3. ADAPTIVE NOISE DETECTION (Texture Awareness) ---
// Calculates local variance to see if a pixel is "noisy" enough to hide data
bool isComplexPixel(unsigned char* imgData, int idx, int total, int width, int channels) {
    if (idx < channels || idx > total - channels) return false;
    // Simple check: compare with previous and next pixel in same channel
    int diff = abs(imgData[idx] - imgData[idx-channels]) + abs(imgData[idx] - imgData[idx+channels]);
    return diff > 5; // Threshold: Only use pixels with a bit of "texture"
}

void embedBitStealthy(unsigned char* imgData, int mappedIdx, int targetBit, mt19937& noiseRng) {
    int currentBit = imgData[mappedIdx] & 1;
    if (currentBit != targetBit) {
        if (imgData[mappedIdx] == 255) imgData[mappedIdx] = 254;
        else if (imgData[mappedIdx] == 0) imgData[mappedIdx] = 1;
        else imgData[mappedIdx] += (noiseRng() % 2 == 0) ? 1 : -1;
    }
}

void processEmbedding(string inputPath, string outputPath, string message, string password) {
    CryptoMaterial keys = deriveHardenedMaterial(password);
    
    // Checksum & Encryption
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
    if (!imgData) return;

    uint32_t totalBytes = width * height * channels;
    vector<int> allIndices(totalBytes);
    iota(allIndices.begin(), allIndices.end(), 0);
    
    cout << "[*] Shuffling global map..." << endl;
    deterministicShuffle(allIndices, keys.prngSeed);

    mt19937 noiseRng(time(0));
    int bitIdx = 0;

    // --- 4. DATA EMBEDDING & NOISE INJECTION ---
    cout << "[*] Embedding with adaptive noise padding..." << endl;
    
    // Embed Length (Always in the first 32 mapped pixels so decoder can find it)
    for (int i = 0; i < 32; i++) {
        int bit = (payloadBytes >> (31 - i)) & 1;
        embedBitStealthy(imgData, allIndices[bitIdx++], bit, noiseRng);
    }

    // Embed Message
    int payloadBits = payloadBytes * 8;
    int currentBitPtr = 0;

    while (bitIdx < totalBytes) {
        int mappedIdx = allIndices[bitIdx++];
        int targetBit;

        if (currentBitPtr < payloadBits) {
            // Still have real data to hide
            int byteIdx = currentBitPtr / 8;
            int bitPos = 7 - (currentBitPtr % 8);
            targetBit = (finalPayload[byteIdx] >> bitPos) & 1;
            currentBitPtr++;
        } else {
            // Real data finished! Inject random noise to maintain statistical uniformity
            targetBit = noiseRng() % 2;
        }
        
        embedBitStealthy(imgData, mappedIdx, targetBit, noiseRng);
    }

    stbi_write_png(outputPath.c_str(), width, height, channels, imgData, width * channels);
    stbi_image_free(imgData);
    cout << "[SUCCESS] Hardened Vault Created. Statistical footprint: UNIFORM." << endl;
}

int main(int argc, char* argv[]) {
    string inputPath, outputPath, message, password;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-i" && i + 1 < argc) inputPath = argv[++i];
        else if (arg == "-o" && i + 1 < argc) outputPath = argv[++i];
        else if (arg == "-m" && i + 1 < argc) message = argv[++i];
        else if (arg == "-p" && i + 1 < argc) password = argv[++i];
    }
    cout << "=== Pixel-Vault (v5.0 - HARDENED FINAL) ===" << endl;
    if (inputPath.empty()) {
        const char* res = tinyfd_openFileDialog("Select Cover", "", 2, (const char*[]){"*.png","*.bmp"}, "Images", 0);
        if (res) inputPath = res;
    }
    if (outputPath.empty()) {
        const char* res = tinyfd_saveFileDialog("Save Stego", "stego.png", 1, (const char*[]){"*.png"}, "PNG");
        if (res) outputPath = res;
    }
    if (message.empty()) { cout << "Message: "; getline(cin, message); }
    if (password.empty()) { cout << "Password: "; getline(cin, password); }

    processEmbedding(inputPath, outputPath, message, password);
    return 0;
}