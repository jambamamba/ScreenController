#include "JpegConverter.h"

#include <QDebug>
#include <QImage>
#include <csetjmp>
#include <jpeglib.h>

namespace  {

struct jpegErrorManager {
    /* "public" fields */
    struct jpeg_error_mgr pub;
    /* for return to caller */
    __jmp_buf_tag setjmp_buffer;
};
char jpegLastErrorMsg[JMSG_LENGTH_MAX];
void jpegOutputMessage (j_common_ptr cinfo)
{
    /* cinfo->err actually points to a jpegErrorManager struct */
    jpegErrorManager* myerr = (jpegErrorManager*) cinfo->err;
    /* note : *(cinfo->err) is now equivalent to myerr->pub */

    /* output_message is a method to print an error message */
    /*(* (cinfo->err->output_message) ) (cinfo);*/

    /* Create the message */
    ( *(cinfo->err->format_message) ) (cinfo, jpegLastErrorMsg);

    /* Jump to the setjmp point */
    longjmp(&myerr->setjmp_buffer, 1);

}
}//namespace


QImage JpegConverter::FromJpeg(uint8_t *jpg_buffer, size_t jpg_size, QImage &out_image)
{
    struct jpeg_decompress_struct cinfo;


//    cinfo.err = jpeg_std_error(&jerr);
    jpegErrorManager jerr;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.output_message = jpegOutputMessage;
    if (_setjmp(&jerr.setjmp_buffer))
    {
        qDebug() << "Error while decompressing JPEG " << jpegLastErrorMsg;
        jpeg_destroy_decompress(&cinfo);
        return out_image;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpg_buffer, jpg_size);
    int rc = jpeg_read_header(&cinfo, TRUE);
    if(rc == -1)
    {
        qDebug() << "jpeg_read_header failed";
        return out_image;
    }
    if(!jpeg_start_decompress(&cinfo))
    {
        qDebug() << "jpeg_start_decompress failed";
        return out_image;
    }
    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int pixel_size = cinfo.output_components;

    size_t bmp_size = width * height * pixel_size;
    uint8_t *bmp_buffer = (uint8_t*) malloc(bmp_size);
    int row_stride = width * pixel_size;

    while (cinfo.output_scanline < cinfo.output_height)
    {
        uint8_t *buffer_array[1];
        buffer_array[0] = bmp_buffer + (cinfo.output_scanline) * row_stride;

        rc = jpeg_read_scanlines(&cinfo, buffer_array, 1);
        if(rc == -1)
        {
            qDebug() << "jpeg_read_scanlines failed";
            return QImage();
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    if(out_image.isNull() || out_image.width() != width || out_image.height() != height)
    {
        out_image = QImage(width, height, QImage::Format::Format_RGB888);
    }
    memcpy(out_image.bits(), bmp_buffer, bmp_size);
    free(bmp_buffer);

    return out_image;
}

//https://www.ridgesolutions.ie/index.php/2019/12/10/libjpeg-example-encode-jpeg-to-memory-buffer-instead-of-file/
// Encodes a 32bpp image to JPEG directly to a memory buffer
// libJEPG will malloc() the buffer so the caller must free() it when
// they are finished with it.
//
// image    - the input 32 bpp bgra image, 4 byte is 1 pixel.
// width    - the width of the input image
// height   - the height of the input image
// quality  - target JPEG 'quality' factor (max 100)
// comment  - optional JPEG NULL-termoinated comment, pass NULL for no comment.
// jpegSize - output, the number of bytes in the output JPEG buffer
// jpegBuf  - output, a pointer to the output JPEG buffer, must call free() when finished with it.
//
void JpegConverter::ToJpeg(unsigned char* image, int width, int height, int quality,
            const char* comment, unsigned long* jpegSize, unsigned char** jpegBuf)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPROW row_pointer[1];
    int bytesperpixel = 4;
    int row_stride;

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);
    cinfo.image_width = width;
    cinfo.image_height = height;

    // Input is greyscale, 1 byte per pixel
    cinfo.input_components = bytesperpixel;
    cinfo.in_color_space = JCS_EXT_BGRX;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    //
    //
    // Tell libJpeg to encode to memory, this is the bit that's different!
    // Lib will alloc buffer.
    //
    jpeg_mem_dest(&cinfo, jpegBuf, jpegSize);

    jpeg_start_compress(&cinfo, TRUE);

    // Add comment section if any..
    if (comment) {
        jpeg_write_marker(&cinfo, JPEG_COM, (const JOCTET*)comment, strlen(comment));
    }

    // 1 BPP
    row_stride = width * bytesperpixel;

    // Encode
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &image[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
}

ssize_t JpegConverter::FindJpegHeader(uint8_t* buffer, ssize_t buffer_tail)
{
    ssize_t idx = 0;
    for(; idx < buffer_tail - 10; ++idx)
    {
//jpeg header                ff d8 ff e0 00 10 4a 46  49 46
        if(buffer[idx] == 0xff &&
                buffer[idx+1] == 0xd8 &&
                buffer[idx+2] == 0xff &&
                buffer[idx+3] == 0xe0 &&
                buffer[idx+4] == 0x00 &&
                buffer[idx+5] == 0x10 &&
                buffer[idx+6] == 0x4a &&
                buffer[idx+7] == 0x46 &&
                buffer[idx+8] == 0x49 &&
                buffer[idx+9] == 0x46
                )
        {
            return idx;
        }
    }
    return -1;
}

bool JpegConverter::ValidJpegFooter(uint8_t second_last_byte, uint8_t last_byte)
{
    return (second_last_byte == 0xff && last_byte == 0xd9);
}
