#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "image-parser.h"
#include "macros.h"

// Constants
#define PROJ_EXE "bmp-hider"
#define PROJ_NAME "Bitmap File Hider"
#define PROJ_VERSION "v0.1.0"

#define FILE_BUFFER_SIZE 2048

// Function definitions
static void print_help_message(void);
static int handle_embed_file();
static int handle_print_size();
static int handle_reverse();
static char* get_image_parser_error_message(ImageParseError error);
static int read_args(int argc, char** argv);
static int embed_content(ImageData img_data, uint8_t* content, size_t content_length);
static uint8_t* retrieve_content(ImageData img_data, size_t* content_size);
static uint8_t* read_file(char* filename, size_t* amount_read);
static int write_file(char* filename, uint8_t* buffer, size_t length);
static int determine_max_content(ImageData data, int bits);

// Tests in debug mode
#ifndef NDEBUG
static void run_tests();
#endif

// Globals
char* data_file = NULL;
char* image_file = NULL;
char* outfile = NULL;
bool print_help = false;
bool print_version = false;
bool print_size = false;
bool reverse = false;
int bit_number = 2;

int main(int argc, char** argv) {
    #ifndef NDEBUG
    run_tests();
    #endif

    // Read arguments
    int exit_code = read_args(argc, argv);
    if(exit_code) {
        return exit_code;
    }
    
    // Handle read arguments
    if(print_help) {
        print_help_message();
    }
    else if(print_version) {
        printf(PROJ_NAME " version " PROJ_VERSION "\n");
    }
    else if(image_file == NULL) {
        eprintf("Error: The argument IMAGEFILE is required\n");
        return 1;
    }
    else if(print_size) {
        handle_print_size();
    }
    else if(reverse) {
        handle_reverse();
    }
    else if(data_file == NULL) {
        eprintf("Error: The argument DATAFILE is required when embedding a file\n");
        return 1;
    }
    else {
        int error = handle_embed_file();
        return error;
    }

    return 0;
}

static int handle_embed_file() {
    int return_code = 0;

    size_t data_file_size;
    size_t image_file_size;
    uint8_t* data_file_contents = read_file(data_file, &data_file_size);
    data_file_contents = realloc(data_file_contents, data_file_size + 4); // Garbage data at end to stop segfault
    uint8_t* image_file_contents = read_file(image_file, &image_file_size);

    if(data_file_contents == NULL) {
        eprintf("Error: File '%s' could not be read\n", data_file);
        return 1;
    }
    else if(image_file_contents == NULL) {
        eprintf("Error: File '%s' could not be read\n", image_file);
        return 1;
    }

    ImageParseError error;
    ImageData image = parse_image(image_file_contents, image_file_size, &error);
    image.reserved = (uint32_t)data_file_size;
    
    if(error) {
        free(data_file_contents);
        free(image_file_contents);
        eprintf("Error: %s\n", get_image_parser_error_message(error));
        return 1;
    }

    int error_int = embed_content(image, data_file_contents, data_file_size);
    if(error) {
        free(data_file_contents);
        free(image_file_contents);
        eprintf("Error: The file was to large to embed into the image with the current bit setting\n");
        return 1;
    }

    size_t new_file_data_length;
    uint8_t* new_file_data = create_image_file(image, &new_file_data_length);
    error_int = write_file(outfile == NULL ? "out.bmp" : outfile, new_file_data, new_file_data_length);
    if(error_int) {
        eprintf("Error: Failed to write to file\n");
        return_code = 1;
    }
    
    free(new_file_data);
    free_image_data(image);
    free(data_file_contents);
    free(image_file_contents);

    return return_code;
}

static int handle_reverse() {
    int return_code = 0;

    size_t image_file_size;
    uint8_t* image_file_contents = read_file(image_file, &image_file_size);

    if(image_file_contents == NULL) {
        eprintf("Error: File '%s' could not be read\n", image_file);
        return 1;
    }

    ImageParseError error;
    ImageData image = parse_image(image_file_contents, image_file_size, &error);
    if(error) {
        free(image_file_contents);
        eprintf("Error: %s\n", get_image_parser_error_message(error));
        return 1;
    }

    size_t content_length;
    uint8_t* content = retrieve_content(image, &content_length);

    if(content == NULL) {
        eprintf("Error: The file was incorrectly encoded\n");
        return_code = 1;
    }
    else {
        int error_int = write_file(outfile == NULL ? "out.bin" : outfile, content, content_length);
        if(error_int) {
            eprintf("Error: Failed to write to file\n");
            return_code = 1;
        }
    }
    
    free(content);
    free_image_data(image);
    free(image_file_contents);

    return return_code;
}

