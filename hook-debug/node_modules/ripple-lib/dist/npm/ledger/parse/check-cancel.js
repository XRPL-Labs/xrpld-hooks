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
function parseCheckCancel(tx) {
    assert.ok(tx.TransactionType === 'CheckCancel');
    return common_1.removeUndefined({
        checkID: tx.CheckID
    });
}
exports.default = parseCheckCancel;
//# sourceMappingURL=check-cancel.js.map