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
const utils_1 = require("./utils");
const common_1 = require("../../common");
function parseEscrowCancellation(tx) {
    assert.ok(tx.TransactionType === 'EscrowCancel');
    return common_1.removeUndefined({
        memos: utils_1.parseMemos(tx),
        owner: tx.Owner,
        escrowSequence: tx.OfferSequence
    });
}
exports.default = parseEscrowCancellation;
//# sourceMappingURL=escrow-cancellation.js.map