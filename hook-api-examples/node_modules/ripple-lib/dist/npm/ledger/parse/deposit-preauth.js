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
function parseDepositPreauth(tx) {
    assert.ok(tx.TransactionType === 'DepositPreauth');
    return common_1.removeUndefined({
        authorize: tx.Authorize,
        unauthorize: tx.Unauthorize
    });
}
exports.default = parseDepositPreauth;
//# sourceMappingURL=deposit-preauth.js.map