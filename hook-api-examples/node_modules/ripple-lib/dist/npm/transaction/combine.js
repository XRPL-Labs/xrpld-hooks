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
const ripple_binary_codec_1 = __importDefault(require("ripple-binary-codec"));
const utils = __importStar(require("./utils"));
const bignumber_js_1 = __importDefault(require("bignumber.js"));
const ripple_address_codec_1 = require("ripple-address-codec");
const common_1 = require("../common");
const hashes_1 = require("../common/hashes");
function addressToBigNumber(address) {
    const hex = Buffer.from(ripple_address_codec_1.decodeAccountID(address)).toString('hex');
    return new bignumber_js_1.default(hex, 16);
}
function compareSigners(a, b) {
    return addressToBigNumber(a.Signer.Account).comparedTo(addressToBigNumber(b.Signer.Account));
}
function combine(signedTransactions) {
    common_1.validate.combine({ signedTransactions });
    const txs = _.map(signedTransactions, ripple_binary_codec_1.default.decode);
    const tx = _.omit(txs[0], 'Signers');
    if (!_.every(txs, _tx => _.isEqual(tx, _.omit(_tx, 'Signers')))) {
        throw new utils.common.errors.ValidationError('txJSON is not the same for all signedTransactions');
    }
    const unsortedSigners = _.reduce(txs, (accumulator, _tx) => accumulator.concat(_tx.Signers || []), []);
    const signers = unsortedSigners.sort(compareSigners);
    const signedTx = _.assign({}, tx, { Signers: signers });
    const signedTransaction = ripple_binary_codec_1.default.encode(signedTx);
    const id = hashes_1.computeBinaryTransactionHash(signedTransaction);
    return { signedTransaction, id };
}
exports.default = combine;
//# sourceMappingURL=combine.js.map