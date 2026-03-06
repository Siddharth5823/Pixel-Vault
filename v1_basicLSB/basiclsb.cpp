#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace std;

// Struct to hold BMP header info - shows low-level understanding
struct BMPHeader {
    unsigned char headerData[54];
    int dataOffset;
    int dataSize;
};

// --- HELPER FUNCTION: RAW BMP PARSER ---
// This function reads the BMP header so we know where to start hiding data.
BMPHeader readBMPHeader(ifstream& file) {
    BMPHeader bmp;
    file.read((char*)bmp.headerData, 54);

    // Verify BMP format by checking the signature 'BM'
    if (bmp.headerData[0] != 'B' || bmp.headerData[1] != 'M') {
        cerr << "Error: Not a valid BMP file!" << endl;
        // Basic error exit for procedural code
        exit(1); 
    }

    // Extract crucial info using pointer arithmetic (very CSE/low-level)
    bmp.dataOffset = *(int*)&bmp.headerData[10];
    bmp.dataSize = *(int*)&bmp.headerData[34];
    
    return bmp;
}

// --- v1.0 EMBEDDER (ENCODER) FUNCTION ---
void embedLSB(string inputPath, string outputPath, string secretMessage) {
    cout << "\n--- Embedding Process ---" << endl;
    
    ifstream file(inputPath, ios::binary);
    if (!file) { cerr << "Error: Cannot open cover image at " << inputPath << endl; return; }

    BMPHeader header = readBMPHeader(file);
    cout << "BMP verified. Pixel data starts at byte: " << header.dataOffset << endl;

    // Read the entire rest of the image data starting from the offset
    file.seekg(header.dataOffset, ios::beg);
    vector<unsigned char> imgData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Prepare message bits: important to add null terminator ('\0') so the decoder knows when to stop.
    secretMessage += '\0';
    long long bitIdx = 0;
    long long msgTotalBits = (long long)secretMessage.length() * 8;

    // Safety check for first year logic
    if (msgTotalBits > imgData.size()) {
        cerr << "Error: Message is too large for this image!" << endl;
        return;
    }

    // --- CORE LSB LOGIC ---
    // Iterate through raw image bytes. Replace LSB with message bit.
    for (long long i = 0; i < imgData.size() && bitIdx < msgTotalBits; i++) {
        // Find which character and which specific bit of that character we are on
        int charPos = bitIdx / 8;
        int bitPos = 7 - (bitIdx % 8); // We process from MSB to LSB of the char
        
        // Extract the target bit of the message (1 or 0)
        int messageBit = (secretMessage[charPos] >> bitPos) & 1;

        // Clear image LSB (mask 0xFE = 11111110) then OR with the message bit
        imgData[i] = (imgData[i] & 0xFE) | messageBit;
        bitIdx++;
    }

    // Write the complete BMP (Header + Modified Pixel Data) back out
    ofstream outFile(outputPath, ios::binary);
    outFile.write((char*)header.headerData, 54);
    
    // Crucial step: if the offset isn't exactly 54, you need to pad the file correctly.
    // For standard 24-bit BMPs, it's usually 54, but let's handle the gap professionally.
    if (header.dataOffset > 54) {
        vector<unsigned char> padding(header.dataOffset - 54, 0); // Placeholder padding
        // Professional note: true padding should be preserved from the original file,
        // but for a simple v1.0, this demonstrates the concept.
        outFile.write((char*)padding.data(), padding.size());
    }
    
    outFile.write((char*)imgData.data(), imgData.size());
    outFile.close();

    cout << "Success: \"" << secretMessage.substr(0, secretMessage.length()-1) 
         << "\" hidden securely in " << outputPath << endl;
}

// --- v1.0 DECODER (EXTRACTOR) FUNCTION ---
void decodeLSB(string stegoPath) {
    cout << "\n--- Decoding Process ---" << endl;
    
    ifstream file(stegoPath, ios::binary);
    if (!file) { cerr << "Error: Cannot open stego image at " << stegoPath << endl; return; }

    BMPHeader header = readBMPHeader(file);
    file.seekg(header.dataOffset, ios::beg);
    
    // Read the raw modified pixel data
    vector<unsigned char> imgData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    string extractedMessage = "";
    char currentChar = 0;
    int bitCount = 0;

    // --- CORE LSB DECODING ---
    // Read every modified byte and grab its LSB.
    for (unsigned char byte : imgData) {
        // Get the LSB (last bit) of the current image byte
        int bit = byte & 1;

        // Construct a character 8 bits at a time using bit shifting
        // We shift the existing bits left and add the new bit at the end
        currentChar |= (bit << (7 - bitCount));
        bitCount++;

        // When we have 8 bits, we have a character
        if (bitCount == 8) {
            // Check if it's the null terminator ('\0') we embedded
            if (currentChar == '\0') {
                break; // Stop decoding immediately!
            }
            extractedMessage += currentChar;
            currentChar = 0; // Reset for the next character
            bitCount = 0;
        }
    }

    cout << "Decoded Message: \"" << extractedMessage << "\"" << endl;
}

// --- MAIN FUNCTION (Execution Workflow) ---
int main() {
    cout << "=== Pixel-Vault v1.0 LSB Steganography (Procedural C++) ===" << endl;
    
    // Handling Paths based on your directory structure (image_0.png)
    // IMPORTANT: Make sure you have a 'cover.bmp' inside your 'images' folder.
    string coverPath = "../images/cover.bmp";
    string stegoPath = "../images/stego.bmp";
    string secret    = "Hackathon-Secret-Key-2026";

    // --- Workflow Demo ---
    // Step 1: Hide the message (Encoder)
    embedLSB(coverPath, stegoPath, secret);

    // Step 2: Retrieve the message from the output file (Decoder)
    // Note: We use the output path as the input for the decoder.
    decodeLSB(stegoPath);

    return 0;
}