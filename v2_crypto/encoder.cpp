#include <iostream>
#include <vector>
#include <string>
#include <cstring>

// STB Image Libraries (Define implementation ONLY in one C++ file)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Tiny-AES-C
extern "C" {
    #include "aes.h"
}

using namespace std;

void embedLSB(const string& inputPath, const string& outputPath, string message, const uint8_t* key) {
    // 1. Load Image using STB (Supports PNG, JPG, BMP)
    int width, height, channels;
    unsigned char* imgData = stbi_load(inputPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) { cerr << "Error loading image!" << endl; return; }

    long long totalPixels = width * height * channels;

    // 2. Pad Message for AES (Must be multiple of 16 bytes)
    int padding = 16 - (message.length() % 16);
    message.append(padding, (char)padding); // PKCS7 Padding
    
    int msgLength = message.length();
    vector<uint8_t> buffer(message.begin(), message.end());

    // 3. Encrypt the Message
    struct AES_ctx ctx;
    AES_init_ctx(&ctx, key);
    for (int i = 0; i < msgLength; i += 16) {
        AES_ECB_encrypt(&ctx, buffer.data() + i);
    }

    // Ensure image has enough capacity (32 bits for length + message bits)
    if (totalPixels < 32 + (msgLength * 8)) {
        cerr << "Error: Image too small!" << endl;
        stbi_image_free(imgData);
        return;
    }

    int bitIdx = 0;

    // 4. Embed the Length Prefix (32 bits)
    for (int i = 0; i < 32; i++) {
        int bit = (msgLength >> (31 - i)) & 1;
        imgData[bitIdx] = (imgData[bitIdx] & 0xFE) | bit;
        bitIdx++;
    }

    // 5. Embed the Ciphertext
    for (int i = 0; i < msgLength; i++) {
        for (int b = 7; b >= 0; b--) {
            int bit = (buffer[i] >> b) & 1;
            imgData[bitIdx] = (imgData[bitIdx] & 0xFE) | bit;
            bitIdx++;
        }
    }

    // 6. Save as PNG (Lossless)
    stbi_write_png(outputPath.c_str(), width, height, channels, imgData, width * channels);
    stbi_image_free(imgData);
    cout << "Success: Encrypted data hidden in " << outputPath << endl;
}

int main() {
    string coverPath = "../images/cover.bmp"; // Moving to PNG for stability
    string stegoPath = "../images/stego.png";
    string secret    = "Hackathon-Secret-Key-2026";
    
    // AES-128 needs a 16-byte key
    uint8_t key[16] = "SuperSecretKey!"; 

    embedLSB(coverPath, stegoPath, secret, key);
    return 0;
}