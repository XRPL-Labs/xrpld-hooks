"use strict";
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (Object.hasOwnProperty.call(mod, k)) result[k] = mod[k];
    result["default"] = mod;
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
const utils = __importStar(require("./utils"));
const ValidationError = utils.common.errors.ValidationError;
const claimFlags = utils.common.txFlags.PaymentChannelClaim;
const common_1 = require("../common");
function createPaymentChannelClaimTransaction(account, claim) {
    const txJSON = {
        Account: account,
        TransactionType: 'PaymentChannelClaim',
        Channel: claim.channel,
        Flags: 0
    };
    if (claim.balance !== undefined) {
        txJSON.Balance = common_1.xrpToDrops(claim.balance);
    }
    if (claim.amount !== undefined) {
        txJSON.Amount = common_1.xrpToDrops(claim.amount);
    }
    if (Boolean(claim.signature) !== Boolean(claim.publicKey)) {
        throw new ValidationError('"signature" and "publicKey" fields on' +
            ' PaymentChannelClaim must only be specified together.');
    }
    if (claim.signature !== undefined) {
        txJSON.Signature = claim.signature;
    }
    if (claim.publicKey !== undefined) {
        txJSON.PublicKey = claim.publicKey;
    }
    if (claim.renew === true && claim.close === true) {
        throw new ValidationError('"renew" and "close" flags on PaymentChannelClaim' +
            ' are mutually exclusive');
    }
    if (claim.renew === true) {
        txJSON.Flags |= claimFlags.Renew;
    }
    if (claim.close === true) {
        txJSON.Flags |= claimFlags.Close;
    }
    return txJSON;
}
function preparePaymentChannelClaim(address, paymentChannelClaim, instructions = {}) {
    try {
        common_1.validate.preparePaymentChannelClaim({
            address,
            paymentChannelClaim,
            instructions
        });
        const txJSON = createPaymentChannelClaimTransaction(address, paymentChannelClaim);
        return utils.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = preparePaymentChannelClaim;
//# sourceMappingURL=payment-channel-claim.js.map