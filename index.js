const addon = require('bindings')('knit_decoder');
const EventEmitter = require('events').EventEmitter;
const util = require('util');
const queue = require('async.queue');

class Decoder extends EventEmitter {
    constructor() {
        super();
        this._decoder = new addon.Decoder();
        if (!this._decoder.init()) {
            throw new Error("Failed to initialize the decoder library");
        }
        this._decoder.setCallback(this._handleDecodeCallback.bind(this));
        this._queue = queue(this._handleQueueTask.bind(this), 1);
    }

    _handleDecodeCallback(err, buffer, width, height) {
        if (err) {
            this.emit('error', err);
        } else {
            if (buffer != null) {
                this.emit('frame', buffer, width, height);
            }
        }
        this._queueCallback();
    }

    _handleQueueTask(task, callback) {
        this._queueCallback = callback;
        this._decoder.decodeFrames(task.buffer, task.offset, task.length);
    }

    decodeFrames(buffer, offset, length) {
        if (!buffer instanceof Buffer) {
            throw new Error('Expect buffer');
        }
        offset = ~~offset;
        length = ~~length;
        if (offset + length > buffer.length) {
            throw new Error('Offset out of bound');
        }
        this._queue.push({buffer, offset, length});
    }

    release() {
        this._decoder.release();
    }
}

module.exports = Decoder;
