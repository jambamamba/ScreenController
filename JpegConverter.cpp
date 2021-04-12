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

JpegConverter::JpegConverter()
    : ImageConverterInterface()
{}
JpegConverter::~JpegConverter()
{}


QImage JpegConverter::Decode(const EncodedChunk &enc)
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
        return QImage();
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, enc._data, enc._size);
    int rc = jpeg_read_header(&cinfo, TRUE);
    if(rc == -1)
    {
        qDebug() << "jpeg_read_header failed";
        return QImage();
    }
    if(!jpeg_start_decompress(&cinfo))
    {
        qDebug() << "jpeg_start_decompress failed";
        return QImage();
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

    QImage img(width, height, QImage::Format::Format_RGB888);
    memcpy(img.bits(), bmp_buffer, bmp_size);
    free(bmp_buffer);

    return img;
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
EncodedChunk JpegConverter::Encode(const uint8_t* image, ssize_t width, ssize_t height, float quality_factor)
//void JpegConverter::ToJpeg(unsigned char* image, int width, int height, int quality,
//            const char* comment, unsigned long* jpegSize, unsigned char** jpegBuf)
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
    cinfo.in_color_space = JCS_EXT_BGR;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, (int)quality_factor, TRUE);

    //
    //
    // Tell libJpeg to encode to memory, this is the bit that's different!
    // Lib will alloc buffer.
    //
    EncodedChunk enc(width, height, [](uint8_t* data){
        if(data) { free(data); }
    });
    jpeg_mem_dest(&cinfo, &enc._data, &enc._size);

    jpeg_start_compress(&cinfo, TRUE);

    // Add comment section if any..
//    if (comment) {
//        jpeg_write_marker(&cinfo, JPEG_COM, (const JOCTET*)comment, strlen(comment));
//    }

    // 1 BPP
    row_stride = width * bytesperpixel;

    // Encode
    while (cinfo.next_scanline < cinfo.image_height)
    {
        row_pointer[0] = (unsigned char*)&image[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return enc;
}

ssize_t JpegConverter::FindHeader(uint8_t* buffer, ssize_t buffer_sz)
{
    ssize_t idx = 0;
    for(; idx < buffer_sz - HeaderSize(); ++idx)
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

bool JpegConverter::IsValid(uint8_t* buffer, ssize_t buffer_sz)
{
    return (buffer[buffer_sz - 2] == 0xff &&
            buffer[buffer_sz - 1] == 0xd9);
}

ssize_t JpegConverter::HeaderSize() const
{
    return 10;
}
