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
const utils_1 = require("./utils");
const common_1 = require("../../common");
const flags_1 = require("./flags");
const amount_1 = __importDefault(require("./amount"));
function parseOrderbookOrder(data) {
    const direction = (data.Flags & flags_1.orderFlags.Sell) === 0 ? 'buy' : 'sell';
    const takerGetsAmount = amount_1.default(data.TakerGets);
    const takerPaysAmount = amount_1.default(data.TakerPays);
    const quantity = direction === 'buy' ? takerPaysAmount : takerGetsAmount;
    const totalPrice = direction === 'buy' ? takerGetsAmount : takerPaysAmount;
    const specification = common_1.removeUndefined({
        direction: direction,
        quantity: quantity,
        totalPrice: totalPrice,
        passive: (data.Flags & flags_1.orderFlags.Passive) !== 0 || undefined,
        expirationTime: utils_1.parseTimestamp(data.Expiration)
    });
    const properties = {
        maker: data.Account,
        sequence: data.Sequence,
        makerExchangeRate: utils_1.adjustQualityForXRP(data.quality, takerGetsAmount.currency, takerPaysAmount.currency)
    };
    const takerGetsFunded = data.taker_gets_funded
        ? amount_1.default(data.taker_gets_funded)
        : undefined;
    const takerPaysFunded = data.taker_pays_funded
        ? amount_1.default(data.taker_pays_funded)
        : undefined;
    const available = common_1.removeUndefined({
        fundedAmount: takerGetsFunded,
        priceOfFundedAmount: takerPaysFunded
    });
    const state = _.isEmpty(available) ? undefined : available;
    return common_1.removeUndefined({ specification, properties, state, data });
}
exports.parseOrderbookOrder = parseOrderbookOrder;
//# sourceMappingURL=orderbook-order.js.map