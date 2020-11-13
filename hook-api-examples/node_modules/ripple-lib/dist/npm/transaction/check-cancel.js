"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const utils_1 = require("./utils");
const common_1 = require("../common");
function createCheckCancelTransaction(account, cancel) {
    const txJSON = {
        Account: account,
        TransactionType: 'CheckCancel',
        CheckID: cancel.checkID
    };
    return txJSON;
}
function prepareCheckCancel(address, checkCancel, instructions = {}) {
    try {
        common_1.validate.prepareCheckCancel({ address, checkCancel, instructions });
        const txJSON = createCheckCancelTransaction(address, checkCancel);
        return utils_1.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = prepareCheckCancel;
//# sourceMappingURL=check-cancel.js.map