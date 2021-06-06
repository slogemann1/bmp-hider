#include <stdint.h>

typedef enum ImageType {
    IMAGE_NONE,
    IMAGE_RGB24,
    IMAGE_RGBA16,
    IMAGE_RGBA32
} ImageType;

typedef enum ImageParseError {
    PARSE_ERROR_NO_ERROR,
    PARSE_ERROR_INVALID_MAGIC_NUMBER,
    PARSE_ERROR_INVALID_LENGTH,
    PARSE_ERROR_COMPRESSION_NOT_SUPPORTED,
    PARSE_ERROR_MINIMUM_PIXEL_SIZE_16
} ImageParseError;

typedef struct Pixel {
    uint16_t r;
    uint16_t g;
    uint16_t b;
    uint16_t a;
} Pixel;

typedef struct ImageData {
    ImageType type;
    size_t height;
    size_t width;
    Pixel* buffer;
    int32_t resolution_horizontal;
    int32_t resolution_vertical;
    int32_t reserved; // Contains data size
} ImageData;

ImageData parse_image(uint8_t* raw_data, size_t length, ImageParseError* parse_error);
uint8_t* create_image_file(ImageData data, size_t* data_length);
void free_image_data(ImageData data);