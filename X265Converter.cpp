#include "X265Converter.h"


#include <QApplication>
#include <QDebug>

extern int x265main(int argc,
                    char **argv,
                    std::function<int(char **data, ssize_t *bytes, int width, int height)> readRgb888,
                    std::function<int(const unsigned char *data, ssize_t bytes)> writeEncodedFrame,
                    std::atomic<bool> &killed
                    );
extern int dec265main(int argc,
                      char** argv,
                      HevcReader *hevcreader,
                      YuvWriter *yuvWriter,
                      std::atomic<bool> &die
                      );

namespace  {
QImage FromYuvImage(const uint8_t *yuv, size_t sz, int width, int height)
{
   QImage img(width, height, QImage::Format::Format_RGB888);
   int frameSize = width * height;
   int chromasize = frameSize;
   {
       int yIndex = 0;
       int uIndex = frameSize;
       int vIndex = frameSize + chromasize;
       int index = 0;
       int Y = 0, U = 0, V = 0;
       for (int j = 0; j < height; j++)
       {
           for (int i = 0; i < width; i++)
           {
               Y = yuv[yIndex++];
               U = yuv[uIndex++];
               V = yuv[vIndex++];
               index++;

               float R = Y + 1.402 * (V - 128);
               float G = Y - 0.344 * (U - 128) - 0.714 * (V - 128);
               float B = Y + 1.772 * (U - 128);

               if (R < 0){ R = 0; } if (G < 0){ G = 0; } if (B < 0){ B = 0; }
               if (R > 255 ){ R = 255; } if (G > 255) { G = 255; } if (B > 255) { B = 255; }

               img.setPixelColor(i, j, qRgb(R, G, B));
           }
       }
   }
   return img;
}
}//namespace

X265Encoder::X265Encoder(
        int width,
        int height,
        std::function<int(char **data, ssize_t *bytes, int width, int height, float quality_factor)> encoderInputFn,
        std::function<void(EncodedChunk enc)> encoderOutputFn
        )
{
    StartEncoderThread(width,height,encoderInputFn,encoderOutputFn);
}

X265Encoder::~X265Encoder()
{
    _die = true;
    if(_encoder_thread.valid()){_encoder_thread.wait();}
}

void X265Encoder::StartEncoderThread(
        int width,
        int height,
        std::function<int(char **data, ssize_t *bytes, int width, int height, float quality_factor)> encoderInputFn,
        std::function<void(EncodedChunk enc)> encoderOutputFn
)
{
    if(width == 0 || height == 0 || !encoderInputFn || !encoderOutputFn)
    { return; }

    _encoder_thread = std::async(std::launch::async, [this,width,height,encoderInputFn,encoderOutputFn](){
        pthread_setname_np(pthread_self(), "x265enc");
        constexpr int NUM_PARAMS = 11;
        static char args[NUM_PARAMS][64] = {"x265",
                                     "--input", "/dev/screen",
                                     "--input-res", "WIDTHxHEIGHT",
                                     "--fps", "1",
                                       "--preset", "placebo",
//                                     "--preset", "ultrafast",
//                                     "--tune", "psnr",
//                                     "--tune", "ssim",
//                                     "--tune", "fastdecode",
//                                     "--tune", "zerolatency",
                                     "--output", "buffer://"
                                    };
        char *argv[NUM_PARAMS] = {args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                                  args[10]//, args[11], args[12], args[13], args[14], args[15], args[16], args[17], args[18]
                                 };
        constexpr int RES = 4;
        char resolution[64] = {0};
        sprintf(resolution, "%ix%i", width, height);
        strcpy(argv[RES], resolution);
        //_img_quality_percent

        extern int optind;
        optind = 1;
        x265main(NUM_PARAMS, (char**)argv,
        [encoderInputFn](char **data, ssize_t *bytes, int width, int height){
            return encoderInputFn ? encoderInputFn(data, bytes, width, height, -1) : 0;
        },
        [encoderOutputFn,width,height](const unsigned char *data, ssize_t bytes){
            if(encoderOutputFn)
            {
                EncodedChunk enc((uint8_t*)data,
                                 bytes,
                                 width,
                                 height,
                                 nullptr);
                encoderOutputFn(enc);
            }
            return 0;
        },
        _die);
    });
}

X265Decoder::X265Decoder( ssize_t img_width, ssize_t img_height, std::function<void(const QImage &img)> onDecode )
    : _hevc([](){
        QApplication::processEvents();
    }, _die)
    , _yuv([img_width, img_height, onDecode](const uint8_t* data, size_t size){
    onDecode(FromYuvImage(
                data,
                size,
                img_width,
                img_height));
    })
    , _decoder_thread(StartDecoderThread())
{
}

X265Decoder::~X265Decoder()
{
    _die = true;
    if(_decoder_thread.valid()){_decoder_thread.wait();}
}
std::future<void> X265Decoder::StartDecoderThread()
{
    return std::async(std::launch::async, [this](){
        constexpr int NUM_PARAMS = 4;
        static char args[NUM_PARAMS][64] = {"dec265",
                                            "c:/temp/frames2.hevc",//not used
                                            "--output",
                                            "c:/temp/frames.yuv", //not used
                                    };
        char *argv[NUM_PARAMS] = {args[0], args[1], args[2], args[3]};
        extern int optind;
        optind = 1;
        dec265main(NUM_PARAMS, argv, &_hevc, &_yuv, _die);
     });
}

void X265Decoder::Decode(uint32_t ip,
                         ssize_t width,
                         ssize_t height,
                           const EncodedChunk &enc)
{//TODO: use _map to get frame and ip for decoder callback used in constructor
    _img_to_decode = enc;
    _hevc.receivedData((char*)enc._data, enc._size);
}

