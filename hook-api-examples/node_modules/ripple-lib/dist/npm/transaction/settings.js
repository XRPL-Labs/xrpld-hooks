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
const assert = __importStar(require("assert"));
const bignumber_js_1 = __importDefault(require("bignumber.js"));
const utils = __importStar(require("./utils"));
const validate = utils.common.validate;
const AccountFlagIndices = utils.common.constants.AccountFlagIndices;
const AccountFields = utils.common.constants.AccountFields;
function setTransactionFlags(txJSON, values) {
    const keys = Object.keys(values);
    assert.ok(keys.length === 1, 'ERROR: can only set one setting per transaction');
    const flagName = keys[0];
    const value = values[flagName];
    const index = AccountFlagIndices[flagName];
    if (index !== undefined) {
        if (value) {
            txJSON.SetFlag = index;
        }
        else {
            txJSON.ClearFlag = index;
        }
    }
}
function setTransactionFields(txJSON, input) {
    const fieldSchema = AccountFields;
    for (const fieldName in fieldSchema) {
        const field = fieldSchema[fieldName];
        let value = input[field.name];
        if (value === undefined) {
            continue;
        }
        if (value === null && field.hasOwnProperty('defaults')) {
            value = field.defaults;
        }
        if (field.encoding === 'hex' && !field.length) {
            value = Buffer.from(value, 'ascii')
                .toString('hex')
                .toUpperCase();
        }
        txJSON[fieldName] = value;
    }
}
function convertTransferRate(transferRate) {
    return new bignumber_js_1.default(transferRate).shiftedBy(9).toNumber();
}
function formatSignerEntry(signer) {
    return {
        SignerEntry: {
            Account: signer.address,
            SignerWeight: signer.weight
        }
    };
}
function createSettingsTransactionWithoutMemos(account, settings) {
    if (settings.regularKey !== undefined) {
        const removeRegularKey = {
            TransactionType: 'SetRegularKey',
            Account: account
        };
        if (settings.regularKey === null) {
            return removeRegularKey;
        }
        return Object.assign({}, removeRegularKey, {
            RegularKey: settings.regularKey
        });
    }
    if (settings.signers !== undefined) {
        const setSignerList = {
            TransactionType: 'SignerListSet',
            Account: account,
            SignerEntries: [],
            SignerQuorum: settings.signers.threshold
        };
        if (settings.signers.weights !== undefined) {
            setSignerList.SignerEntries = settings.signers.weights.map(formatSignerEntry);
        }
        return setSignerList;
    }
    const txJSON = {
        TransactionType: 'AccountSet',
        Account: account
    };
    const settingsWithoutMemos = Object.assign({}, settings);
    delete settingsWithoutMemos.memos;
    setTransactionFlags(txJSON, settingsWithoutMemos);
    setTransactionFields(txJSON, settings);
    if (txJSON.TransferRate !== undefined) {
        txJSON.TransferRate = convertTransferRate(txJSON.TransferRate);
    }
    return txJSON;
}
function createSettingsTransaction(account, settings) {
    const txJSON = createSettingsTransactionWithoutMemos(account, settings);
    if (settings.memos !== undefined) {
        txJSON.Memos = settings.memos.map(utils.convertMemo);
    }
    return txJSON;
}
function prepareSettings(address, settings, instructions = {}) {
    try {
        validate.prepareSettings({ address, settings, instructions });
        const txJSON = createSettingsTransaction(address, settings);
        return utils.prepareTransaction(txJSON, this, instructions);
    }
    catch (e) {
        return Promise.reject(e);
    }
}
exports.default = prepareSettings;
//# sourceMappingURL=settings.js.map