"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const utils_1 = require("./utils");
const common_1 = require("../../common");
const settings_1 = __importDefault(require("./settings"));
const account_delete_1 = __importDefault(require("./account-delete"));
const check_cancel_1 = __importDefault(require("./check-cancel"));
const check_cash_1 = __importDefault(require("./check-cash"));
const check_create_1 = __importDefault(require("./check-create"));
const deposit_preauth_1 = __importDefault(require("./deposit-preauth"));
const escrow_cancellation_1 = __importDefault(require("./escrow-cancellation"));
const escrow_creation_1 = __importDefault(require("./escrow-creation"));
const escrow_execution_1 = __importDefault(require("./escrow-execution"));
const cancellation_1 = __importDefault(require("./cancellation"));
const order_1 = __importDefault(require("./order"));
const payment_1 = __importDefault(require("./payment"));
const payment_channel_claim_1 = __importDefault(require("./payment-channel-claim"));
const payment_channel_create_1 = __importDefault(require("./payment-channel-create"));
const payment_channel_fund_1 = __importDefault(require("./payment-channel-fund"));
const trustline_1 = __importDefault(require("./trustline"));
const amendment_1 = __importDefault(require("./amendment"));
const fee_update_1 = __importDefault(require("./fee-update"));
function parseTransactionType(type) {
    const mapping = {
        AccountSet: 'settings',
        AccountDelete: 'accountDelete',
        CheckCancel: 'checkCancel',
        CheckCash: 'checkCash',
        CheckCreate: 'checkCreate',
        DepositPreauth: 'depositPreauth',
        EscrowCancel: 'escrowCancellation',
        EscrowCreate: 'escrowCreation',
        EscrowFinish: 'escrowExecution',
        OfferCancel: 'orderCancellation',
        OfferCreate: 'order',
        Payment: 'payment',
        PaymentChannelClaim: 'paymentChannelClaim',
        PaymentChannelCreate: 'paymentChannelCreate',
        PaymentChannelFund: 'paymentChannelFund',
        SetRegularKey: 'settings',
        SignerListSet: 'settings',
        TrustSet: 'trustline',
        EnableAmendment: 'amendment',
        SetFee: 'feeUpdate'
    };
    return mapping[type] || null;
}
function parseTransaction(tx, includeRawTransaction) {
    const type = parseTransactionType(tx.TransactionType);
    const mapping = {
        settings: settings_1.default,
        accountDelete: account_delete_1.default,
        checkCancel: check_cancel_1.default,
        checkCash: check_cash_1.default,
        checkCreate: check_create_1.default,
        depositPreauth: deposit_preauth_1.default,
        escrowCancellation: escrow_cancellation_1.default,
        escrowCreation: escrow_creation_1.default,
        escrowExecution: escrow_execution_1.default,
        orderCancellation: cancellation_1.default,
        order: order_1.default,
        payment: payment_1.default,
        paymentChannelClaim: payment_channel_claim_1.default,
        paymentChannelCreate: payment_channel_create_1.default,
        paymentChannelFund: payment_channel_fund_1.default,
        trustline: trustline_1.default,
        amendment: amendment_1.default,
        feeUpdate: fee_update_1.default
    };
    const parser = mapping[type];
    const specification = parser
        ? parser(tx)
        : {
            UNAVAILABLE: 'Unrecognized transaction type.',
            SEE_RAW_TRANSACTION: 'Since this type is unrecognized, `rawTransaction` is included in this response.'
        };
    if (!parser) {
        includeRawTransaction = true;
    }
    const outcome = utils_1.parseOutcome(tx);
    return common_1.removeUndefined({
        type: type,
        address: tx.Account,
        sequence: tx.Sequence,
        id: tx.hash,
        specification: common_1.removeUndefined(specification),
        outcome: outcome ? common_1.removeUndefined(outcome) : undefined,
        rawTransaction: includeRawTransaction ? JSON.stringify(tx) : undefined
    });
}
exports.default = parseTransaction;
//# sourceMappingURL=transaction.js.map