static int handle_print_size() {
    size_t image_file_size;
    uint8_t* image_file_contents = read_file(image_file, &image_file_size);

    if(image_file_contents == NULL) {
        eprintf("Error: File '%s' could not be read\n", image_file);
        return 1;
    }

    ImageParseError error;
    ImageData image = parse_image(image_file_contents, image_file_size, &error);
    if(error) {
        free(image_file_contents);
        eprintf("Error: %s\n", get_image_parser_error_message(error));
        return 1;
    }

    int byte_num = determine_max_content(image, bit_number);
    
    free_image_data(image);
    free(image_file_contents);

    if(byte_num == 0) {
        eprintf("The image encoding would not support bit amounts of %d without severely damaging the image content\n", bit_number);
    }
    else {
        printf("The image can store %d bytes using the %d least significant bit(s)\n", byte_num, bit_number);
    }

    return 0;
}

static void print_help_message(void) {
    printf(PROJ_NAME " " PROJ_VERSION "\n");
    printf("Useage: " PROJ_EXE " [FLAGS] [ARGUMENTS]\n");
    printf("FLAGS:\n");
    printf("     -h (--help)                    Displays this help message\n");
    printf("     -v (--version)                 Displays the version\n");
    printf("     -i (--image-file) IMAGEFILE    Accepts the image file name (required)\n");
    printf("     -d (--data-file) DATAFILE      Accepts the data file name\n");
    printf("     -o (--out-file) OUTFILE        Accepts the output file name\n");
    printf("     -s (--max-size)                Displays the maximum size (in bytes) that can be embedded in the image\n");
    printf("     -b (--bit-number) BITNUM       Accepts number of bits used for embedding\n");
    printf("     -r (--reverse)                 Retrieves an embedded file created using this tool\n");
    printf("ARGUMENTS:\n");
    printf("     IMAGEFILE                      The image file in/from which data should be hidden/retrieved\n");
    printf("     DATAFILE                       The file containing the data to hide\n");
    printf("     OUTFILE                        The file to which generated output should be written\n");
    printf("     BITNUM                         The number of less significant bits to use for embedding\n");
}

static char* get_image_parser_error_message(ImageParseError error) {
    switch(error) {
        case PARSE_ERROR_INVALID_MAGIC_NUMBER:
        case PARSE_ERROR_INVALID_LENGTH:
            return "The image file does not have the correct format";
        case PARSE_ERROR_COMPRESSION_NOT_SUPPORTED:
            return "This tool does not support compressed bitmap files";
        case PARSE_ERROR_MINIMUM_PIXEL_SIZE_16:
            return "This tool does not support pixel encodings with less than 16 bytes in total";
        default:
            return NULL; // Should never happen
    }
}

static int read_args(int argc, char** argv) {
    if(argc == 1) {
        eprintf("Error: No arguments were given\n");
        return 1;
    }

    bool val_expected = false;
    for(int i = 1; i < argc; i++) {
        char* arg = argv[i];

        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            print_help = true;
        }
        else if(!strcmp(arg, "-v") || !strcmp(arg, "--version")) {
            print_version = true;
        }
        else if(!strcmp(arg, "-i") || !strcmp(arg, "--image-file")) {
            if(i + 1 < argc) {
                image_file = argv[i + 1];
                i++;
            }
            else val_expected = true;
        }
        else if(!strcmp(arg, "-d") || !strcmp(arg, "--data-file")) {
            if(i + 1 < argc) {
                data_file = argv[i + 1];
                i++;
            }
            else val_expected = true;
        }
        else if(!strcmp(arg, "-s") || !strcmp(arg, "--max-size")) {
            print_size = true;
        }
        else if(!strcmp(arg, "-b") || !strcmp(arg, "--bit-number")) {
            if(i + 1 < argc) {
                bit_number = atoi(argv[i + 1]);
                i++;
            }
            else val_expected = true;
        }
        else if(!strcmp(arg, "-o") || !strcmp(arg, "--outfile-name")) {
            if(i + 1 < argc) {
                outfile = argv[i + 1];
                i++;
            }
            else val_expected = true;
        }
        else if(!strcmp(arg, "-r") || !strcmp(arg, "--reverse")) {
            reverse = true;
        }
        else {
            eprintf("Error: Unexpected argument '%s'\n", arg);
            return 1;
        }

        if(val_expected) {
            eprintf("Error: The flag '%s' expected a value\n", arg);
            return 1;
        }
    }

    return 0;
}

