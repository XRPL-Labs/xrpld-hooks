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
const ValidationError = utils.common.errors.ValidationError;
function createEscrowExecutionTransaction(account, payment) {
    const txJSON = {
        TransactionType: 'EscrowFinish',
        Account: account,
        Owner: payment.owner,
        OfferSequence: payment.escrowSequence
    };
    if (Boolean(payment.condition) !== Boolean(payment.fulfillment)) {
        throw new ValidationError('"condition" and "fulfillment" fields on' +
            ' EscrowFinish must only be specified together.');
    }
    if (payment.condition !== undefined) {
        txJSON.Condition = payment.condition;
    }
    if (payment.fulfillment !== undefined) {
        txJSON.Fulfillment = payment.fulfillment;
    }
    if (payment.memos !== undefined) {
        txJSON.Memos = payment.memos.map(utils.convertMemo);
    }
    return txJSON;
}
function prepareEscrowExecution(address, escrowExecution, instructions = {}) {
    try {
        validate.prepareEscrowExecution({ address, escrowExecution, instructions });
        const txJSON = createEscrowExecutionTransaction(address, escrowExecution);
        return utils.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = prepareEscrowExecution;
//# sourceMappingURL=escrow-execution.js.map