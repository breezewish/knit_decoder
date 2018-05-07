#ifndef KNIT_DECODER_WORKER
#define KNIT_DECODER_WORKER

#include <string>

#include <nan.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/avutil.h>
    #include <libavutil/imgutils.h>
}

#include "context.hpp"

using namespace v8;

class DecodeWorker : public Nan::AsyncWorker {
private:
    DecodeContext *context;
    uint8_t *input_buffer;
    uint32_t input_length;
    bool has_decode_output = false;
public:
    // `input_buffer` must contain 32 byte paddings.
    DecodeWorker(DecodeContext *context,
                 Local<Object> &buffer,
                 uint32_t offset,
                 uint32_t length): Nan::AsyncWorker(nullptr) {
        this->context = context;
        this->input_buffer = reinterpret_cast<uint8_t *>(node::Buffer::Data(buffer)) + offset;
        this->input_length = length;
    }
    
    void Execute() {
        decode_frames();
    }
    
    /**
     * Decode one or more frames.
     */
    bool decode_frames() {
        uint8_t *cur_buf = input_buffer;
        int remain_buf_size = input_length;
        
        uint8_t *data = nullptr;
        int size = 0;
        
        while (remain_buf_size) {
            int len = av_parser_parse2(context->parser, context->codec, &data, &size, cur_buf, remain_buf_size, 0, 0, AV_NOPTS_VALUE);
            cur_buf += len;
            remain_buf_size -= len;
            if (size > 0) {
                if (!decode_frame(data, size)) {
                    return false;
                }
            } else {
                if (remain_buf_size > 0) {
                    SetErrorMessage("Failed to decode frame: Incomplete frame");
                    return false;
                }
            }
        }
        
        return true;
    }
    
    /**
     * Decode single frame.
     */
    bool decode_frame(uint8_t *buffer, int buffer_size) {
        AVPacket packet;
        av_init_packet(&packet);
        packet.data = buffer;
        packet.size = buffer_size;
        
        int ret;
        
        has_decode_output = false;
        
        ret = avcodec_send_packet(context->codec, &packet);
        if (ret != 0) {
            SetErrorMessage(std::string("Failed to decode frame: avcodec_send_packet: ").append(av_err2str(ret)).data());
            return false;
        }
        
        while (true) {
            ret = avcodec_receive_frame(context->codec, context->frame_temp);
            if (ret == 0) {
                // Successfully decoded a frame. Let's transform the output color format.
                // TODO: We only need to transform the last frame.
                context->ensure_output_frame(
                                             context->codec->width,
                                             context->codec->height,
                                             context->codec->pix_fmt);
                sws_scale(
                          context->sws_context,
                          context->frame_temp->data,
                          context->frame_temp->linesize,
                          0,
                          context->frame_height,
                          context->frame_output->data,
                          context->frame_output->linesize);
                has_decode_output = true;
            } else if (ret == AVERROR(EAGAIN)) {
                // Output is drained.
                break;
            } else {
                SetErrorMessage(std::string("Failed to decode frame: avcodec_receive_frame: ").append(av_err2str(ret)).data());
                return false;
            }
        }
        
        return true;
    }
    
    void WorkComplete() {
        context->decode_in_progress = false;
        context->in_buffer_store.Reset();
        AsyncWorker::WorkComplete();
        if (context->pending_release) {
            context->release();
        }
    }
    
    void HandleOKCallback() {
        Nan::HandleScope scope;
        if (has_decode_output) {
            if (context->pending_out_buffer_change) {
                // There is pending `out_buffer` change during decoding.
                auto output_buffer = Nan::NewBuffer(reinterpret_cast<char *>(context->frame_output->data[0]),
                                                    context->frame_output_size,
                                                    [](char *, void * hint) {
                                                        // Don't free memory when Buffer is freed.
                                                    },
                                                    nullptr);
                context->out_buffer_store.Reset(output_buffer.ToLocalChecked());
                context->pending_out_buffer_change = false;
            }
            Local<Value> argv[] = {
                Nan::Null(),
                Nan::New(context->out_buffer_store),
                Nan::New(context->frame_width),
                Nan::New(context->frame_height),
            };
            context->callback.Call(3, argv, async_resource);
        } else {
            // Even if there is no decode output, we still need to call the callback
            // to inform that the async task is done.
            Local<Value> argv[] = { Nan::Null() };
            context->callback.Call(1, argv, async_resource);
        }
    }
    
    void HandleErrorCallback() {
        Nan::HandleScope scope;
        Local<Value> argv[] = { Nan::New(this->ErrorMessage()).ToLocalChecked() };
        context->callback.Call(1, argv, async_resource);
    }
};

#endif
