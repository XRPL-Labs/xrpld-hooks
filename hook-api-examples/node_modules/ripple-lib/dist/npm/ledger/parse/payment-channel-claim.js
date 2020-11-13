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
const assert = __importStar(require("assert"));
const common_1 = require("../../common");
const amount_1 = __importDefault(require("./amount"));
const claimFlags = common_1.txFlags.PaymentChannelClaim;
function parsePaymentChannelClaim(tx) {
    assert.ok(tx.TransactionType === 'PaymentChannelClaim');
    return common_1.removeUndefined({
        channel: tx.Channel,
        balance: tx.Balance && amount_1.default(tx.Balance).value,
        amount: tx.Amount && amount_1.default(tx.Amount).value,
        signature: tx.Signature,
        publicKey: tx.PublicKey,
        renew: Boolean(tx.Flags & claimFlags.Renew) || undefined,
        close: Boolean(tx.Flags & claimFlags.Close) || undefined
    });
}
exports.default = parsePaymentChannelClaim;
//# sourceMappingURL=payment-channel-claim.js.map