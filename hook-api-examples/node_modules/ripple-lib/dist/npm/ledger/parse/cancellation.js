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
function parseOrderCancellation(tx) {
    assert.ok(tx.TransactionType === 'OfferCancel');
    return {
        orderSequence: tx.OfferSequence
    };
}
exports.default = parseOrderCancellation;
//# sourceMappingURL=cancellation.js.map