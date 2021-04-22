"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const ripple_keypairs_1 = require("ripple-keypairs");
exports.deriveKeypair = ripple_keypairs_1.deriveKeypair;
exports.deriveAddress = ripple_keypairs_1.deriveAddress;
const ripple_address_codec_1 = require("ripple-address-codec");
function deriveXAddress(options) {
    const classicAddress = ripple_keypairs_1.deriveAddress(options.publicKey);
    return ripple_address_codec_1.classicAddressToXAddress(classicAddress, options.tag, options.test);
}
exports.deriveXAddress = deriveXAddress;
//# sourceMappingURL=derive.js.map