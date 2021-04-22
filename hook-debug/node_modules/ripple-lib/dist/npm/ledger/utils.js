"use strict";
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (Object.hasOwnProperty.call(mod, k)) result[k] = mod[k];
    result["default"] = mod;
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
const _ = __importStar(require("lodash"));
const assert = __importStar(require("assert"));
const common = __importStar(require("../common"));
exports.common = common;
function clamp(value, min, max) {
    assert.ok(min <= max, 'Illegal clamp bounds');
    return Math.min(Math.max(value, min), max);
}
exports.clamp = clamp;
function getXRPBalance(connection, address, ledgerVersion) {
    const request = {
        command: 'account_info',
        account: address,
        ledger_index: ledgerVersion
    };
    return connection
        .request(request)
        .then(data => common.dropsToXrp(data.account_data.Balance));
}
exports.getXRPBalance = getXRPBalance;
function getRecursiveRecur(getter, marker, limit) {
    return getter(marker, limit).then(data => {
        const remaining = limit - data.results.length;
        if (remaining > 0 && data.marker !== undefined) {
            return getRecursiveRecur(getter, data.marker, remaining).then(results => data.results.concat(results));
        }
        return data.results.slice(0, limit);
    });
}
function getRecursive(getter, limit) {
    return getRecursiveRecur(getter, undefined, limit || Infinity);
}
exports.getRecursive = getRecursive;
function renameCounterpartyToIssuer(obj) {
    const issuer = obj.counterparty !== undefined
        ? obj.counterparty
        : obj.issuer !== undefined
            ? obj.issuer
            : undefined;
    const withIssuer = Object.assign({}, obj, { issuer });
    delete withIssuer.counterparty;
    return withIssuer;
}
exports.renameCounterpartyToIssuer = renameCounterpartyToIssuer;
function renameCounterpartyToIssuerInOrder(order) {
    const taker_gets = renameCounterpartyToIssuer(order.taker_gets);
    const taker_pays = renameCounterpartyToIssuer(order.taker_pays);
    const changes = { taker_gets, taker_pays };
    return _.assign({}, order, _.omitBy(changes, _.isUndefined));
}
exports.renameCounterpartyToIssuerInOrder = renameCounterpartyToIssuerInOrder;
function signum(num) {
    return num === 0 ? 0 : num > 0 ? 1 : -1;
}
function compareTransactions(first, second) {
    if (!first.outcome || !second.outcome) {
        return 0;
    }
    if (first.outcome.ledgerVersion === second.outcome.ledgerVersion) {
        return signum(first.outcome.indexInLedger - second.outcome.indexInLedger);
    }
    return first.outcome.ledgerVersion < second.outcome.ledgerVersion ? -1 : 1;
}
exports.compareTransactions = compareTransactions;
function hasCompleteLedgerRange(connection, minLedgerVersion, maxLedgerVersion) {
    const firstLedgerVersion = 32570;
    return connection.hasLedgerVersions(minLedgerVersion || firstLedgerVersion, maxLedgerVersion);
}
exports.hasCompleteLedgerRange = hasCompleteLedgerRange;
function isPendingLedgerVersion(connection, maxLedgerVersion) {
    return connection
        .getLedgerVersion()
        .then(ledgerVersion => ledgerVersion < (maxLedgerVersion || 0));
}
exports.isPendingLedgerVersion = isPendingLedgerVersion;
function ensureLedgerVersion(options) {
    if (Boolean(options) &&
        options.ledgerVersion !== undefined &&
        options.ledgerVersion !== null) {
        return Promise.resolve(options);
    }
    return this.getLedgerVersion().then(ledgerVersion => _.assign({}, options, { ledgerVersion }));
}
exports.ensureLedgerVersion = ensureLedgerVersion;
//# sourceMappingURL=utils.js.map