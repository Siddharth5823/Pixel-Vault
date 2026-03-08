#include <iostream>
#include <vector>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern "C" {
    #include "aes.h"
}

using namespace std;

void deriveKey(string password, uint8_t* key) {
    // Standard "Salt" to ensure we get a 16-byte key even for short passwords
    uint8_t salt[16] = {0x50, 0x69, 0x78, 0x65, 0x6C, 0x56, 0x61, 0x75, 0x6C, 0x74, 0x32, 0x30, 0x32, 0x36, 0x21, 0x23};
    
    for (int i = 0; i < 16; i++) {
        if (i < password.length()) {
            key[i] = (uint8_t)password[i] ^ salt[i];
        } else {
            key[i] = salt[i];
        }
    }
}

void decodeLSB(const string& stegoPath, const uint8_t* key) {
    int width, height, channels;
    // Load the image using STB
    unsigned char* imgData = stbi_load(stegoPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) {
        cerr << "Error: Could not load stego image at " << stegoPath << endl;
        return;
    }

    int bitIdx = 0;

    // 1. Extract the 32-bit length prefix
    int msgLength = 0;
    for (int i = 0; i < 32; i++) {
        int bit = imgData[bitIdx] & 1;
        msgLength = (msgLength << 1) | bit;
        bitIdx++;
    }

    // Sanity Check: If the length is garbage (e.g., larger than the image can hold), abort immediately.
    long long totalPixels = (long long)width * height * channels;
    if (msgLength <= 0 || (msgLength * 8 + 32) > totalPixels) {
        cout << "\n[!] CRITICAL ERROR: Image does not contain a valid Pixel-Vault payload." << endl;
        stbi_image_free(imgData);
        return;
    }

    // 2. Extract the Ciphertext bytes
    vector<uint8_t> buffer(msgLength);
    for (int i = 0; i < msgLength; i++) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++) {
            int bit = imgData[bitIdx] & 1;
            byte = (byte << 1) | bit;
            bitIdx++;
        }
        buffer[i] = byte;
    }
    stbi_image_free(imgData); // Free memory early

    // 3. Decrypt the Ciphertext using AES-128-ECB
    struct AES_ctx ctx;
    AES_init_ctx(&ctx, key);
    for (int i = 0; i < msgLength; i += 16) {
        AES_ECB_decrypt(&ctx, buffer.data() + i);
    }

    // 4. THE SECURITY CHECK: Verify Magic Bytes
    string header = "";
    // Only check the first 4 bytes if the message is at least 4 bytes long!
    if (msgLength >= 4) {
        for (int i = 0; i < 4; i++) {
            header += (char)buffer[i];
        }
    }

    if (header != "PVLT") {
        cout << "\n[!] CRITICAL ERROR: Incorrect password or corrupted file." << endl;
        cout << "[!] Access Denied." << endl;
        return; // Exit early safely
    }

    // 5. Remove PKCS7 Padding safely
    try {
        int padding = buffer.back();
        // Validation: Padding must be between 1 and 16, and not larger than our actual message size
        if (padding <= 0 || padding > 16 || padding > (buffer.size() - 4)) {
             throw std::out_of_range("Invalid Padding");
        }
        
        // Final message: Skip the 4 Magic Bytes and remove padding
        string decryptedText(buffer.begin() + 4, buffer.end() - padding);
        cout << "\n[SUCCESS] Decoded Message: " << decryptedText << endl;
    } 
    catch (...) {
        cout << "\n[!] ERROR: Decryption logic failed. Check your password." << endl;
    }
}

int main() {

    string stegoPath = "../images/stego.png";
    string password;

    cout << "=== Pixel-Vault Decoder (v2.1) ===" << endl;
    cout << "Enter Decryption Password: ";
    getline(cin, password);

    uint8_t key[16];
    deriveKey(password, key);

    decodeLSB(stegoPath, key);
    return 0;

}