static uint8_t* read_file(char* filename, size_t* size_read) {
    FILE* file = fopen(filename, "rb");
    if(file == NULL) {
        return NULL;
    }
    
    uint8_t* buffer = (uint8_t*)malloc(0);
    size_t total_read = 0;
    size_t bytes_read;
    do {
        buffer = (uint8_t*)realloc(buffer, total_read + FILE_BUFFER_SIZE);
        bytes_read = fread(buffer + total_read, 1, FILE_BUFFER_SIZE, file);
        total_read += bytes_read;
    }
    while(bytes_read == FILE_BUFFER_SIZE);

    if(ferror(file)) {
        return NULL;
    }
    fclose(file);

    *size_read = total_read;
    return buffer;
}

static int write_file(char* filename, uint8_t* buffer, size_t length) {
    FILE* file = fopen(filename, "wb");
    if(file == NULL) {
        return 1;
    }

    fwrite(buffer, 1, length, file);
    if(ferror(file)) {
        return 1;
    }
    fclose(file);

    return 0;
}

static int determine_max_content(ImageData data, int bits) {
    int factor = 0; // Number of pixels or 0 if invalid
    
    switch(data.type) {
        case IMAGE_RGBA16:
            if(bits < 3) factor = 4;
            break;
        case IMAGE_RGB24:
            if(bits < 5) factor = 3;
            break;
        case IMAGE_RGBA32:
            if(bits < 5) factor = 4;
            break;
        default: break;
    }

    return (factor * bits * data.width * data.height) / 8;
}


// Get the bits (amount of bits = bit_number) from the specified bit index of the buffer (expressed in byte and bit)  
static inline uint16_t get_bits(uint8_t* content, int byte, int bit) {
    if(bit_number + bit <= 8) {
        uint8_t bit_number_inv = 8 - bit_number;
        uint8_t mask = 0xFF << bit_number_inv;
        mask = mask >> bit_number_inv; // Mask with correct number of 1s
        uint8_t mask_shifted = mask << bit; // Mask with 1s in correct position

        uint8_t content_bits = content[byte] & mask_shifted; // Content with correct bits
        uint8_t content_bits_shifted = content_bits >> bit; // Content with bits in correct position

        return content_bits_shifted;
    }
    else {
        uint8_t bits_in_1 = 8 - bit;
        uint8_t bits_in_2 = bit_number - bits_in_1;

        uint8_t mask_1 = 0xFF << (8 - bits_in_1);
        uint8_t mask_2 = 0xFF >> (8 - bits_in_2);

        uint8_t content_1 = (content[byte] & mask_1) >> (8 - bits_in_1);
        uint8_t content_2 = (content[byte + 1] & mask_2) << bits_in_1;

        uint8_t final_content = content_1 | content_2;
        return final_content;
    }
}

static int embed_content(ImageData img_data, uint8_t* content, size_t content_length) {
    size_t max_size = (size_t)determine_max_content(img_data, bit_number);
    if(max_size < content_length) {
        return 1;
    }

    uint8_t byte_mask = 0xFF << bit_number;

    int content_byte = 0;
    int content_bit = 0;
    for(int i = 0; (size_t)i < img_data.width * img_data.height && (size_t)content_byte < content_length; i++) {
        Pixel* pixel = img_data.buffer + i;
        
        pixel->r = (pixel->r & byte_mask) | get_bits(content, content_byte, content_bit);
        content_byte += (content_bit + bit_number >= 8) ? 1 : 0;
        content_bit = (content_bit + bit_number) % 8;
        
        pixel->g = (pixel->g & byte_mask) | get_bits(content, content_byte, content_bit);
        content_byte += (content_bit + bit_number >= 8) ? 1 : 0;
        content_bit = (content_bit + bit_number) % 8;
        
        pixel->b = (pixel->b & byte_mask) | get_bits(content, content_byte, content_bit);
        content_byte += (content_bit + bit_number >= 8) ? 1 : 0;
        content_bit = (content_bit + bit_number) % 8;
        
        if(img_data.type == IMAGE_RGBA16 || img_data.type == IMAGE_RGBA32) {
            pixel->a = (pixel->a & byte_mask) | get_bits(content, content_byte, content_bit);
            content_byte += (content_bit + bit_number >= 8) ? 1 : 0;
            content_bit = (content_bit + bit_number) % 8;
        }
    }

    return 0;
}

