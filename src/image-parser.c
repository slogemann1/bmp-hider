#include <stdlib.h>

#include "image-parser.h"
#include "macros.h"

// Bitmap file format
ImageData parse_image(uint8_t* raw_data, size_t length, ImageParseError* parse_error) {
    ImageData parsed = { 0 };
    
    if(length < 34) {
        *parse_error = PARSE_ERROR_INVALID_LENGTH;
        return parsed;
    }
    else if(*raw_data != 'B' || raw_data[1] != 'M') {
        *parse_error = PARSE_ERROR_INVALID_MAGIC_NUMBER;
        return parsed;
    }

    uint32_t reserved = *(uint32_t*)(raw_data + 6);
    uint8_t data_start = *(uint32_t*)(raw_data + 10);
    uint32_t width = *(uint32_t*)(raw_data + 18);
    uint32_t height = *(uint32_t*)(raw_data + 22);
    uint16_t image_depth = *(uint16_t*)(raw_data + 28);
    uint32_t compression = *(uint32_t*)(raw_data + 30);
    int32_t res_hoz = *(uint32_t*)(raw_data + 38);
    int32_t res_vrt = *(uint32_t*)(raw_data + 42);

    int padding = 4 - (((width * image_depth) / 8) % 4);
    int row_size = width * height * image_depth / 8 + height * padding;

    if(compression != 0) {
        *parse_error = PARSE_ERROR_COMPRESSION_NOT_SUPPORTED;
        return parsed;
    }
    else if(image_depth < 16) {
        *parse_error = PARSE_ERROR_MINIMUM_PIXEL_SIZE_16;
        return parsed;
    }
    else if(length < (size_t)(data_start + row_size)) {
        *parse_error = PARSE_ERROR_INVALID_LENGTH;
        return parsed;
    }

    Pixel* pixel_arr = (Pixel*)malloc(sizeof(Pixel) * width * height);
    for(int i = 0; (size_t)i < width * height; i++) {
        int index = data_start + i * image_depth / 8 + (i / width) * padding;

        Pixel currentPixel;
        if(image_depth == 16) {
            currentPixel.b = raw_data[index] & 0x0F;
            currentPixel.g = raw_data[index] & 0xF0;
            currentPixel.r = raw_data[index + 1] & 0x0F;
            currentPixel.a = raw_data[index + 1] & 0xF0;
        }
        else if(image_depth == 24) {
            currentPixel.b = raw_data[index];
            currentPixel.g = raw_data[index + 1];
            currentPixel.r = raw_data[index + 2];
        }
        else if(image_depth == 32) {
            currentPixel.b = (raw_data[index] + ((uint16_t)raw_data[index + 1] << 8)) & 0x1FF;
            currentPixel.g = (raw_data[index + 1] + ((uint16_t)raw_data[index + 2] << 8)) & 0x1FE;
            currentPixel.r = raw_data[index + 2] & 0xFE;
            currentPixel.a = raw_data[index + 3] & 0x1F;
        }

        pixel_arr[i] = currentPixel;
    }

    parsed.buffer = pixel_arr;
    parsed.height = height;
    parsed.width = width;
    parsed.resolution_horizontal = res_hoz;
    parsed.resolution_vertical = res_vrt;
    parsed.reserved = reserved;
    switch(image_depth) {
        case 16:
            parsed.type = IMAGE_RGBA16; break;
        case 24:
            parsed.type = IMAGE_RGB24; break;
        case 32:
            parsed.type = IMAGE_RGBA32; break;
    }

    *parse_error = PARSE_ERROR_NO_ERROR;
    return parsed;
}

uint8_t* create_image_file(ImageData data, size_t* data_length) {
    uint16_t image_depth;
    switch(data.type) {
        case IMAGE_RGBA16:
            image_depth = 16; break;
        case IMAGE_RGB24:
            image_depth = 24; break;
        case IMAGE_RGBA32:
            image_depth = 32; break;
        default:
            return NULL;
    }
    int padding = 4 - (((data.width * image_depth) / 8) % 4);
    
    uint32_t header_size = 54;
    uint32_t image_size = data.height * data.width * image_depth / 8 + data.height * padding;
    uint32_t file_size = image_size + header_size;

    uint8_t* buffer = (uint8_t*)malloc(file_size);

    // General Header
    buffer[0] = 'B'; buffer[1] = 'M'; // Magic Number
    *(uint32_t*)(buffer + 2) = file_size;
    *(uint32_t*)(buffer + 6) = data.reserved; // Reserved
    *(uint32_t*)(buffer + 10) = header_size; // Pixel Array Offset

    // Bitmap Core Header
    *(uint32_t*)(buffer + 14) = 40; // Size of Core Header
    *(uint32_t*)(buffer + 18) = data.width; // Width
    *(uint32_t*)(buffer + 22) = data.height; // Height
    buffer[26] = 1; buffer[27] = 0; // Number of Color Panes
    *(uint16_t*)(buffer + 28) = image_depth; // Pixel Size
    *(uint32_t*)(buffer + 30) = 0; // Compression Method
    *(uint32_t*)(buffer + 34) = data.height * data.width * image_depth / 8; // Image Size
    *(int32_t*)(buffer + 38) = data.resolution_horizontal;
    *(int32_t*)(buffer + 42) = data.resolution_vertical;
    *(uint32_t*)(buffer + 46) = 0; // Color Palette
    *(uint32_t*)(buffer + 50) = 0; // Important Colors

    for(int i = 0; (size_t)i < data.width * data.height; i++) {
        int index = header_size + i * image_depth / 8 + (i / data.width) * padding;

        Pixel currentPixel = data.buffer[i];
        if(image_depth == 16) {
            buffer[index] = (uint8_t)currentPixel.b;
            buffer[index] |= (uint8_t)currentPixel.g << 4;
            buffer[index + 1] = (uint8_t)currentPixel.r;
            buffer[index + 1] |= (uint8_t)currentPixel.a << 4;
        }
        else if(image_depth == 24) {
            buffer[index] = (uint8_t)currentPixel.b;
            buffer[index + 1] = (uint8_t)currentPixel.g;
            buffer[index + 2] = (uint8_t)currentPixel.r;
        }
        else if(image_depth == 32) {
            uint32_t* pixel_data = (uint32_t*)(buffer + index);
            *pixel_data = (uint8_t)currentPixel.b;
            *pixel_data |= currentPixel.g << 9;
            *pixel_data |= (uint32_t)currentPixel.b << 17;
            *pixel_data |= (uint32_t)currentPixel.a << 24;
        }
    }

    *data_length = file_size;
    return buffer;
}

void free_image_data(ImageData data) {
    free(data.buffer);
}