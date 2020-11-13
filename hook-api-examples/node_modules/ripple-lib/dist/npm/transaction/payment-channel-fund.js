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
const common_1 = require("../common");
function createPaymentChannelFundTransaction(account, fund) {
    const txJSON = {
        Account: account,
        TransactionType: 'PaymentChannelFund',
        Channel: fund.channel,
        Amount: common_1.xrpToDrops(fund.amount)
    };
    if (fund.expiration !== undefined) {
        txJSON.Expiration = common_1.iso8601ToRippleTime(fund.expiration);
    }
    return txJSON;
}
function preparePaymentChannelFund(address, paymentChannelFund, instructions = {}) {
    try {
        common_1.validate.preparePaymentChannelFund({
            address,
            paymentChannelFund,
            instructions
        });
        const txJSON = createPaymentChannelFundTransaction(address, paymentChannelFund);
        return utils.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = preparePaymentChannelFund;
//# sourceMappingURL=payment-channel-fund.js.map