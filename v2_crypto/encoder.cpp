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

void deriveKey(string password, uint8_t* key) {
    // A 16-byte "Salt" to ensure even short passwords are 16 bytes
    uint8_t salt[16] = {0x50, 0x69, 0x78, 0x65, 0x6C, 0x56, 0x61, 0x75, 0x6C, 0x74, 0x32, 0x30, 0x32, 0x36, 0x21, 0x23};
    
    for (int i = 0; i < 16; i++) {
        if (i < password.length()) {
            key[i] = (uint8_t)password[i] ^ salt[i];
        } else {
            key[i] = salt[i];
        }
    }
}


void embedLSB(const string& inputPath, const string& outputPath, string message, const uint8_t* key) {
    // 1. Prepare the Message with Header
    // Adding Magic Bytes "PVLT" allows the decoder to verify the password.
    string messageWithHeader = "PVLT" + message;

    // 2. PKCS7 Padding
    // AES requires blocks of exactly 16 bytes.
    int paddingVal = 16 - (messageWithHeader.length() % 16);
    messageWithHeader.append(paddingVal, (char)paddingVal); 

    int msgLength = messageWithHeader.length();
    vector<uint8_t> buffer(messageWithHeader.begin(), messageWithHeader.end());

    // 3. Load Image using STB
    int width, height, channels;
    unsigned char* imgData = stbi_load(inputPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) { 
        cerr << "Error: Could not load image " << inputPath << endl; 
        return; 
    }

    long long totalPixels = (long long)width * height * channels;

    // 4. Encrypt the Buffer (Header + Message + Padding)
    struct AES_ctx ctx;
    AES_init_ctx(&ctx, key);
    for (int i = 0; i < msgLength; i += 16) {
        AES_ECB_encrypt(&ctx, buffer.data() + i);
    }

    // 5. Capacity Check
    // We need 32 bits for the length prefix + 8 bits for every byte of ciphertext.
    if (totalPixels < 32 + (msgLength * 8)) {
        cerr << "Error: Image is too small to hold this message!" << endl;
        stbi_image_free(imgData);
        return;
    }

    int bitIdx = 0;

    // 6. Embed the Length Prefix (32 bits)
    // This tells the decoder how many encrypted bytes to read.
    for (int i = 0; i < 32; i++) {
        int bit = (msgLength >> (31 - i)) & 1;
        imgData[bitIdx] = (imgData[bitIdx] & 0xFE) | bit;
        bitIdx++;
    }

    // 7. Embed the Ciphertext bits
    for (int i = 0; i < msgLength; i++) {
        for (int b = 7; b >= 0; b--) {
            int bit = (buffer[i] >> b) & 1;
            imgData[bitIdx] = (imgData[bitIdx] & 0xFE) | bit;
            bitIdx++;
        }
    }

    // 8. Save as PNG (Must be lossless to preserve LSBs)
    if (stbi_write_png(outputPath.c_str(), width, height, channels, imgData, width * channels)) {
        cout << "Success: Encrypted data (with PVLT header) hidden in " << outputPath << endl;
    } else {
        cerr << "Error: Failed to write image to " << outputPath << endl;
    }

    stbi_image_free(imgData);
}

int main() {
    string coverPath = "../images/cover.bmp"; // Moving to PNG for stability
    string stegoPath = "../images/stego.png";
    string secret, password;

    cout << "=== Pixel-Vault Encoder (v2.1) ===" << endl;
    cout << "Enter Secret Message: ";
    getline(cin, secret);
    cout << "Set Encryption Password: ";
    getline(cin, password);

    uint8_t key[16];
    deriveKey(password, key);

    embedLSB(coverPath, stegoPath, secret, key);
    return 0;
}