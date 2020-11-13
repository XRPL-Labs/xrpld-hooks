"use strict";
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (Object.hasOwnProperty.call(mod, k)) result[k] = mod[k];
    result["default"] = mod;
    return result;
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const common = __importStar(require("../common"));
const ripple_keypairs_1 = __importDefault(require("ripple-keypairs"));
const ripple_binary_codec_1 = __importDefault(require("ripple-binary-codec"));
const { validate, xrpToDrops } = common;
function signPaymentChannelClaim(channel, amount, privateKey) {
    validate.signPaymentChannelClaim({ channel, amount, privateKey });
    const signingData = ripple_binary_codec_1.default.encodeForSigningClaim({
        channel: channel,
        amount: xrpToDrops(amount)
    });
    return ripple_keypairs_1.default.sign(signingData, privateKey);
}
exports.default = signPaymentChannelClaim;
//# sourceMappingURL=sign-payment-channel-claim.js.map