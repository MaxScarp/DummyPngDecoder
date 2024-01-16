#include <zlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define PNG_PATH "basn6a08.png"
#define PNG_SIGNATURE_LENGTH 8
#define CHUNK_DATA_LENGTH 4
#define CHUNK_TYPE_LENGTH 4
#define CHUNK_CRC_LENGTH 4
#define LAST_CHUNK_TYPE_SIGNATURE "IEND"
#define IHDR_LENGTH 13
#define IHDR_WIDTH_BYTES 4
#define IHDR_HEIGHT_BYTES 4
#define IHDR_OTHER_BYTES 1
#define DATA_CHUNK_TYPE "IDAT"

// Structure to represent a PNG chunk
typedef struct Chunk
{
    unsigned int dataLength;
    unsigned char type[CHUNK_TYPE_LENGTH + 1];
    unsigned char* data;
} Chunk;

// Function to get the size of a file
int GetFileSize(FILE* file)
{
    // Open the file in binary mode
    if(fopen_s(&file, PNG_PATH, "rb") != 0)
    {
        fprintf(stderr, "Error: Can't open the file!\n");
        return -1;
    }
    
    // Move the cursor to the end of the file
    if(fseek(file, 0, SEEK_END) < 0)
    {
        fclose(file);
        fprintf(stderr, "Error: Can't find the end of the file!\n");
        return -1;
    }
    
    // Get the file size
    const int fileSize = ftell(file);
    fclose(file);

    return fileSize;
}

// Function to fill a buffer with the contents of a file
int FillBuffer(FILE* file, unsigned char* buffer, const int fileSize, unsigned int* cursor)
{
    const unsigned char pngSignature[] = {137, 80, 78, 71, 13, 10, 26, 10};

    // Open the file in binary mode
    if(fopen_s(&file, PNG_PATH, "rb") != 0)
    {
        fprintf(stderr, "Error: Can't open the file!\n");
        return -1;
    }

    // Read the entire file into the buffer
    if(fread_s((unsigned char*)buffer, fileSize, 1, fileSize, file) <= 0)
    {
        free((unsigned char*)buffer);
        fclose(file);
        fprintf(stderr, "Error: Something in the reading went wrong!\n");
        return -1;
    }

    // Check the PNG signature
    if(memcmp(pngSignature, buffer, PNG_SIGNATURE_LENGTH) != 0)
    {
        free((unsigned char*)buffer);
        fclose(file);
        fprintf(stderr, "Error: Invalid PNG signature!\n");
        return -1;
    }

    *cursor = PNG_SIGNATURE_LENGTH;

    fclose(file);

    return 0;
}

// Function to determine if the system is little-endian
bool IsLittleEndian()
{
    union
    {
        unsigned int value;
        unsigned char bytes[4];
    } test;

    test.value = 1;

    return test.bytes[0] == 1;
}

// Function to convert a value to little-endian
unsigned int ToLittleEndian(const unsigned int value)
{
    return ((value & 0xFF000000) >> 24) | ((value & 0x00FF0000) >> 8) | ((value & 0x0000FF00) << 8) | ((value & 0x000000FF) << 24);
}

