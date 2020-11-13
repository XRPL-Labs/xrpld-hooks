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
Object.defineProperty(exports, "__esModule", { value: true });
const common_1 = require("../common");
function formatAccountInfo(response) {
    const data = response.account_data;
    return common_1.removeUndefined({
        sequence: data.Sequence,
        xrpBalance: common_1.dropsToXrp(data.Balance),
        ownerCount: data.OwnerCount,
        previousInitiatedTransactionID: data.AccountTxnID,
        previousAffectingTransactionID: data.PreviousTxnID,
        previousAffectingTransactionLedgerVersion: data.PreviousTxnLgrSeq
    });
}
function getAccountInfo(address, options = {}) {
    return __awaiter(this, void 0, void 0, function* () {
        common_1.validate.getAccountInfo({ address, options });
        address = common_1.ensureClassicAddress(address);
        const response = yield this.request('account_info', {
            account: address,
            ledger_index: options.ledgerVersion || 'validated'
        });
        return formatAccountInfo(response);
    });
}
exports.default = getAccountInfo;
//# sourceMappingURL=accountinfo.js.map