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
const _ = __importStar(require("lodash"));
const assert = __importStar(require("assert"));
const utils = __importStar(require("./utils"));
const common_1 = require("../../common");
const amount_1 = __importDefault(require("./amount"));
function isNoDirectRipple(tx) {
    return (tx.Flags & common_1.txFlags.Payment.NoRippleDirect) !== 0;
}
function isQualityLimited(tx) {
    return (tx.Flags & common_1.txFlags.Payment.LimitQuality) !== 0;
}
function removeGenericCounterparty(amount, address) {
    return amount.counterparty === address
        ? _.omit(amount, 'counterparty')
        : amount;
}
function parsePayment(tx) {
    assert.ok(tx.TransactionType === 'Payment');
    const source = {
        address: tx.Account,
        maxAmount: removeGenericCounterparty(amount_1.default(tx.SendMax || tx.Amount), tx.Account),
        tag: tx.SourceTag
    };
    const destination = {
        address: tx.Destination,
        tag: tx.DestinationTag
    };
    return common_1.removeUndefined({
        source: common_1.removeUndefined(source),
        destination: common_1.removeUndefined(destination),
        memos: utils.parseMemos(tx),
        invoiceID: tx.InvoiceID,
        paths: tx.Paths ? JSON.stringify(tx.Paths) : undefined,
        allowPartialPayment: utils.isPartialPayment(tx) || undefined,
        noDirectRipple: isNoDirectRipple(tx) || undefined,
        limitQuality: isQualityLimited(tx) || undefined
    });
}
exports.default = parsePayment;
//# sourceMappingURL=payment.js.map