// Function to read a PNG chunk
int ReadChunk(const unsigned char* buffer, unsigned int* cursor, Chunk* chunk, const bool isLittleEndian)
{
    // Read data length
    memcpy(&chunk->dataLength, buffer + *cursor, CHUNK_DATA_LENGTH);
    if(isLittleEndian)
    {
        chunk->dataLength = ToLittleEndian(chunk->dataLength);
    }
    *cursor += CHUNK_DATA_LENGTH;

    // Read chunk type
    memcpy(&chunk->type, buffer + *cursor, CHUNK_TYPE_LENGTH);
    chunk->type[CHUNK_TYPE_LENGTH] = '\0';
    *cursor += CHUNK_TYPE_LENGTH;

    // Allocate memory for chunk data
    chunk->data = (unsigned char*)malloc(chunk->dataLength);
    if(!chunk->data)
    {
        free((unsigned char*)buffer);
        fprintf(stderr, "Error: Unable to allocate enough memory for chunk data!\n");
        return -1;
    }

    // Read chunk data
    memcpy(chunk->data, buffer + *cursor, chunk->dataLength);
    *cursor += chunk->dataLength;
    
    // Read CRC
    unsigned int crc;
    memcpy(&crc, buffer + *cursor, CHUNK_CRC_LENGTH);
    if(isLittleEndian)
    {
        crc = ToLittleEndian(crc);
    }
    *cursor += CHUNK_CRC_LENGTH;

    // Verify checksum
    unsigned int checksum = crc32(0L, Z_NULL, 0);
    checksum = crc32(checksum, chunk->type, CHUNK_TYPE_LENGTH);
    checksum = crc32(checksum, chunk->data, chunk->dataLength);

    if(crc != checksum)
    {
        free((unsigned char*)buffer);
        free(chunk->data);
        fprintf(stderr, "Error: Checksum failed! %u != %u\n", crc, checksum);
        return -1;
    }

    return 0;
}

// Function to append a chunk to the dynamic array
int AppendChunk(Chunk** chunkDynamicArray, const unsigned int arraySize, const Chunk* chunk)
{
    *chunkDynamicArray = realloc(*chunkDynamicArray, arraySize * sizeof(Chunk));
    if(!*chunkDynamicArray)
    {
        fprintf(stderr, "Error: Unable to reallocate memory for chunk dynamic array!\n");
        return -1;
    }

    (*chunkDynamicArray)[arraySize - 1] = *chunk;

    return 0;
}

// Enumeration for PNG color types
typedef enum ColorType
{
    GRAYSCALE = 0,
    TRUECOLOR = 2,
    INDEXED_COLOR = 3,
    GRAYSCALE_WITH_ALPHA = 4,
    TRUECOLOR_WITH_ALPHA = 6,
    LAST_IMAGE_TYPE
} ColorType;

// Structure to represent the IHDR chunk data
typedef struct Ihdr
{
    unsigned int width;
    unsigned int height;
    unsigned int bitDepth;
    ColorType colorType;
    char compressionMethod;
    char filterMethod;
    char interlaceMethod;
} Ihdr;

