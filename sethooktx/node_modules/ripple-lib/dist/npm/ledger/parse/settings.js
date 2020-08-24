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
const _ = __importStar(require("lodash"));
const assert = __importStar(require("assert"));
const common_1 = require("../../common");
const AccountFlags = common_1.constants.AccountFlags;
const fields_1 = __importDefault(require("./fields"));
function getAccountRootModifiedNode(tx) {
    const modifiedNodes = tx.meta.AffectedNodes.filter(node => node.ModifiedNode.LedgerEntryType === 'AccountRoot');
    assert.ok(modifiedNodes.length === 1);
    return modifiedNodes[0].ModifiedNode;
}
function parseFlags(tx) {
    const settings = {};
    if (tx.TransactionType !== 'AccountSet') {
        return settings;
    }
    const node = getAccountRootModifiedNode(tx);
    const oldFlags = _.get(node.PreviousFields, 'Flags');
    const newFlags = _.get(node.FinalFields, 'Flags');
    if (oldFlags !== undefined && newFlags !== undefined) {
        const changedFlags = oldFlags ^ newFlags;
        const setFlags = newFlags & changedFlags;
        const clearedFlags = oldFlags & changedFlags;
        _.forEach(AccountFlags, (flagValue, flagName) => {
            if (setFlags & flagValue) {
                settings[flagName] = true;
            }
            else if (clearedFlags & flagValue) {
                settings[flagName] = false;
            }
        });
    }
    const oldField = _.get(node.PreviousFields, 'AccountTxnID');
    const newField = _.get(node.FinalFields, 'AccountTxnID');
    if (newField && !oldField) {
        settings.enableTransactionIDTracking = true;
    }
    else if (oldField && !newField) {
        settings.enableTransactionIDTracking = false;
    }
    return settings;
}
function parseSettings(tx) {
    const txType = tx.TransactionType;
    assert.ok(txType === 'AccountSet' ||
        txType === 'SetRegularKey' ||
        txType === 'SignerListSet');
    return _.assign({}, parseFlags(tx), fields_1.default(tx));
}
exports.default = parseSettings;
//# sourceMappingURL=settings.js.map