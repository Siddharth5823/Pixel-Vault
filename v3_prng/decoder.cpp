#include <iostream>
#include <vector>
#include <string>

extern "C" {
    #include "aes.h"
}

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

void deriveKey(string password, uint8_t* key) {
    uint8_t salt[16] = {0x50, 0x69, 0x78, 0x65, 0x6C, 0x56, 0x61, 0x75, 0x6C, 0x74, 0x32, 0x30, 0x32, 0x36, 0x21, 0x23};
    for (int i = 0; i < 16; i++) {
        if (i < password.length()) key[i] = (uint8_t)password[i] ^ salt[i];
        else key[i] = salt[i];
    }
}

void decodeLSB(const string& stegoPath, const uint8_t* key) {
    int width, height, channels;
    unsigned char* imgData = stbi_load(stegoPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) { cerr << "Error: Could not load stego image!" << endl; return; }

    int bitIdx = 0;

    // 1. Extract the 32-bit length prefix
    int payloadBytes = 0;
    for (int i = 0; i < 32; i++) {
        int bit = imgData[bitIdx] & 1;
        payloadBytes = (payloadBytes << 1) | bit;
        bitIdx++;
    }

    long long totalPixels = (long long)width * height * channels;
    if (payloadBytes <= 16 || (payloadBytes * 8 + 32) > totalPixels) {
        cout << "\n[!] CRITICAL ERROR: Image does not contain a valid Pixel-Vault payload." << endl;
        stbi_image_free(imgData); return;
    }

    // 2. Extract the Payload (IV + Ciphertext)
    vector<uint8_t> finalPayload(payloadBytes);
    for (int i = 0; i < payloadBytes; i++) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++) {
            int bit = imgData[bitIdx] & 1;
            byte = (byte << 1) | bit;
            bitIdx++;
        }
        finalPayload[i] = byte;
    }
    stbi_image_free(imgData); 

    // 3. Separate the IV and the Ciphertext
    uint8_t iv[16];
    for (int i = 0; i < 16; i++) iv[i] = finalPayload[i];
    
    int cipherLen = payloadBytes - 16;
    vector<uint8_t> ciphertext(finalPayload.begin() + 16, finalPayload.end());

    // 4. Decrypt using AES-CTR mode
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    // CTR mode uses the exact same function to decrypt!
    AES_CTR_xcrypt_buffer(&ctx, ciphertext.data(), cipherLen); 

    // 5. Verify Magic Bytes
    string header = "";
    if (cipherLen >= 4) {
        for (int i = 0; i < 4; i++) header += (char)ciphertext[i];
    }

    if (header != "PVLT") {
        cout << "\n[!] CRITICAL ERROR: Incorrect password or corrupted file." << endl;
        cout << "[!] Access Denied." << endl;
        return;
    }

    // 6. Extract the final message (No padding to remove!)
    string decryptedText(ciphertext.begin() + 4, ciphertext.end());
    cout << "\n[SUCCESS] Decoded Message: " << decryptedText << endl;
}

int main() {
    string stegoPath = "../images/stego.png";
    string password;

    cout << "=== Pixel-Vault Decoder (v3.0 - AES-CTR) ===" << endl;
    cout << "Enter Decryption Password: ";
    getline(cin, password);

    uint8_t key[16];
    deriveKey(password, key);

    decodeLSB(stegoPath, key);
    return 0;
}