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
const assert = __importStar(require("assert"));
const { Validator } = require('jsonschema');
const errors_1 = require("./errors");
const ripple_address_codec_1 = require("ripple-address-codec");
const utils_1 = require("./utils");
exports.isValidSecret = utils_1.isValidSecret;
function loadSchemas() {
    const schemas = [
        require('./schemas/objects/tx-json.json'),
        require('./schemas/objects/transaction-type.json'),
        require('./schemas/objects/hash128.json'),
        require('./schemas/objects/hash256.json'),
        require('./schemas/objects/sequence.json'),
        require('./schemas/objects/signature.json'),
        require('./schemas/objects/issue.json'),
        require('./schemas/objects/ledger-version.json'),
        require('./schemas/objects/max-adjustment.json'),
        require('./schemas/objects/memo.json'),
        require('./schemas/objects/memos.json'),
        require('./schemas/objects/public-key.json'),
        require('./schemas/objects/private-key.json'),
        require('./schemas/objects/uint32.json'),
        require('./schemas/objects/value.json'),
        require('./schemas/objects/source-adjustment.json'),
        require('./schemas/objects/destination-adjustment.json'),
        require('./schemas/objects/tag.json'),
        require('./schemas/objects/lax-amount.json'),
        require('./schemas/objects/lax-lax-amount.json'),
        require('./schemas/objects/min-adjustment.json'),
        require('./schemas/objects/source-exact-adjustment.json'),
        require('./schemas/objects/destination-exact-adjustment.json'),
        require('./schemas/objects/destination-address-tag.json'),
        require('./schemas/objects/transaction-hash.json'),
        require('./schemas/objects/address.json'),
        require('./schemas/objects/x-address.json'),
        require('./schemas/objects/classic-address.json'),
        require('./schemas/objects/adjustment.json'),
        require('./schemas/objects/quality.json'),
        require('./schemas/objects/amount.json'),
        require('./schemas/objects/amountbase.json'),
        require('./schemas/objects/balance.json'),
        require('./schemas/objects/blob.json'),
        require('./schemas/objects/currency.json'),
        require('./schemas/objects/signed-value.json'),
        require('./schemas/objects/orderbook.json'),
        require('./schemas/objects/instructions.json'),
        require('./schemas/objects/settings-plus-memos.json'),
        require('./schemas/specifications/settings.json'),
        require('./schemas/specifications/payment.json'),
        require('./schemas/specifications/get-payment.json'),
        require('./schemas/specifications/escrow-cancellation.json'),
        require('./schemas/specifications/order-cancellation.json'),
        require('./schemas/specifications/order.json'),
        require('./schemas/specifications/escrow-execution.json'),
        require('./schemas/specifications/escrow-creation.json'),
        require('./schemas/specifications/payment-channel-create.json'),
        require('./schemas/specifications/payment-channel-fund.json'),
        require('./schemas/specifications/payment-channel-claim.json'),
        require('./schemas/specifications/check-create.json'),
        require('./schemas/specifications/check-cash.json'),
        require('./schemas/specifications/check-cancel.json'),
        require('./schemas/specifications/trustline.json'),
        require('./schemas/specifications/deposit-preauth.json'),
        require('./schemas/specifications/account-delete.json'),
        require('./schemas/output/sign.json'),
        require('./schemas/output/submit.json'),
        require('./schemas/output/get-account-info.json'),
        require('./schemas/output/get-account-objects.json'),
        require('./schemas/output/get-balances.json'),
        require('./schemas/output/get-balance-sheet.json'),
        require('./schemas/output/get-ledger.json'),
        require('./schemas/output/get-orderbook.json'),
        require('./schemas/output/get-orders.json'),
        require('./schemas/output/order-change.json'),
        require('./schemas/output/get-payment-channel.json'),
        require('./schemas/output/prepare.json'),
        require('./schemas/output/ledger-event.json'),
        require('./schemas/output/get-paths.json'),
        require('./schemas/output/get-server-info.json'),
        require('./schemas/output/get-settings.json'),
        require('./schemas/output/orderbook-orders.json'),
        require('./schemas/output/outcome.json'),
        require('./schemas/output/get-transaction.json'),
        require('./schemas/output/get-transactions.json'),
        require('./schemas/output/get-trustlines.json'),
        require('./schemas/output/sign-payment-channel-claim.json'),
        require('./schemas/output/verify-payment-channel-claim.json'),
        require('./schemas/input/get-balances.json'),
        require('./schemas/input/get-balance-sheet.json'),
        require('./schemas/input/get-ledger.json'),
        require('./schemas/input/get-orders.json'),
        require('./schemas/input/get-orderbook.json'),
        require('./schemas/input/get-paths.json'),
        require('./schemas/input/get-payment-channel.json'),
        require('./schemas/input/api-options.json'),
        require('./schemas/input/get-settings.json'),
        require('./schemas/input/get-account-info.json'),
        require('./schemas/input/get-account-objects.json'),
        require('./schemas/input/get-transaction.json'),
        require('./schemas/input/get-transactions.json'),
        require('./schemas/input/get-trustlines.json'),
        require('./schemas/input/prepare-payment.json'),
        require('./schemas/input/prepare-order.json'),
        require('./schemas/input/prepare-trustline.json'),
        require('./schemas/input/prepare-order-cancellation.json'),
        require('./schemas/input/prepare-settings.json'),
        require('./schemas/input/prepare-escrow-creation.json'),
        require('./schemas/input/prepare-escrow-cancellation.json'),
        require('./schemas/input/prepare-escrow-execution.json'),
        require('./schemas/input/prepare-payment-channel-create.json'),
        require('./schemas/input/prepare-payment-channel-fund.json'),
        require('./schemas/input/prepare-payment-channel-claim.json'),
        require('./schemas/input/prepare-check-create.json'),
        require('./schemas/input/prepare-check-cash.json'),
        require('./schemas/input/prepare-check-cancel.json'),
        require('./schemas/input/compute-ledger-hash.json'),
        require('./schemas/input/sign.json'),
        require('./schemas/input/submit.json'),
        require('./schemas/input/generate-address.json'),
        require('./schemas/input/sign-payment-channel-claim.json'),
        require('./schemas/input/verify-payment-channel-claim.json'),
        require('./schemas/input/combine.json')
    ];
    const titles = schemas.map(schema => schema.title);
    const duplicates = _.keys(_.pickBy(_.countBy(titles), count => count > 1));
    assert.ok(duplicates.length === 0, 'Duplicate schemas for: ' + duplicates);
    const validator = new Validator();
    validator.customFormats.xAddress = function (instance) {
        if (instance === undefined) {
            return true;
        }
        return ripple_address_codec_1.isValidXAddress(instance);
    };
    validator.customFormats.classicAddress = function (instance) {
        if (instance === undefined) {
            return true;
        }
        return isValidAddress(instance);
    };
    validator.customFormats.secret = function (instance) {
        if (instance === undefined) {
            return true;
        }
        return utils_1.isValidSecret(instance);
    };
    _.forEach(schemas, schema => validator.addSchema(schema, '/' + schema.title));
    return validator;
}
const schemaValidator = loadSchemas();
function schemaValidate(schemaName, object) {
    const schema = schemaValidator.getSchema('/' + schemaName);
    if (schema === undefined) {
        throw new errors_1.ValidationError('no schema for ' + schemaName);
    }
    const result = schemaValidator.validate(object, schema);
    if (!result.valid) {
        throw new errors_1.ValidationError(result.errors.join());
    }
}
exports.schemaValidate = schemaValidate;
function isValidAddress(address) {
    return ripple_address_codec_1.isValidXAddress(address) || ripple_address_codec_1.isValidClassicAddress(address);
}
exports.isValidAddress = isValidAddress;
//# sourceMappingURL=schema-validator.js.map