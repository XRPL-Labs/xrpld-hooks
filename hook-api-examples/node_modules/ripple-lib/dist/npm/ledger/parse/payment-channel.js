"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const utils_1 = require("./utils");
const common_1 = require("../../common");
function parsePaymentChannel(data) {
    return common_1.removeUndefined({
        account: data.Account,
        amount: common_1.dropsToXrp(data.Amount),
        balance: common_1.dropsToXrp(data.Balance),
        destination: data.Destination,
        publicKey: data.PublicKey,
        settleDelay: data.SettleDelay,
        expiration: utils_1.parseTimestamp(data.Expiration),
        cancelAfter: utils_1.parseTimestamp(data.CancelAfter),
        sourceTag: data.SourceTag,
        destinationTag: data.DestinationTag,
        previousAffectingTransactionID: data.PreviousTxnID,
        previousAffectingTransactionLedgerVersion: data.PreviousTxnLgrSeq
    });
}
exports.parsePaymentChannel = parsePaymentChannel;
//# sourceMappingURL=payment-channel.js.map