// Adds the bits of data (amount of bits = bit_number) to the buffer at the specified bit index (expressed in byte and bit)
// Data can only contain the important bits e.g. 0b00000101 and not 0b10101101 (for bit_number = 3)
static inline void add_bits(uint8_t* buffer, int byte, int bit, uint8_t data) {
    if(bit_number + bit <= 8) {
        buffer[byte] |= data << bit;
    }
    else {
        int bits_in_1 = 8 - bit;

        buffer[byte] |= data << bit;
        buffer[byte + 1] |= data >> bits_in_1;
    }
}

static uint8_t* retrieve_content(ImageData img_data, size_t* content_size) {
    if(img_data.reserved > determine_max_content(img_data, bit_number)) {
        return NULL;
    }

    *content_size = (size_t)img_data.reserved;
    uint8_t* buffer = malloc(*content_size + 2); // Extra space to avoid segfault
    memset(buffer, 0, *content_size);

    uint8_t byte_mask = 0xFF << (8 - bit_number);
    byte_mask = byte_mask >> (8 - bit_number);

    int content_byte = 0;
    int content_bit = 0;
    for(int i = 0; (size_t)i < img_data.width * img_data.height && (size_t)content_byte < *content_size; i++) {
        Pixel pixel = img_data.buffer[i];
        
        uint8_t r_data = pixel.r & byte_mask;
        add_bits(buffer, content_byte, content_bit, r_data);
        content_byte += (content_bit + bit_number >= 8) ? 1 : 0;
        content_bit = (content_bit + bit_number) % 8;

        uint8_t g_data = pixel.g & byte_mask;
        add_bits(buffer, content_byte, content_bit, g_data);
        content_byte += (content_bit + bit_number >= 8) ? 1 : 0;
        content_bit = (content_bit + bit_number) % 8;
        
        uint8_t b_data = pixel.b & byte_mask;
        add_bits(buffer, content_byte, content_bit, b_data);
        content_byte += (content_bit + bit_number >= 8) ? 1 : 0;
        content_bit = (content_bit + bit_number) % 8;
        
        if(img_data.type == IMAGE_RGBA16 || img_data.type == IMAGE_RGBA32) {
            uint8_t a_data = pixel.a & byte_mask;
            add_bits(buffer, content_byte, content_bit, a_data);
            content_byte += (content_bit + bit_number >= 8) ? 1 : 0;
            content_bit = (content_bit + bit_number) % 8;
        }
    }

    return buffer;
}

#ifndef NDEBUG
#define ASSERT(arg1, arg2) if(arg1 != arg2) { printf("Assertion failed at %s line %d, %hhu != %hhu\n", __FUNCTION__, __LINE__, arg1, arg2); }

static void TEST_get_bits() {
    uint8_t arr[] = { 0b00101111, 0b10011011 };
    
    bit_number = 3;
    uint8_t content_bits = get_bits(arr, 0, 3);
    ASSERT(content_bits, 0b00000101);

    bit_number = 2;
    content_bits = get_bits(arr, 1, 0);
    ASSERT(content_bits, 0b00000011);

    bit_number = 3;
    content_bits = get_bits(arr, 1, 1);
    ASSERT(content_bits, 0b00000101);

    bit_number = 3;
    content_bits = get_bits(arr, 0, 7);
    ASSERT(content_bits, 0b00000110);
}

static void TEST_add_bits() {
    uint8_t arr[2];

    *(uint16_t*)arr = 0;
    bit_number = 3;
    add_bits(arr, 0, 3, 0b00000111);
    ASSERT(arr[0], 0b00111000);

    *(uint16_t*)arr = 0;
    bit_number = 3;
    add_bits(arr, 0, 6, 0b00000101);
    ASSERT(arr[0], 0b01000000);
    ASSERT(arr[1], 0b00000001);

    *(uint16_t*)arr = 0;
    bit_number = 3;
    add_bits(arr, 0, 7, 0b00000101);
    ASSERT(arr[0], 0b10000000);
    ASSERT(arr[1], 0b00000010);

}

static void run_tests() {
    int initial_bit_number = bit_number;

    TEST_add_bits();
    TEST_get_bits();
    
    printf("TESTS RAN\n");

    bit_number = initial_bit_number;
}
#endif