#ifndef KNIT_DECODER_CONTEXT
#define KNIT_DECODER_CONTEXT

#include <iostream>

#include <nan.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/avutil.h>
    #include <libavutil/imgutils.h>
}

using namespace v8;

const AVPixelFormat OUTPUT_FORMAT = AV_PIX_FMT_RGB24;

class DecodeContext {
public:
    bool init_done = false;
    bool decode_in_progress = false;
    bool pending_release = false;
    
    bool pending_out_buffer_change = false; // Are there any deferred `out_buffer` change? `out_buffer` cannot be re-allocated outside v8 context.
    Nan::Callback callback;
    Nan::Persistent<Object> out_buffer_store; // Persist until release. Internal buffer may be re-allocated according to video size.
    Nan::Persistent<Object> in_buffer_store; // Persist during decoding.
    
    AVCodecContext *codec = nullptr;
    AVCodecParserContext *parser = nullptr;
    
    // The buffer for codec output
    AVFrame *frame_temp = nullptr;
    
    // Save latest frame size. When frame size is changed, following members will be re-allocated.
    int frame_width = -1, frame_height = -1;
    AVPixelFormat frame_pixel_format = AV_PIX_FMT_NONE;
    SwsContext *sws_context = nullptr;
    AVFrame *frame_output = nullptr;
    int frame_output_size = 0;
    
    bool init() {
        if (init_done) {
            return true;
        }
        
        auto decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!decoder) {
            std::cerr << "[knit_decoder] Error: Failed to find H264 decoder" << std::endl;
            return false;
        }
        codec = avcodec_alloc_context3(decoder);
        if (!codec) {
            std::cerr << "[knit_decoder] Error: Failed to decoder context" << std::endl;
            return false;
        }
        if (avcodec_open2(codec, nullptr, nullptr) < 0) {
            std::cerr << "[knit_decoder] Error: Failed to open decoder" << std::endl;
            return false;
        }
        
        // configure for low delay
        codec->flags |= AV_CODEC_FLAG_LOW_DELAY;
        codec->flags2 |= AV_CODEC_FLAG2_FAST;
        
        parser = av_parser_init(AV_CODEC_ID_H264);
        if (!parser) {
            std::cerr << "[knit_decoder] Error: Failed to create H264 parser" << std::endl;
            return false;
        }
        
        frame_temp = av_frame_alloc();
        if (!frame_temp) {
            std::cerr << "[knit_decoder] Error: Failed to allocate decode temp frame" << std::endl;
            return false;
        }
        
        init_done = true;
        return true;
    }
    
    void release() {
        if (decode_in_progress) {
            pending_release = true;
            return;
        }
        
        pending_release = false;
        if (codec != nullptr) {
            avcodec_free_context(&codec);
        }
        if (parser != nullptr) {
            av_parser_close(parser);
            parser = nullptr;
        }
        if (frame_temp != nullptr) {
            av_frame_free(&frame_temp);
        }
        destroy_output_frame();
        
        init_done = false;
    }
    
    ~DecodeContext() {
        release();
    }
    
    /**
     * Ensure the output frame. If size is changed, new output frame will be allocated.
     * Otherwise previous one will be reused.
     */
    bool ensure_output_frame(int width, int height, AVPixelFormat pixel_format) {
        if (width != frame_width || height != frame_height || frame_pixel_format != pixel_format) {
            frame_width = width;
            frame_height = height;
            frame_pixel_format = pixel_format;
            return create_output_frame();
        } else {
            return true;
        }
    }
    
    /**
     * Create new output frame and associated sws contexts. Previous one will be destroyed if exists.
     */
    bool create_output_frame() {
        destroy_output_frame();
        sws_context = sws_getContext(
                                     frame_width, frame_height, frame_pixel_format,
                                     frame_width, frame_height, OUTPUT_FORMAT,
                                     SWS_POINT, nullptr, nullptr, nullptr);
        if (!sws_context) {
            std::cerr << "[knit_decoder] Error: Failed to create sws_context" << std::endl;
            return false;
        }
        frame_output = av_frame_alloc();
        if (!frame_output) {
            std::cerr << "[knit_decoder] Error: Failed to allocate output frame" << std::endl;
            return false;
        }
        int ret = av_image_alloc(frame_output->data, frame_output->linesize, frame_width, frame_height, OUTPUT_FORMAT, 1);
        if (ret < 0) {
            std::cerr << "[knit_decoder] Error: Failed to allocate image for output frame" << std::endl;
            return false;
        } else {
            frame_output_size = ret;
        }
        pending_out_buffer_change = true;
        
        return true;
    }
    
    /**
     * Destroy output frame and associated contexts if exists.
     */
    void destroy_output_frame() {
        out_buffer_store.Reset();
        if (sws_context != nullptr) {
            sws_freeContext(sws_context);
            sws_context = nullptr;
        }
        if (frame_output != nullptr) {
            av_freep(&frame_output->data[0]);
            av_frame_free(&frame_output);
        }
    }
};

#endif
