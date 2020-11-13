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
const assert = __importStar(require("assert"));
const common_1 = require("../../common");
const amount_1 = __importDefault(require("./amount"));
function parseCheckCash(tx) {
    assert.ok(tx.TransactionType === 'CheckCash');
    return common_1.removeUndefined({
        checkID: tx.CheckID,
        amount: tx.Amount && amount_1.default(tx.Amount),
        deliverMin: tx.DeliverMin && amount_1.default(tx.DeliverMin)
    });
}
exports.default = parseCheckCash;
//# sourceMappingURL=check-cash.js.map