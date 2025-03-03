#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <jpeglib.h>
#include <fftw3.h>

typedef struct {
    unsigned char *data;
    int width;
    int height;
    int channels;
} Image;

// Function to load a JPEG image
Image load_jpeg(const char *filename) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *infile;
    JSAMPARRAY buffer;
    int row_stride;
    Image img = {0};
    
    if ((infile = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "Can't open %s\n", filename);
        return img;
    }
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    
    img.width = cinfo.output_width;
    img.height = cinfo.output_height;
    img.channels = cinfo.output_components;
    img.data = (unsigned char *)malloc(img.width * img.height * img.channels);
    
    row_stride = img.width * img.channels;
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);
    
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(img.data + (cinfo.output_scanline - 1) * row_stride, buffer[0], row_stride);
    }
    
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    
    return img;
}

// Function to save a grayscale image
void save_grayscale_image(const char *filename, unsigned char *data, int width, int height) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *outfile;
    JSAMPARRAY buffer;
    
    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "Can't open %s\n", filename);
        return;
    }
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);
    
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;
    
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, width, 1);
    
    while (cinfo.next_scanline < cinfo.image_height) {
        memcpy(buffer[0], data + cinfo.next_scanline * width, width);
        jpeg_write_scanlines(&cinfo, buffer, 1);
    }
    
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);
    
    printf("Saved %s\n", filename);
}

// Function to shift Hartley transform
void hartley_shift(double *data, double *shifted, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Calculate destination coordinates after shift
            int dst_y = (y + height/2) % height;
            int dst_x = (x + width/2) % width;
            
            // Copy data with shift
            shifted[dst_y * width + dst_x] = data[y * width + x];
        }
    }
}

// Function to inverse shift Hartley transform
void hartley_ishift(double *shifted, double *data, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Calculate source coordinates before shift
            int src_y = (y + height/2) % height;
            int src_x = (x + width/2) % width;
            
            // Copy data with inverse shift
            data[y * width + x] = shifted[src_y * width + src_x];
        }
    }
}

// Function to apply square low-pass filter to shifted Hartley transform
void apply_square_filter(double *shifted, int width, int height, int cutoff_size) {
    int center_x = width / 2;
    int center_y = height / 2;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Calculate distance from center (maximum of x and y components)
            int dx = abs(x - center_x);
            int dy = abs(y - center_y);
            
            // Apply square filter - keep only frequencies within square centered at DC
            if (dx > cutoff_size || dy > cutoff_size) {
                shifted[y * width + x] = 0.0;
            }
        }
    }
}

// Function to save visualization of Hartley transform
void save_visualization(const char *filename, double *data, int width, int height, int is_filtered) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *outfile;
    JSAMPARRAY buffer;
    
    // Create a copy of the data for visualization
    double *vis_data = (double *)malloc(width * height * sizeof(double));
    memcpy(vis_data, data, width * height * sizeof(double));
    
    // For visualization, we'll use logarithmic scaling
    double min_val = vis_data[0];
    double max_val = vis_data[0];
    for (int i = 1; i < width * height; i++) {
        if (vis_data[i] < min_val) min_val = vis_data[i];
        if (vis_data[i] > max_val) max_val = vis_data[i];
    }
    
    // Apply log scaling: log(1 + |x|), preserving signs
    for (int i = 0; i < width * height; i++) {
        double sign = (vis_data[i] >= 0) ? 1.0 : -1.0;
        vis_data[i] = sign * log(1.0 + fabs(vis_data[i]));
    }
    
    // Find new min/max after scaling
    min_val = vis_data[0];
    max_val = vis_data[0];
    for (int i = 1; i < width * height; i++) {
        if (vis_data[i] < min_val) min_val = vis_data[i];
        if (vis_data[i] > max_val) max_val = vis_data[i];
    }
    
    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "Can't open %s\n", filename);
        free(vis_data);
        return;
    }
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);
    
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;
    
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, width, 1);
    
    while (cinfo.next_scanline < cinfo.image_height) {
        for (int i = 0; i < width; i++) {
            // Normalize to 0-255 range
            double normalized = (vis_data[cinfo.next_scanline * width + i] - min_val) / (max_val - min_val);
            buffer[0][i] = (JSAMPLE)(normalized * 255.0);
        }
        jpeg_write_scanlines(&cinfo, buffer, 1);
    }
    
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);
    free(vis_data);
    
    printf("Saved %s (Hartley transform %s)\n", 
           filename, is_filtered ? "filtered" : "unfiltered");
}

