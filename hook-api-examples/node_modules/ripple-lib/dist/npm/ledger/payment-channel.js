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
const payment_channel_1 = require("./parse/payment-channel");
const common_1 = require("../common");
const NotFoundError = common_1.errors.NotFoundError;
function formatResponse(response) {
    if (response.node === undefined ||
        response.node.LedgerEntryType !== 'PayChannel') {
        throw new NotFoundError('Payment channel ledger entry not found');
    }
    return payment_channel_1.parsePaymentChannel(response.node);
}
function getPaymentChannel(id) {
    return __awaiter(this, void 0, void 0, function* () {
        common_1.validate.getPaymentChannel({ id });
        const response = yield this.request('ledger_entry', {
            index: id,
            binary: false,
            ledger_index: 'validated'
        });
        return formatResponse(response);
    });
}
exports.default = getPaymentChannel;
//# sourceMappingURL=payment-channel.js.map