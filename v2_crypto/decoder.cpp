#include <iostream>
#include <vector>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern "C" {
    #include "aes.h"
}

using namespace std;

void decodeLSB(const string& stegoPath, const uint8_t* key) {
    int width, height, channels;
    unsigned char* imgData = stbi_load(stegoPath.c_str(), &width, &height, &channels, 0);
    if (!imgData) { cerr << "Error loading stego image!" << endl; return; }

    int bitIdx = 0;

    // 1. Extract the Length Prefix (32 bits)
    int msgLength = 0;
    for (int i = 0; i < 32; i++) {
        int bit = imgData[bitIdx] & 1;
        msgLength = (msgLength << 1) | bit;
        bitIdx++;
    }

    // 2. Extract the Ciphertext
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
    stbi_image_free(imgData);

    // 3. Decrypt the Ciphertext
    struct AES_ctx ctx;
    AES_init_ctx(&ctx, key);
    for (int i = 0; i < msgLength; i += 16) {
        AES_ECB_decrypt(&ctx, buffer.data() + i);
    }

    // 4. Remove PKCS7 Padding
    int padding = buffer.back();
    string decryptedText(buffer.begin(), buffer.end() - padding);

    cout << "Decoded Message: " << decryptedText << endl;
}

int main() {
    string stegoPath = "../images/stego.png";
    uint8_t key[16] = "SuperSecretKey!"; 

    decodeLSB(stegoPath, key);
    return 0;
}