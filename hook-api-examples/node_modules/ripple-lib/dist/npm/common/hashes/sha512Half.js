"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const crypto_1 = require("crypto");
const sha512Half = (hex) => {
    return crypto_1.createHash('sha512')
        .update(Buffer.from(hex, 'hex'))
        .digest('hex')
        .toUpperCase()
        .slice(0, 64);
};
exports.default = sha512Half;
//# sourceMappingURL=sha512Half.js.map