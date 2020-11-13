"use strict";
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (Object.hasOwnProperty.call(mod, k)) result[k] = mod[k];
    result["default"] = mod;
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
const _ = __importStar(require("lodash"));
const errors_1 = require("./errors");
const schema_validator_1 = require("./schema-validator");
function error(text) {
    return new errors_1.ValidationError(text);
}
function validateLedgerRange(options) {
    if (!_.isUndefined(options) &&
        !_.isUndefined(options.minLedgerVersion) &&
        !_.isUndefined(options.maxLedgerVersion)) {
        if (Number(options.minLedgerVersion) > Number(options.maxLedgerVersion)) {
            throw error('minLedgerVersion must not be greater than maxLedgerVersion');
        }
    }
}
function validateOptions(schema, instance) {
    schema_validator_1.schemaValidate(schema, instance);
    validateLedgerRange(instance.options);
}
exports.getPaths = _.partial(schema_validator_1.schemaValidate, 'getPathsParameters');
exports.getTransactions = _.partial(validateOptions, 'getTransactionsParameters');
exports.getSettings = _.partial(validateOptions, 'getSettingsParameters');
exports.getAccountInfo = _.partial(validateOptions, 'getAccountInfoParameters');
exports.getTrustlines = _.partial(validateOptions, 'getTrustlinesParameters');
exports.getBalances = _.partial(validateOptions, 'getBalancesParameters');
exports.getBalanceSheet = _.partial(validateOptions, 'getBalanceSheetParameters');
exports.getOrders = _.partial(validateOptions, 'getOrdersParameters');
exports.getOrderbook = _.partial(validateOptions, 'getOrderbookParameters');
exports.getTransaction = _.partial(validateOptions, 'getTransactionParameters');
exports.getPaymentChannel = _.partial(validateOptions, 'getPaymentChannelParameters');
exports.getLedger = _.partial(validateOptions, 'getLedgerParameters');
exports.preparePayment = _.partial(schema_validator_1.schemaValidate, 'preparePaymentParameters');
exports.prepareOrder = _.partial(schema_validator_1.schemaValidate, 'prepareOrderParameters');
exports.prepareOrderCancellation = _.partial(schema_validator_1.schemaValidate, 'prepareOrderCancellationParameters');
exports.prepareTrustline = _.partial(schema_validator_1.schemaValidate, 'prepareTrustlineParameters');
exports.prepareSettings = _.partial(schema_validator_1.schemaValidate, 'prepareSettingsParameters');
exports.prepareEscrowCreation = _.partial(schema_validator_1.schemaValidate, 'prepareEscrowCreationParameters');
exports.prepareEscrowCancellation = _.partial(schema_validator_1.schemaValidate, 'prepareEscrowCancellationParameters');
exports.prepareEscrowExecution = _.partial(schema_validator_1.schemaValidate, 'prepareEscrowExecutionParameters');
exports.preparePaymentChannelCreate = _.partial(schema_validator_1.schemaValidate, 'preparePaymentChannelCreateParameters');
exports.preparePaymentChannelFund = _.partial(schema_validator_1.schemaValidate, 'preparePaymentChannelFundParameters');
exports.preparePaymentChannelClaim = _.partial(schema_validator_1.schemaValidate, 'preparePaymentChannelClaimParameters');
exports.prepareCheckCreate = _.partial(schema_validator_1.schemaValidate, 'prepareCheckCreateParameters');
exports.prepareCheckCash = _.partial(schema_validator_1.schemaValidate, 'prepareCheckCashParameters');
exports.prepareCheckCancel = _.partial(schema_validator_1.schemaValidate, 'prepareCheckCancelParameters');
exports.sign = _.partial(schema_validator_1.schemaValidate, 'signParameters');
exports.combine = _.partial(schema_validator_1.schemaValidate, 'combineParameters');
exports.submit = _.partial(schema_validator_1.schemaValidate, 'submitParameters');
exports.computeLedgerHash = _.partial(schema_validator_1.schemaValidate, 'computeLedgerHashParameters');
exports.generateAddress = _.partial(schema_validator_1.schemaValidate, 'generateAddressParameters');
exports.signPaymentChannelClaim = _.partial(schema_validator_1.schemaValidate, 'signPaymentChannelClaimParameters');
exports.verifyPaymentChannelClaim = _.partial(schema_validator_1.schemaValidate, 'verifyPaymentChannelClaimParameters');
exports.apiOptions = _.partial(schema_validator_1.schemaValidate, 'api-options');
exports.instructions = _.partial(schema_validator_1.schemaValidate, 'instructions');
exports.tx_json = _.partial(schema_validator_1.schemaValidate, 'tx-json');
//# sourceMappingURL=validate.js.map