// Function to get data from IHDR chunk
int GetIhdrChunkData(const Chunk* ihdrChunk, Ihdr* ihdr, const bool isLittleEndian)
{
    if(ihdrChunk->dataLength < IHDR_LENGTH)
    {
        fprintf(stderr, "Error: IHDR chunk data length is less than %d!\n", IHDR_LENGTH);
        return -1;
    }

    // Read width and height
    unsigned int index = 0;
    memcpy(&ihdr->width, ihdrChunk->data + index, IHDR_WIDTH_BYTES);
    if(ihdr->width <= 0)
    {
        fprintf(stderr, "Error: IHDR width less/equal zero!\n");
        return -1;
    }
    index += IHDR_WIDTH_BYTES;
    memcpy(&ihdr->height, ihdrChunk->data + index, IHDR_HEIGHT_BYTES);
    if(ihdr->height <= 0)
    {
        fprintf(stderr, "Error: IHDR height less/equal zero!\n");
        return -1;
    }
    index += IHDR_HEIGHT_BYTES;

    // Convert to little-endian if necessary
    if(isLittleEndian)
    {
        ihdr->width = ToLittleEndian(ihdr->width);
        ihdr->height = ToLittleEndian(ihdr->height);
    }

    // Read bit depth
    memcpy(&ihdr->bitDepth, ihdrChunk->data + index, IHDR_OTHER_BYTES);    
    if(ihdr->bitDepth != 1 && ihdr->bitDepth != 2 && ihdr->bitDepth != 4 && ihdr->bitDepth != 8 && ihdr->bitDepth != 16)
    {
        fprintf(stderr, "Error: Invalid IHDR bit depth!\n");
        return -1;
    }
    index += IHDR_OTHER_BYTES;

    // Read color type and verify if there are misconfigurations
    memcpy(&ihdr->colorType, ihdrChunk->data + index, IHDR_OTHER_BYTES);
    if(ihdr->colorType != GRAYSCALE && ihdr->colorType != TRUECOLOR && ihdr->colorType != INDEXED_COLOR && ihdr->colorType != GRAYSCALE_WITH_ALPHA && ihdr->colorType != TRUECOLOR_WITH_ALPHA)
    {
        fprintf(stderr, "Error: Invalid IHDR color type!\n");
        return -1;
    }
    switch ((int)ihdr->colorType)
    {
        case TRUECOLOR:
            if(ihdr->bitDepth != 8 && ihdr->bitDepth != 16)
            {
                fprintf(stderr, "Error: IHDR color type and bit depth invalid pair!\n");
                return -1;
            }
            break;
        case INDEXED_COLOR:
            if(ihdr->bitDepth != 1 && ihdr->bitDepth != 2 && ihdr->bitDepth != 4 && ihdr->bitDepth != 8)
            {
                fprintf(stderr, "Error: IHDR color type and bit depth invalid pair!\n");
                return -1;
            }
            break;
        case GRAYSCALE_WITH_ALPHA:
            if(ihdr->bitDepth != 8 && ihdr->bitDepth != 16)
            {
                fprintf(stderr, "Error: IHDR color type and bit depth invalid pair!\n");
                return -1;
            }
            break;
        case TRUECOLOR_WITH_ALPHA:
            if(ihdr->bitDepth != 8 && ihdr->bitDepth != 16)
            {
                fprintf(stderr, "Error: IHDR color type and bit depth invalid pair!\n");
                return -1;
            }
            break;
    }
    index += IHDR_OTHER_BYTES;
    
    // Read compression method
    memcpy(&ihdr->compressionMethod, ihdrChunk->data + index, IHDR_OTHER_BYTES);
    if(ihdr->compressionMethod != 0)
    {
        fprintf(stderr, "Error: IHDR compression method not zero. Only zero value allowed for PNG!\n");
        return -1;
    }
    index += IHDR_OTHER_BYTES;

    // Read filter method
    memcpy(&ihdr->filterMethod, ihdrChunk->data + index, IHDR_OTHER_BYTES);
    if(ihdr->filterMethod != 0)
    {
        fprintf(stderr, "Error: IHDR filter method not zero. Only zero value allowed for PNG!\n");
        return -1;
    }
    index += IHDR_OTHER_BYTES;

    // Read interlace method
    memcpy(&ihdr->interlaceMethod, ihdrChunk->data + index, IHDR_OTHER_BYTES);
    if(ihdr->interlaceMethod != 0 && ihdr->interlaceMethod != 1)
    {
        fprintf(stderr, "Error: IHDR interlace method not valid!\n");
        return -1;
    }

    return 0;
}

// Function to decompress IDAT chunks
int DecompressIdatChuncks(Chunk* chunkDynamicArray, const unsigned int chunkArraySize, unsigned char* uncompressedDestination, unsigned long* uncompressedSize)
{
    unsigned char* compressedSource = malloc(ULONG_MAX);
    if(!compressedSource)
    {
        fprintf(stderr, "Error: Unable to allocate enough memory for compressed source!\n");
        return -1;
    }
    unsigned int compressedSourceIndex = 0;
    unsigned long compressedSize = 0;
    // Collect all compressed data from IDAT chunks
    for(int i = 0; i < chunkArraySize; i++)
    {
        if(strcmp((const char*)(chunkDynamicArray + i)->type, DATA_CHUNK_TYPE) == 0)
        {
            memcpy(compressedSource + compressedSourceIndex, (chunkDynamicArray + i)->data, (chunkDynamicArray + i)->dataLength);
            compressedSize += (chunkDynamicArray + i)->dataLength;
            compressedSourceIndex++;
        }
    }
    realloc(compressedSource, compressedSize);

    // Decompress the collected data
    uncompressedDestination = malloc(ULONG_MAX);
    if(!uncompressedDestination)
    {
        free(compressedSource);
        fprintf(stderr, "Error: Unable to allocate enough memory for uncompressed destination!\n");
        return -1;
    }
    int result = uncompress(uncompressedDestination, uncompressedSize, compressedSource, compressedSize);
    if(result != Z_OK)
    {
        free(compressedSource);
        fprintf(stderr, "Error: Cannot decompress!\n");
        return -1;
    }

    free(compressedSource);
    realloc(uncompressedDestination, *uncompressedSize);

    return 0;
}

