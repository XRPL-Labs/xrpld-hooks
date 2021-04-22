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
const validate = utils.common.validate;
function createOrderCancellationTransaction(account, orderCancellation) {
    const txJSON = {
        TransactionType: 'OfferCancel',
        Account: account,
        OfferSequence: orderCancellation.orderSequence
    };
    if (orderCancellation.memos !== undefined) {
        txJSON.Memos = orderCancellation.memos.map(utils.convertMemo);
    }
    return txJSON;
}
function prepareOrderCancellation(address, orderCancellation, instructions = {}) {
    try {
        validate.prepareOrderCancellation({
            address,
            orderCancellation,
            instructions
        });
        const txJSON = createOrderCancellationTransaction(address, orderCancellation);
        return utils.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = prepareOrderCancellation;
//# sourceMappingURL=ordercancellation.js.map