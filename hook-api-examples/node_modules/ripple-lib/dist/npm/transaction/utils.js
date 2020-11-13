"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (Object.hasOwnProperty.call(mod, k)) result[k] = mod[k];
    result["default"] = mod;
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
const bignumber_js_1 = __importDefault(require("bignumber.js"));
const common = __importStar(require("../common"));
exports.common = common;
const errors_1 = require("../common/errors");
const ripple_address_codec_1 = require("ripple-address-codec");
const txFlags = common.txFlags;
const TRANSACTION_TYPES_WITH_DESTINATION_TAG_FIELD = [
    'Payment',
    'CheckCreate',
    'EscrowCreate',
    'PaymentChannelCreate'
];
function formatPrepareResponse(txJSON) {
    const instructions = {
        fee: common.dropsToXrp(txJSON.Fee),
        sequence: txJSON.Sequence,
        maxLedgerVersion: txJSON.LastLedgerSequence === undefined ? null : txJSON.LastLedgerSequence
    };
    return {
        txJSON: JSON.stringify(txJSON),
        instructions
    };
}
function setCanonicalFlag(txJSON) {
    txJSON.Flags |= txFlags.Universal.FullyCanonicalSig;
    txJSON.Flags = txJSON.Flags >>> 0;
}
exports.setCanonicalFlag = setCanonicalFlag;
function scaleValue(value, multiplier, extra = 0) {
    return new bignumber_js_1.default(value)
        .times(multiplier)
        .plus(extra)
        .toString();
}
function getClassicAccountAndTag(Account, expectedTag) {
    if (ripple_address_codec_1.isValidXAddress(Account)) {
        const classic = ripple_address_codec_1.xAddressToClassicAddress(Account);
        if (expectedTag !== undefined && classic.tag !== expectedTag) {
            throw new errors_1.ValidationError('address includes a tag that does not match the tag specified in the transaction');
        }
        return {
            classicAccount: classic.classicAddress,
            tag: classic.tag
        };
    }
    else {
        return {
            classicAccount: Account,
            tag: expectedTag
        };
    }
}
exports.getClassicAccountAndTag = getClassicAccountAndTag;
function prepareTransaction(txJSON, api, instructions) {
    common.validate.instructions(instructions);
    common.validate.tx_json(txJSON);
    const disallowedFieldsInTxJSON = [
        'maxLedgerVersion',
        'maxLedgerVersionOffset',
        'fee',
        'sequence'
    ];
    const badFields = disallowedFieldsInTxJSON.filter(field => txJSON[field]);
    if (badFields.length) {
        return Promise.reject(new errors_1.ValidationError('txJSON additionalProperty "' +
            badFields[0] +
            '" exists in instance when not allowed'));
    }
    const newTxJSON = Object.assign({}, txJSON);
    if (txJSON['SignerQuorum'] === 0) {
        delete newTxJSON.SignerEntries;
    }
    const { classicAccount, tag: sourceTag } = getClassicAccountAndTag(txJSON.Account);
    newTxJSON.Account = classicAccount;
    if (sourceTag !== undefined) {
        if (txJSON.SourceTag && txJSON.SourceTag !== sourceTag) {
            return Promise.reject(new errors_1.ValidationError('The `SourceTag`, if present, must match the tag of the `Account` X-address'));
        }
        if (sourceTag) {
            newTxJSON.SourceTag = sourceTag;
        }
    }
    if (typeof txJSON.Destination === 'string') {
        const { classicAccount: destinationAccount, tag: destinationTag } = getClassicAccountAndTag(txJSON.Destination);
        newTxJSON.Destination = destinationAccount;
        if (destinationTag !== undefined) {
            if (TRANSACTION_TYPES_WITH_DESTINATION_TAG_FIELD.includes(txJSON.TransactionType)) {
                if (txJSON.DestinationTag && txJSON.DestinationTag !== destinationTag) {
                    return Promise.reject(new errors_1.ValidationError('The Payment `DestinationTag`, if present, must match the tag of the `Destination` X-address'));
                }
                if (destinationTag) {
                    newTxJSON.DestinationTag = destinationTag;
                }
            }
        }
    }
    function convertToClassicAccountIfPresent(fieldName) {
        const account = txJSON[fieldName];
        if (typeof account === 'string') {
            const { classicAccount: ca } = getClassicAccountAndTag(account);
            newTxJSON[fieldName] = ca;
        }
    }
    convertToClassicAccountIfPresent('Authorize');
    convertToClassicAccountIfPresent('Unauthorize');
    convertToClassicAccountIfPresent('Owner');
    convertToClassicAccountIfPresent('RegularKey');
    setCanonicalFlag(newTxJSON);
    function prepareMaxLedgerVersion() {
        if (newTxJSON.LastLedgerSequence && instructions.maxLedgerVersion) {
            return Promise.reject(new errors_1.ValidationError('`LastLedgerSequence` in txJSON and `maxLedgerVersion`' +
                ' in `instructions` cannot both be set'));
        }
        if (newTxJSON.LastLedgerSequence && instructions.maxLedgerVersionOffset) {
            return Promise.reject(new errors_1.ValidationError('`LastLedgerSequence` in txJSON and `maxLedgerVersionOffset`' +
                ' in `instructions` cannot both be set'));
        }
        if (newTxJSON.LastLedgerSequence) {
            return Promise.resolve();
        }
        if (instructions.maxLedgerVersion !== undefined) {
            if (instructions.maxLedgerVersion !== null) {
                newTxJSON.LastLedgerSequence = instructions.maxLedgerVersion;
            }
            return Promise.resolve();
        }
        const offset = instructions.maxLedgerVersionOffset !== undefined
            ? instructions.maxLedgerVersionOffset
            : 3;
        return api.connection.getLedgerVersion().then(ledgerVersion => {
            newTxJSON.LastLedgerSequence = ledgerVersion + offset;
            return;
        });
    }
    function prepareFee() {
        if (newTxJSON.Fee && instructions.fee) {
            return Promise.reject(new errors_1.ValidationError('`Fee` in txJSON and `fee` in `instructions` cannot both be set'));
        }
        if (newTxJSON.Fee) {
            return Promise.resolve();
        }
        const multiplier = instructions.signersCount === undefined
            ? 1
            : instructions.signersCount + 1;
        if (instructions.fee !== undefined) {
            const fee = new bignumber_js_1.default(instructions.fee);
            if (fee.isGreaterThan(api._maxFeeXRP)) {
                return Promise.reject(new errors_1.ValidationError(`Fee of ${fee.toString(10)} XRP exceeds ` +
                    `max of ${api._maxFeeXRP} XRP. To use this fee, increase ` +
                    '`maxFeeXRP` in the RippleAPI constructor.'));
            }
            newTxJSON.Fee = scaleValue(common.xrpToDrops(instructions.fee), multiplier);
            return Promise.resolve();
        }
        const cushion = api._feeCushion;
        return api.getFee(cushion).then(fee => {
            return api.connection.getFeeRef().then(feeRef => {
                const extraFee = newTxJSON.TransactionType !== 'EscrowFinish' ||
                    newTxJSON.Fulfillment === undefined
                    ? 0
                    : cushion *
                        feeRef *
                        (32 +
                            Math.floor(Buffer.from(newTxJSON.Fulfillment, 'hex').length / 16));
                const feeDrops = common.xrpToDrops(fee);
                const maxFeeXRP = instructions.maxFee
                    ? bignumber_js_1.default.min(api._maxFeeXRP, instructions.maxFee)
                    : api._maxFeeXRP;
                const maxFeeDrops = common.xrpToDrops(maxFeeXRP);
                const normalFee = scaleValue(feeDrops, multiplier, extraFee);
                newTxJSON.Fee = bignumber_js_1.default.min(normalFee, maxFeeDrops).toString(10);
                return;
            });
        });
    }
    function prepareSequence() {
        return __awaiter(this, void 0, void 0, function* () {
            if (instructions.sequence !== undefined) {
                if (newTxJSON.Sequence === undefined ||
                    instructions.sequence === newTxJSON.Sequence) {
                    newTxJSON.Sequence = instructions.sequence;
                    return Promise.resolve();
                }
                else {
                    return Promise.reject(new errors_1.ValidationError('`Sequence` in txJSON must match `sequence` in `instructions`'));
                }
            }
            if (newTxJSON.Sequence !== undefined) {
                return Promise.resolve();
            }
            try {
                const response = yield api.request('account_info', {
                    account: classicAccount
                });
                newTxJSON.Sequence = response.account_data.Sequence;
                return Promise.resolve();
            }
            catch (e) {
                return Promise.reject(e);
            }
        });
    }
    return Promise.all([
        prepareMaxLedgerVersion(),
        prepareFee(),
        prepareSequence()
    ]).then(() => formatPrepareResponse(newTxJSON));
}
exports.prepareTransaction = prepareTransaction;
function convertStringToHex(string) {
    return Buffer.from(string, 'utf8')
        .toString('hex')
        .toUpperCase();
}
exports.convertStringToHex = convertStringToHex;
function convertMemo(memo) {
    return {
        Memo: common.removeUndefined({
            MemoData: memo.data ? convertStringToHex(memo.data) : undefined,
            MemoType: memo.type ? convertStringToHex(memo.type) : undefined,
            MemoFormat: memo.format ? convertStringToHex(memo.format) : undefined
        })
    };
}
exports.convertMemo = convertMemo;
//# sourceMappingURL=utils.js.map