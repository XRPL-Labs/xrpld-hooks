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
function createPaymentChannelCreateTransaction(account, paymentChannel) {
    const txJSON = {
        Account: account,
        TransactionType: 'PaymentChannelCreate',
        Amount: common_1.xrpToDrops(paymentChannel.amount),
        Destination: paymentChannel.destination,
        SettleDelay: paymentChannel.settleDelay,
        PublicKey: paymentChannel.publicKey.toUpperCase()
    };
    if (paymentChannel.cancelAfter !== undefined) {
        txJSON.CancelAfter = common_1.iso8601ToRippleTime(paymentChannel.cancelAfter);
    }
    if (paymentChannel.sourceTag !== undefined) {
        txJSON.SourceTag = paymentChannel.sourceTag;
    }
    if (paymentChannel.destinationTag !== undefined) {
        txJSON.DestinationTag = paymentChannel.destinationTag;
    }
    return txJSON;
}
function preparePaymentChannelCreate(address, paymentChannelCreate, instructions = {}) {
    try {
        common_1.validate.preparePaymentChannelCreate({
            address,
            paymentChannelCreate,
            instructions
        });
        const txJSON = createPaymentChannelCreateTransaction(address, paymentChannelCreate);
        return utils.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = preparePaymentChannelCreate;
//# sourceMappingURL=payment-channel-create.js.map