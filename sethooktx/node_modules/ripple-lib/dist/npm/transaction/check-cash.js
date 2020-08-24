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
const ValidationError = utils.common.errors.ValidationError;
const toRippledAmount = utils.common.toRippledAmount;
const common_1 = require("../common");
function createCheckCashTransaction(account, checkCash) {
    if (checkCash.amount && checkCash.deliverMin) {
        throw new ValidationError('"amount" and "deliverMin" properties on ' +
            'CheckCash are mutually exclusive');
    }
    const txJSON = {
        Account: account,
        TransactionType: 'CheckCash',
        CheckID: checkCash.checkID
    };
    if (checkCash.amount !== undefined) {
        txJSON.Amount = toRippledAmount(checkCash.amount);
    }
    if (checkCash.deliverMin !== undefined) {
        txJSON.DeliverMin = toRippledAmount(checkCash.deliverMin);
    }
    return txJSON;
}
function prepareCheckCash(address, checkCash, instructions = {}) {
    try {
        common_1.validate.prepareCheckCash({ address, checkCash, instructions });
        const txJSON = createCheckCashTransaction(address, checkCash);
        return utils.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = prepareCheckCash;
//# sourceMappingURL=check-cash.js.map