"use strict";
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
const lodash_isequal_1 = __importDefault(require("lodash.isequal"));
const utils = __importStar(require("./utils"));
const ripple_keypairs_1 = __importDefault(require("ripple-keypairs"));
const ripple_binary_codec_1 = __importDefault(require("ripple-binary-codec"));
const hashes_1 = require("../common/hashes");
const bignumber_js_1 = require("bignumber.js");
const common_1 = require("../common");
const validate = utils.common.validate;
function computeSignature(tx, privateKey, signAs) {
    const signingData = signAs
        ? ripple_binary_codec_1.default.encodeForMultisigning(tx, signAs)
        : ripple_binary_codec_1.default.encodeForSigning(tx);
    return ripple_keypairs_1.default.sign(signingData, privateKey);
}
function signWithKeypair(api, txJSON, keypair, options = {
    signAs: ''
}) {
    validate.sign({ txJSON, keypair });
    const tx = JSON.parse(txJSON);
    if (tx.TxnSignature || tx.Signers) {
        throw new utils.common.errors.ValidationError('txJSON must not contain "TxnSignature" or "Signers" properties');
    }
    checkFee(api, tx.Fee);
    const txToSignAndEncode = Object.assign({}, tx);
    txToSignAndEncode.SigningPubKey = options.signAs ? '' : keypair.publicKey;
    if (options.signAs) {
        const signer = {
            Account: options.signAs,
            SigningPubKey: keypair.publicKey,
            TxnSignature: computeSignature(txToSignAndEncode, keypair.privateKey, options.signAs)
        };
        txToSignAndEncode.Signers = [{ Signer: signer }];
    }
    else {
        txToSignAndEncode.TxnSignature = computeSignature(txToSignAndEncode, keypair.privateKey);
    }
    const serialized = ripple_binary_codec_1.default.encode(txToSignAndEncode);
    checkTxSerialization(serialized, tx);
    return {
        signedTransaction: serialized,
        id: hashes_1.computeBinaryTransactionHash(serialized)
    };
}
function objectDiff(a, b) {
    const diffs = {};
    const compare = function (i1, i2, k) {
        const type1 = Object.prototype.toString.call(i1);
        const type2 = Object.prototype.toString.call(i2);
        if (type2 === '[object Undefined]') {
            diffs[k] = null;
            return;
        }
        if (type1 !== type2) {
            diffs[k] = i2;
            return;
        }
        if (type1 === '[object Object]') {
            const objDiff = objectDiff(i1, i2);
            if (Object.keys(objDiff).length > 0) {
                diffs[k] = objDiff;
            }
            return;
        }
        if (type1 === '[object Array]') {
            if (!lodash_isequal_1.default(i1, i2)) {
                diffs[k] = i2;
            }
            return;
        }
        if (type1 === '[object Function]') {
            if (i1.toString() !== i2.toString()) {
                diffs[k] = i2;
            }
            return;
        }
        if (i1 !== i2) {
            diffs[k] = i2;
        }
    };
    for (const key in a) {
        if (a.hasOwnProperty(key)) {
            compare(a[key], b[key], key);
        }
    }
    for (const key in b) {
        if (b.hasOwnProperty(key)) {
            if (!a[key] && a[key] !== b[key]) {
                diffs[key] = b[key];
            }
        }
    }
    return diffs;
}
function checkTxSerialization(serialized, tx) {
    const decoded = ripple_binary_codec_1.default.decode(serialized);
    if (!decoded.TxnSignature && !decoded.Signers) {
        throw new utils.common.errors.ValidationError('Serialized transaction must have a TxnSignature or Signers property');
    }
    delete decoded.TxnSignature;
    delete decoded.Signers;
    if (!tx.SigningPubKey) {
        delete decoded.SigningPubKey;
    }
    if (!lodash_isequal_1.default(decoded, tx)) {
        const error = new utils.common.errors.ValidationError('Serialized transaction does not match original txJSON. See `error.data`');
        error.data = {
            decoded,
            tx,
            diff: objectDiff(tx, decoded)
        };
        throw error;
    }
}
function checkFee(api, txFee) {
    const fee = new bignumber_js_1.BigNumber(txFee);
    const maxFeeDrops = common_1.xrpToDrops(api._maxFeeXRP);
    if (fee.isGreaterThan(maxFeeDrops)) {
        throw new utils.common.errors.ValidationError(`"Fee" should not exceed "${maxFeeDrops}". ` +
            'To use a higher fee, set `maxFeeXRP` in the RippleAPI constructor.');
    }
}
function sign(txJSON, secret, options, keypair) {
    if (typeof secret === 'string') {
        validate.sign({ txJSON, secret });
        return signWithKeypair(this, txJSON, ripple_keypairs_1.default.deriveKeypair(secret), options);
    }
    else {
        if (!keypair && !secret) {
            throw new utils.common.errors.ValidationError('sign: Missing secret or keypair.');
        }
        return signWithKeypair(this, txJSON, keypair ? keypair : secret, options);
    }
}
exports.default = sign;
//# sourceMappingURL=sign.js.map