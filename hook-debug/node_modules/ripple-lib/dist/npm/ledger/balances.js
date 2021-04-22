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
function getTrustlineBalanceAmount(trustline) {
    return {
        currency: trustline.specification.currency,
        counterparty: trustline.specification.counterparty,
        value: trustline.state.balance
    };
}
function formatBalances(options, balances) {
    const result = balances.trustlines.map(getTrustlineBalanceAmount);
    if (!(options.counterparty || (options.currency && options.currency !== 'XRP'))) {
        const xrpBalance = {
            currency: 'XRP',
            value: balances.xrp
        };
        result.unshift(xrpBalance);
    }
    if (options.limit && result.length > options.limit) {
        const toRemove = result.length - options.limit;
        result.splice(-toRemove, toRemove);
    }
    return result;
}
function getLedgerVersionHelper(connection, optionValue) {
    if (optionValue !== undefined && optionValue !== null) {
        return Promise.resolve(optionValue);
    }
    return connection.getLedgerVersion();
}
function getBalances(address, options = {}) {
    common_1.validate.getTrustlines({ address, options });
    address = common_1.ensureClassicAddress(address);
    return Promise.all([
        getLedgerVersionHelper(this.connection, options.ledgerVersion).then(ledgerVersion => utils.getXRPBalance(this.connection, address, ledgerVersion)),
        this.getTrustlines(address, options)
    ]).then(results => formatBalances(options, { xrp: results[0], trustlines: results[1] }));
}
exports.default = getBalances;
//# sourceMappingURL=balances.js.map