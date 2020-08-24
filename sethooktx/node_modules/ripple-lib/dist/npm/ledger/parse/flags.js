"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const orderFlags = {
    Passive: 0x00010000,
    Sell: 0x00020000
};
exports.orderFlags = orderFlags;
const trustlineFlags = {
    LowReserve: 0x00010000,
    HighReserve: 0x00020000,
    LowAuth: 0x00040000,
    HighAuth: 0x00080000,
    LowNoRipple: 0x00100000,
    HighNoRipple: 0x00200000,
    LowFreeze: 0x00400000,
    HighFreeze: 0x00800000
};
exports.trustlineFlags = trustlineFlags;
//# sourceMappingURL=flags.js.map