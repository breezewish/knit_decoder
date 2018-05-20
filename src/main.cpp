#include <nan.h>
#include <assert.h>

#include "context.hpp"
#include "worker.hpp"

using namespace v8;

class Decoder : public Nan::ObjectWrap {
public:
    static NAN_MODULE_INIT(Initialize) {
        Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("Decoder").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(tpl, "init", Init);
        Nan::SetPrototypeMethod(tpl, "release", Release);
        Nan::SetPrototypeMethod(tpl, "decodeFrames", DecodeFrames);
        Nan::SetPrototypeMethod(tpl, "setCallback", SetCallback);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("Decoder").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
    }

private:
    static inline Nan::Persistent<Function> & constructor() {
        static Nan::Persistent<Function> constructor;
        return constructor;
    }

    static NAN_METHOD(New) {
        if (info.IsConstructCall()) {
            auto decoder = new Decoder();
            decoder->Wrap(info.This());
            info.GetReturnValue().Set(info.This());
        } else {
            const int argc = 0;
            Local<Value> argv[0] = {};
            Local<Function> cons = Nan::New(constructor());
            info.GetReturnValue().Set(Nan::NewInstance(cons, argc, argv).ToLocalChecked());
        }
    }

private:
    DecodeContext ctx;

    explicit Decoder() {}

    // init() -> bool
    static NAN_METHOD(Init) {
        auto decoder = Nan::ObjectWrap::Unwrap<Decoder>(info.Holder());
        assert(!decoder->ctx.init_done);

        auto result = decoder->ctx.init();
        if (!result) {
            decoder->ctx.release();
        }

        info.GetReturnValue().Set(result);
    }

    // release()
    // If there are in progress works, the release will be deferred.
    static NAN_METHOD(Release) {
        auto decoder = Nan::ObjectWrap::Unwrap<Decoder>(info.Holder());
        decoder->ctx.release();
    }

    // setCallback()
    static NAN_METHOD(SetCallback) {
        assert(info.Length() == 1);

        auto decoder = Nan::ObjectWrap::Unwrap<Decoder>(info.Holder());
        assert(decoder->ctx.init_done);
        assert(!decoder->ctx.pending_release);
        assert(!decoder->ctx.decode_in_progress);

        decoder->ctx.callback.Reset(info[0].As<Function>());
    }

    // decodeFrames(buffer, offset, length)
    //
    // Note: Although this function is async, only one task at a time is accepted.
    static NAN_METHOD(DecodeFrames) {
        assert(info.Length() == 3);
        assert(node::Buffer::HasInstance(info[0]));

        auto decoder = Nan::ObjectWrap::Unwrap<Decoder>(info.Holder());

        assert(decoder->ctx.init_done);
        assert(!decoder->ctx.pending_release);
        assert(!decoder->ctx.decode_in_progress);

        decoder->ctx.decode_in_progress = true;
        // `decode_in_progress` will be cleared in `DecodeWorker`.

        auto buffer = info[0]->ToObject();
        auto offset = info[1]->Uint32Value();
        auto length = info[2]->Uint32Value();
        assert(node::Buffer::IsWithinBounds(offset, length, node::Buffer::Length(buffer)));

        decoder->ctx.in_buffer_store.Reset(buffer);
        // `in_buffer_store` will be cleared in `DecodeWorker`.

        Nan::AsyncQueueWorker(new DecodeWorker(&(decoder->ctx),
                                               buffer,
                                               offset,
                                               length));
    }
};

NODE_MODULE(knit_decoder, Decoder::Initialize)
