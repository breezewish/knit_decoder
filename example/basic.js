const Decoder = require('..');
const fs = require('fs');
const bufferSplit = require('buffer-split');

const data = fs.readFileSync('sample.h264');

let buffers = bufferSplit(data, new Buffer([0x00, 0x00, 0x00, 0x01]));
buffers = buffers.slice(1).map(buffer => {
    let prefix = new Buffer([0x00, 0x00, 0x00, 0x01]);
    let newBuffer = Buffer.concat([prefix, buffer]);
    return newBuffer;
});

let lastDecodeTime = Date.now();
let decoder = new Decoder();
decoder.on('frame', (buffer, width, height) => {
    let now = Date.now();
    console.log('decode cost %d ms', now - lastDecodeTime);
    lastDecodeTime = now;
});

buffers.forEach(buffer => decoder.decodeFrames(buffer, 0, buffer.length, false));
