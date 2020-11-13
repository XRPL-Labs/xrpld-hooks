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
function createEscrowCancellationTransaction(account, payment) {
    const txJSON = {
        TransactionType: 'EscrowCancel',
        Account: account,
        Owner: payment.owner,
        OfferSequence: payment.escrowSequence
    };
    if (payment.memos !== undefined) {
        txJSON.Memos = payment.memos.map(utils.convertMemo);
    }
    return txJSON;
}
function prepareEscrowCancellation(address, escrowCancellation, instructions = {}) {
    validate.prepareEscrowCancellation({
        address,
        escrowCancellation,
        instructions
    });
    const txJSON = createEscrowCancellationTransaction(address, escrowCancellation);
    return utils.prepareTransaction(txJSON, this, instructions);
}
exports.default = prepareEscrowCancellation;
//# sourceMappingURL=escrow-cancellation.js.map