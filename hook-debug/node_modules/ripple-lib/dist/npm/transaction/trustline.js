"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (Object.hasOwnProperty.call(mod, k)) result[k] = mod[k];
    result["default"] = mod;
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
const bignumber_js_1 = __importDefault(require("bignumber.js"));
const utils = __importStar(require("./utils"));
const validate = utils.common.validate;
const trustlineFlags = utils.common.txFlags.TrustSet;
function convertQuality(quality) {
    return new bignumber_js_1.default(quality)
        .shiftedBy(9)
        .integerValue(bignumber_js_1.default.ROUND_DOWN)
        .toNumber();
}
function createTrustlineTransaction(account, trustline) {
    const limit = {
        currency: trustline.currency,
        issuer: trustline.counterparty,
        value: trustline.limit
    };
    const txJSON = {
        TransactionType: 'TrustSet',
        Account: account,
        LimitAmount: limit,
        Flags: 0
    };
    if (trustline.qualityIn !== undefined) {
        txJSON.QualityIn = convertQuality(trustline.qualityIn);
    }
    if (trustline.qualityOut !== undefined) {
        txJSON.QualityOut = convertQuality(trustline.qualityOut);
    }
    if (trustline.authorized === true) {
        txJSON.Flags |= trustlineFlags.SetAuth;
    }
    if (trustline.ripplingDisabled !== undefined) {
        txJSON.Flags |= trustline.ripplingDisabled
            ? trustlineFlags.NoRipple
            : trustlineFlags.ClearNoRipple;
    }
    if (trustline.frozen !== undefined) {
        txJSON.Flags |= trustline.frozen
            ? trustlineFlags.SetFreeze
            : trustlineFlags.ClearFreeze;
    }
    if (trustline.memos !== undefined) {
        txJSON.Memos = trustline.memos.map(utils.convertMemo);
    }
    return txJSON;
}
function prepareTrustline(address, trustline, instructions = {}) {
    try {
        validate.prepareTrustline({ address, trustline, instructions });
        const txJSON = createTrustlineTransaction(address, trustline);
        return utils.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = prepareTrustline;
//# sourceMappingURL=trustline.js.map