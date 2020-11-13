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
const toRippledAmount = utils.common.toRippledAmount;
const common_1 = require("../common");
function createCheckCreateTransaction(account, check) {
    const txJSON = {
        Account: account,
        TransactionType: 'CheckCreate',
        Destination: check.destination,
        SendMax: toRippledAmount(check.sendMax)
    };
    if (check.destinationTag !== undefined) {
        txJSON.DestinationTag = check.destinationTag;
    }
    if (check.expiration !== undefined) {
        txJSON.Expiration = common_1.iso8601ToRippleTime(check.expiration);
    }
    if (check.invoiceID !== undefined) {
        txJSON.InvoiceID = check.invoiceID;
    }
    return txJSON;
}
function prepareCheckCreate(address, checkCreate, instructions = {}) {
    try {
        common_1.validate.prepareCheckCreate({ address, checkCreate, instructions });
        const txJSON = createCheckCreateTransaction(address, checkCreate);
        return utils.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = prepareCheckCreate;
//# sourceMappingURL=check-create.js.map