// Function to compute the 2D Hartley Transform
double* compute_hartley_transform(unsigned char *grayscale, int width, int height) {
    // Allocate memory for FFTW
    double *input = (double *)fftw_malloc(sizeof(double) * width * height);
    double *hartley = (double *)fftw_malloc(sizeof(double) * width * height);
    
    // Copy grayscale image to double array for processing
    for (int i = 0; i < width * height; i++) {
        input[i] = (double)grayscale[i];
    }
    
    // Use r2r (real-to-real) transform with DHT (discrete Hartley transform)
    fftw_plan plan = fftw_plan_r2r_2d(height, width, input, hartley, FFTW_DHT, FFTW_DHT, FFTW_ESTIMATE);
    fftw_execute(plan);
    
    // FFTW computes an unnormalized transform, so normalize by sqrt(N)
    double norm_factor = 1.0 / sqrt(width * height);
    for (int i = 0; i < width * height; i++) {
        hartley[i] *= norm_factor;
    }
    
    // Clean up FFTW resources
    fftw_destroy_plan(plan);
    fftw_free(input);
    
    // Return the result
    return hartley;
}

// Function to compute the inverse 2D Hartley Transform
unsigned char* inverse_hartley_transform(double *hartley, int width, int height) {
    // Allocate memory for FFTW
    double *output = (double *)fftw_malloc(sizeof(double) * width * height);
    unsigned char *result = (unsigned char *)malloc(width * height);
    
    // Create FFTW plan for inverse DHT
    // DHT is its own inverse, just need to scale afterward
    fftw_plan plan = fftw_plan_r2r_2d(height, width, hartley, output, FFTW_DHT, FFTW_DHT, FFTW_ESTIMATE);
    fftw_execute(plan);
    
    // Normalize by 1/N for inverse transform
    double norm_factor = 1.0 / (width * height);
    
    // Convert back to unsigned char with proper scaling
    double min_val = output[0] * norm_factor;
    double max_val = min_val;
    
    // Find min/max values
    for (int i = 1; i < width * height; i++) {
        double val = output[i] * norm_factor;
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }
    
    // Scale and convert to unsigned char
    for (int i = 0; i < width * height; i++) {
        // Normalize to 0-255 range if needed
        if (max_val > 255 || min_val < 0) {
            double normalized = (output[i] * norm_factor - min_val) / (max_val - min_val);
            result[i] = (unsigned char)(normalized * 255.0);
        } else {
            // Just clip values
            double val = output[i] * norm_factor;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            result[i] = (unsigned char)val;
        }
    }
    
    // Clean up FFTW resources
    fftw_destroy_plan(plan);
    fftw_free(output);
    
    return result;
}

// Convert color image to grayscale
unsigned char* convert_to_grayscale(Image img) {
    unsigned char *grayscale = (unsigned char *)malloc(img.width * img.height);
    
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            int idx = (y * img.width + x) * img.channels;
            
            if (img.channels >= 3) {
                // Standard luminance formula
                grayscale[y * img.width + x] = (unsigned char)(
                    0.299 * img.data[idx] +      // Red
                    0.587 * img.data[idx + 1] +  // Green
                    0.114 * img.data[idx + 2]);  // Blue
            } else {
                // Already grayscale
                grayscale[y * img.width + x] = img.data[idx];
            }
        }
    }
    
    return grayscale;
}

int main() {
    // Load the image
    Image img = load_jpeg("img.jpg");
    if (img.data == NULL) {
        fprintf(stderr, "Failed to load image\n");
        return 1;
    }
    
    printf("Image loaded: %d x %d with %d channels\n", img.width, img.height, img.channels);
    
    // Convert to grayscale
    unsigned char *grayscale = convert_to_grayscale(img);
    
    // Save the grayscale image
    save_grayscale_image("grayscale.jpg", grayscale, img.width, img.height);
    
    // Compute the Hartley transform
    double *hartley_data = compute_hartley_transform(grayscale, img.width, img.height);
    
    // Create shifted version of the transform
    double *shifted_data = (double *)fftw_malloc(sizeof(double) * img.width * img.height);
    hartley_shift(hartley_data, shifted_data, img.width, img.height);
    
    // Save the shifted Hartley transform
    save_visualization("hartley.jpg", shifted_data, img.width, img.height, 0);
    
    // Apply square low-pass filter (filter size is 25% of the image width)
    int cutoff_size = img.width * 0.25;
    printf("Applying square low-pass filter with half-width %d pixels\n", cutoff_size);
    apply_square_filter(shifted_data, img.width, img.height, cutoff_size);
    
    // Save the filtered (still shifted) Hartley transform
    save_visualization("hartley_filtered.jpg", shifted_data, img.width, img.height, 1);
    
    // Inverse shift the filtered data back to original arrangement
    double *filtered_hartley = (double *)fftw_malloc(sizeof(double) * img.width * img.height);
    hartley_ishift(shifted_data, filtered_hartley, img.width, img.height);
    
    // Compute inverse Hartley transform to get filtered image
    unsigned char *filtered_image = inverse_hartley_transform(filtered_hartley, img.width, img.height);
    
    // Save the filtered image
    save_grayscale_image("img_filtered.jpg", filtered_image, img.width, img.height);
    
    // Clean up
    free(img.data);
    free(grayscale);
    free(filtered_image);
    fftw_free(hartley_data);
    fftw_free(shifted_data);
    fftw_free(filtered_hartley);
    
    return 0;
}