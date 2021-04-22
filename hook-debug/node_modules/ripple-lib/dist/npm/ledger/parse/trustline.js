"use strict";
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (Object.hasOwnProperty.call(mod, k)) result[k] = mod[k];
    result["default"] = mod;
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
const assert = __importStar(require("assert"));
const utils_1 = require("./utils");
const common_1 = require("../../common");
const flags = common_1.txFlags.TrustSet;
function parseFlag(flagsValue, trueValue, falseValue) {
    if (flagsValue & trueValue) {
        return true;
    }
    if (flagsValue & falseValue) {
        return false;
    }
    return undefined;
}
function parseTrustline(tx) {
    assert.ok(tx.TransactionType === 'TrustSet');
    return common_1.removeUndefined({
        limit: tx.LimitAmount.value,
        currency: tx.LimitAmount.currency,
        counterparty: tx.LimitAmount.issuer,
        memos: utils_1.parseMemos(tx),
        qualityIn: utils_1.parseQuality(tx.QualityIn),
        qualityOut: utils_1.parseQuality(tx.QualityOut),
        ripplingDisabled: parseFlag(tx.Flags, flags.SetNoRipple, flags.ClearNoRipple),
        frozen: parseFlag(tx.Flags, flags.SetFreeze, flags.ClearFreeze),
        authorized: parseFlag(tx.Flags, flags.SetAuth, 0)
    });
}
exports.default = parseTrustline;
//# sourceMappingURL=trustline.js.map