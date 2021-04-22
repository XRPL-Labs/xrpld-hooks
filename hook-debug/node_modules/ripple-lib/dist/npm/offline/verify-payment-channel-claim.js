"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const ripple_keypairs_1 = __importDefault(require("ripple-keypairs"));
const ripple_binary_codec_1 = __importDefault(require("ripple-binary-codec"));
const common_1 = require("../common");
function verifyPaymentChannelClaim(channel, amount, signature, publicKey) {
    common_1.validate.verifyPaymentChannelClaim({ channel, amount, signature, publicKey });
    const signingData = ripple_binary_codec_1.default.encodeForSigningClaim({
        channel: channel,
        amount: common_1.xrpToDrops(amount)
    });
    return ripple_keypairs_1.default.verify(signingData, signature, publicKey);
}
exports.default = verifyPaymentChannelClaim;
//# sourceMappingURL=verify-payment-channel-claim.js.map