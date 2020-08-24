"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
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
const utils = __importStar(require("./utils"));
const orderbook_order_1 = require("./parse/orderbook-order");
const common_1 = require("../common");
const bignumber_js_1 = __importDefault(require("bignumber.js"));
function isSameIssue(a, b) {
    return a.currency === b.currency && a.counterparty === b.counterparty;
}
function directionFilter(direction, order) {
    return order.specification.direction === direction;
}
function flipOrder(order) {
    const specification = order.specification;
    const flippedSpecification = {
        quantity: specification.totalPrice,
        totalPrice: specification.quantity,
        direction: specification.direction === 'buy' ? 'sell' : 'buy'
    };
    const newSpecification = _.merge({}, specification, flippedSpecification);
    return _.merge({}, order, { specification: newSpecification });
}
function alignOrder(base, order) {
    const quantity = order.specification.quantity;
    return isSameIssue(quantity, base) ? order : flipOrder(order);
}
function formatBidsAndAsks(orderbook, offers) {
    const orders = offers
        .sort((a, b) => {
        return new bignumber_js_1.default(a.quality).comparedTo(b.quality);
    })
        .map(orderbook_order_1.parseOrderbookOrder);
    const alignedOrders = orders.map(_.partial(alignOrder, orderbook.base));
    const bids = alignedOrders.filter(_.partial(directionFilter, 'buy'));
    const asks = alignedOrders.filter(_.partial(directionFilter, 'sell'));
    return { bids, asks };
}
exports.formatBidsAndAsks = formatBidsAndAsks;
function makeRequest(api, taker, options, takerGets, takerPays) {
    return __awaiter(this, void 0, void 0, function* () {
        const orderData = utils.renameCounterpartyToIssuerInOrder({
            taker_gets: takerGets,
            taker_pays: takerPays
        });
        return api._requestAll('book_offers', {
            taker_gets: orderData.taker_gets,
            taker_pays: orderData.taker_pays,
            ledger_index: options.ledgerVersion || 'validated',
            limit: options.limit,
            taker
        });
    });
}
function getOrderbook(address, orderbook, options = {}) {
    return __awaiter(this, void 0, void 0, function* () {
        common_1.validate.getOrderbook({ address, orderbook, options });
        const [directOfferResults, reverseOfferResults] = yield Promise.all([
            makeRequest(this, address, options, orderbook.base, orderbook.counter),
            makeRequest(this, address, options, orderbook.counter, orderbook.base)
        ]);
        const directOffers = _.flatMap(directOfferResults, directOfferResult => directOfferResult.offers);
        const reverseOffers = _.flatMap(reverseOfferResults, reverseOfferResult => reverseOfferResult.offers);
        return formatBidsAndAsks(orderbook, [...directOffers, ...reverseOffers]);
    });
}
exports.getOrderbook = getOrderbook;
//# sourceMappingURL=orderbook.js.map