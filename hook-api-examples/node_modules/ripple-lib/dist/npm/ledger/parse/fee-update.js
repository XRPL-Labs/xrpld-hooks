"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const bignumber_js_1 = __importDefault(require("bignumber.js"));
const common_1 = require("../../common");
function parseFeeUpdate(tx) {
    const baseFeeDrops = new bignumber_js_1.default(tx.BaseFee, 16).toString();
    return {
        baseFeeXRP: common_1.dropsToXrp(baseFeeDrops),
        referenceFeeUnits: tx.ReferenceFeeUnits,
        reserveBaseXRP: common_1.dropsToXrp(tx.ReserveBase),
        reserveIncrementXRP: common_1.dropsToXrp(tx.ReserveIncrement)
    };
}
exports.default = parseFeeUpdate;
//# sourceMappingURL=fee-update.js.map