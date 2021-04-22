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
const amount_1 = __importDefault(require("./amount"));
const utils_1 = require("./utils");
const common_1 = require("../../common");
function parseEscrowCreation(tx) {
    assert.ok(tx.TransactionType === 'EscrowCreate');
    return common_1.removeUndefined({
        amount: amount_1.default(tx.Amount).value,
        destination: tx.Destination,
        memos: utils_1.parseMemos(tx),
        condition: tx.Condition,
        allowCancelAfter: utils_1.parseTimestamp(tx.CancelAfter),
        allowExecuteAfter: utils_1.parseTimestamp(tx.FinishAfter),
        sourceTag: tx.SourceTag,
        destinationTag: tx.DestinationTag
    });
}
exports.default = parseEscrowCreation;
//# sourceMappingURL=escrow-creation.js.map