int main(int argc, char** argv, char** envs)
{
    bool isLittleEndian = IsLittleEndian();

    // Get the size of the file
    FILE* file;
    const int fileSize = GetFileSize(file);
    if(fileSize == -1)
    {
        return -1;
    }

    // Allocate a buffer to hold the file content
    const unsigned char* buffer = (unsigned char*)malloc(fileSize);
    if(!buffer)
    {
        fprintf(stderr, "Error: Unable to allocate enough memory!\n");
        return -1;
    }

    // Fill the buffer with file content and validate PNG signature
    unsigned int cursor;
    if(FillBuffer(file, (unsigned char*)buffer, fileSize, &cursor) == -1)
    {
        return -1;
    }

    Chunk* chunkDynamicArray = NULL;
    unsigned int chunkArraySize = 0;
    // Read chunks until the last chunk is encountered
    for(;;)
    {
        Chunk chunk;
        // Read the next chunk
        if(ReadChunk(buffer, &cursor, &chunk, isLittleEndian) == -1)
        {
            return -1;
        }

        // Append the chunk to the dynamic array
        if(AppendChunk(&chunkDynamicArray, ++chunkArraySize, &chunk) == -1)
        {
            for(int i = 0; i < chunkArraySize - 1; i++)
            {
                free((chunkDynamicArray + i)->data);
            }
            free(chunkDynamicArray);
            free(chunk.data);
            free((unsigned char*)buffer);
            return -1;
        }      

        // Break the loop if the last chunk is reached
        if(strcmp((const char*)chunk.type, LAST_CHUNK_TYPE_SIGNATURE) == 0)
        {
            free((unsigned char*)buffer);
            break;
        } 
    }

    Ihdr ihdr;
    // Get information from the IHDR chunk
    if(GetIhdrChunkData(&chunkDynamicArray[0], &ihdr, isLittleEndian) == -1)
    {
        for(int i = 0; i < chunkArraySize; i++)
        {
            free((chunkDynamicArray + i)->data);
        }
        free(chunkDynamicArray);
        return -1;
    }

    unsigned char* uncompressedDestination;
    unsigned long uncompressedSize = ULONG_MAX;
    // Decompress IDAT chunks
    if(DecompressIdatChuncks(chunkDynamicArray, chunkArraySize, uncompressedDestination, &uncompressedSize) == -1)
    {
        free(uncompressedDestination);
        for(int i = 0; i < chunkArraySize; i++)
        {
            free((chunkDynamicArray + i)->data);
        }
        free(chunkDynamicArray);
        return -1;
    }
     // Clean up allocated memory
    for(int i = 0; i < chunkArraySize; i++)
    {
        free((chunkDynamicArray + i)->data);
    }
    free(chunkDynamicArray);

    for(int i = 0; i < uncompressedSize; i+= ihdr.width * 4)
    {
        printf("%hhu\n", *(uncompressedDestination + i));
    }

    //const unsigned int bytesPerPixel = 4;
    //for(int i = 0; i < ihdr.height * ihdr.width; i+= ihdr.width * bytesPerPixel)
    //{
    //    printf("%u\n", *(uncompressedDestination + i));
    //}

    return 0;
}