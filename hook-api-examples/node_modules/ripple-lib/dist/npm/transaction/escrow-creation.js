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
const common_1 = require("../common");
const ValidationError = utils.common.errors.ValidationError;
function createEscrowCreationTransaction(account, payment) {
    const txJSON = {
        TransactionType: 'EscrowCreate',
        Account: account,
        Destination: payment.destination,
        Amount: common_1.xrpToDrops(payment.amount)
    };
    if (payment.condition !== undefined) {
        txJSON.Condition = payment.condition;
    }
    if (payment.allowCancelAfter !== undefined) {
        txJSON.CancelAfter = common_1.iso8601ToRippleTime(payment.allowCancelAfter);
    }
    if (payment.allowExecuteAfter !== undefined) {
        txJSON.FinishAfter = common_1.iso8601ToRippleTime(payment.allowExecuteAfter);
    }
    if (payment.sourceTag !== undefined) {
        txJSON.SourceTag = payment.sourceTag;
    }
    if (payment.destinationTag !== undefined) {
        txJSON.DestinationTag = payment.destinationTag;
    }
    if (payment.memos !== undefined) {
        txJSON.Memos = payment.memos.map(utils.convertMemo);
    }
    if (Boolean(payment.allowCancelAfter) &&
        Boolean(payment.allowExecuteAfter) &&
        txJSON.CancelAfter <= txJSON.FinishAfter) {
        throw new ValidationError('prepareEscrowCreation: ' +
            '"allowCancelAfter" must be after "allowExecuteAfter"');
    }
    return txJSON;
}
function prepareEscrowCreation(address, escrowCreation, instructions = {}) {
    try {
        common_1.validate.prepareEscrowCreation({ address, escrowCreation, instructions });
        const txJSON = createEscrowCreationTransaction(address, escrowCreation);
        return utils.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = prepareEscrowCreation;
//# sourceMappingURL=escrow-creation.js.map