#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <ctime>

// Wrap the C-library headers in extern "C"
extern "C" {
    #include "aes.h"
}

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;

// Key Derivation Function
void deriveKey(string password, uint8_t* key) {
    uint8_t salt[16] = {0x50, 0x69, 0x78, 0x65, 0x6C, 0x56, 0x61, 0x75, 0x6C, 0x74, 0x32, 0x30, 0x32, 0x36, 0x21, 0x23};
    for (int i = 0; i < 16; i++) {
        if (i < password.length()) key[i] = (uint8_t)password[i] ^ salt[i];
        else key[i] = salt[i];
    }
}

void embedLSB(const string& inputPath, const string& outputPath, string message, const uint8_t* key) {
    // 1. Prepare the Message with Magic Bytes (NO PADDING NEEDED for CTR mode!)
    string rawMessage = "PVLT" + message;
    int cipherLen = rawMessage.length();
    vector<uint8_t> ciphertext(rawMessage.begin(), rawMessage.end());

    // 2. Generate a 16-byte Random Initialization Vector (IV)
    uint8_t iv[16];
    mt19937 rng(time(0)); 
    for (int i = 0; i < 16; i++) {
        iv[i] = rng() % 256;
    }

    // 3. Encrypt using AES-CTR mode
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    // CTR mode uses xcrypt for both encryption and decryption
    AES_CTR_xcrypt_buffer(&ctx, ciphertext.data(), cipherLen); 

    // 4. Assemble the Final Payload: [IV] + [Ciphertext]
    vector<uint8_t> finalPayload;
    for (int i = 0; i < 16; i++) finalPayload.push_back(iv[i]);
    for (int i = 0; i < cipherLen; i++) finalPayload.push_back(ciphertext[i]);
    
    int payloadBytes = finalPayload.size();

    // 5. Load Image using STB
    int width, height, channels;
    unsigned char* imgData = stbi_load(inputPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) { cerr << "Error: Could not load image!" << endl; return; }

    long long totalPixels = (long long)width * height * channels;
    if (totalPixels < 32 + (payloadBytes * 8)) {
        cerr << "Error: Image is too small!" << endl;
        stbi_image_free(imgData); return;
    }

    int bitIdx = 0;

    // 6. Embed the Length Prefix (32 bits) - Represents total bytes of (IV + Ciphertext)
    for (int i = 0; i < 32; i++) {
        int bit = (payloadBytes >> (31 - i)) & 1;
        imgData[bitIdx] = (imgData[bitIdx] & 0xFE) | bit;
        bitIdx++;
    }

    // 7. Embed the Payload bits
    for (int i = 0; i < payloadBytes; i++) {
        for (int b = 7; b >= 0; b--) {
            int bit = (finalPayload[i] >> b) & 1;
            imgData[bitIdx] = (imgData[bitIdx] & 0xFE) | bit;
            bitIdx++;
        }
    }

    // 8. Save as PNG
    stbi_write_png(outputPath.c_str(), width, height, channels, imgData, width * channels);
    stbi_image_free(imgData);
    cout << "Success: CTR-Encrypted data hidden in " << outputPath << endl;
}

int main() {
    string coverPath = "../images/cover.bmp";
    string stegoPath = "../images/stego.png";
    string secret, password;

    cout << "=== Pixel-Vault Encoder (v3.0 - AES-CTR) ===" << endl;
    cout << "Enter Secret Message: ";
    getline(cin, secret);
    cout << "Set Encryption Password: ";
    getline(cin, password);

    uint8_t key[16];
    deriveKey(password, key);

    embedLSB(coverPath, stegoPath, secret, key);
    return 0;
}