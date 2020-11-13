"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const bignumber_js_1 = __importDefault(require("bignumber.js"));
const amount_1 = __importDefault(require("./amount"));
const utils_1 = require("./utils");
const common_1 = require("../../common");
const flags_1 = require("./flags");
function computeQuality(takerGets, takerPays) {
    const quotient = new bignumber_js_1.default(takerPays.value).dividedBy(takerGets.value);
    return quotient.precision(16, bignumber_js_1.default.ROUND_HALF_UP).toString();
}
function parseAccountOrder(address, order) {
    const direction = (order.flags & flags_1.orderFlags.Sell) === 0 ? 'buy' : 'sell';
    const takerGetsAmount = amount_1.default(order.taker_gets);
    const takerPaysAmount = amount_1.default(order.taker_pays);
    const quantity = direction === 'buy' ? takerPaysAmount : takerGetsAmount;
    const totalPrice = direction === 'buy' ? takerGetsAmount : takerPaysAmount;
    const specification = common_1.removeUndefined({
        direction: direction,
        quantity: quantity,
        totalPrice: totalPrice,
        passive: (order.flags & flags_1.orderFlags.Passive) !== 0 || undefined,
        expirationTime: utils_1.parseTimestamp(order.expiration)
    });
    const makerExchangeRate = order.quality
        ? utils_1.adjustQualityForXRP(order.quality.toString(), takerGetsAmount.currency, takerPaysAmount.currency)
        : computeQuality(takerGetsAmount, takerPaysAmount);
    const properties = {
        maker: address,
        sequence: order.seq,
        makerExchangeRate: makerExchangeRate
    };
    return { specification, properties };
}
exports.parseAccountOrder = parseAccountOrder;
//# sourceMappingURL=account-order.js.map