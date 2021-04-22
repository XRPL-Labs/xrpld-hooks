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
const offerFlags = utils.common.txFlags.OfferCreate;
const common_1 = require("../common");
function createOrderTransaction(account, order) {
    const takerPays = utils.common.toRippledAmount(order.direction === 'buy' ? order.quantity : order.totalPrice);
    const takerGets = utils.common.toRippledAmount(order.direction === 'buy' ? order.totalPrice : order.quantity);
    const txJSON = {
        TransactionType: 'OfferCreate',
        Account: account,
        TakerGets: takerGets,
        TakerPays: takerPays,
        Flags: 0
    };
    if (order.direction === 'sell') {
        txJSON.Flags |= offerFlags.Sell;
    }
    if (order.passive === true) {
        txJSON.Flags |= offerFlags.Passive;
    }
    if (order.immediateOrCancel === true) {
        txJSON.Flags |= offerFlags.ImmediateOrCancel;
    }
    if (order.fillOrKill === true) {
        txJSON.Flags |= offerFlags.FillOrKill;
    }
    if (order.expirationTime !== undefined) {
        txJSON.Expiration = common_1.iso8601ToRippleTime(order.expirationTime);
    }
    if (order.orderToReplace !== undefined) {
        txJSON.OfferSequence = order.orderToReplace;
    }
    if (order.memos !== undefined) {
        txJSON.Memos = order.memos.map(utils.convertMemo);
    }
    return txJSON;
}
function prepareOrder(address, order, instructions = {}) {
    try {
        common_1.validate.prepareOrder({ address, order, instructions });
        const txJSON = createOrderTransaction(address, order);
        return utils.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = prepareOrder;
//# sourceMappingURL=order.js.map