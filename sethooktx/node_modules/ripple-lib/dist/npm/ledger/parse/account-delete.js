"use strict";
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (Object.hasOwnProperty.call(mod, k)) result[k] = mod[k];
    result["default"] = mod;
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
const assert = __importStar(require("assert"));
const common_1 = require("../../common");
const ripple_address_codec_1 = require("ripple-address-codec");
function parseAccountDelete(tx) {
    assert.ok(tx.TransactionType === 'AccountDelete');
    return common_1.removeUndefined({
        destination: tx.Destination,
        destinationTag: tx.DestinationTag,
        destinationXAddress: ripple_address_codec_1.classicAddressToXAddress(tx.Destination, tx.DestinationTag === undefined ? false : tx.DestinationTag, false)
    });
}
exports.default = parseAccountDelete;
//# sourceMappingURL=account-delete.js.map