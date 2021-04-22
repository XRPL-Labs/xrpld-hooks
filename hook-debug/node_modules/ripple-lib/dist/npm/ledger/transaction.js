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
const utils = __importStar(require("./utils"));
const transaction_1 = __importDefault(require("./parse/transaction"));
const common_1 = require("../common");
function attachTransactionDate(connection, tx) {
    if (tx.date) {
        return Promise.resolve(tx);
    }
    const ledgerVersion = tx.ledger_index || tx.LedgerSequence;
    if (!ledgerVersion) {
        return new Promise(() => {
            const error = new common_1.errors.NotFoundError('Transaction has not been validated yet; try again later');
            error.data = {
                details: '(ledger_index and LedgerSequence not found in tx)'
            };
            throw error;
        });
    }
    const request = {
        command: 'ledger',
        ledger_index: ledgerVersion
    };
    return connection
        .request(request)
        .then(data => {
        if (typeof data.ledger.close_time === 'number') {
            return _.assign({ date: data.ledger.close_time }, tx);
        }
        throw new common_1.errors.UnexpectedError('Ledger missing close_time');
    })
        .catch(error => {
        if (error instanceof common_1.errors.UnexpectedError) {
            throw error;
        }
        throw new common_1.errors.NotFoundError('Transaction ledger not found');
    });
}
function isTransactionInRange(tx, options) {
    return ((!options.minLedgerVersion ||
        tx.ledger_index >= options.minLedgerVersion) &&
        (!options.maxLedgerVersion || tx.ledger_index <= options.maxLedgerVersion));
}
function convertError(connection, options, error) {
    let shouldUseNotFoundError = false;
    if ((error.data && error.data.error === 'txnNotFound') ||
        error.message === 'txnNotFound') {
        shouldUseNotFoundError = true;
    }
    const _error = shouldUseNotFoundError
        ? new common_1.errors.NotFoundError('Transaction not found')
        : error;
    if (_error instanceof common_1.errors.NotFoundError) {
        return utils
            .hasCompleteLedgerRange(connection, options.minLedgerVersion, options.maxLedgerVersion)
            .then(hasCompleteLedgerRange => {
            if (!hasCompleteLedgerRange) {
                return utils
                    .isPendingLedgerVersion(connection, options.maxLedgerVersion)
                    .then(isPendingLedgerVersion => {
                    return isPendingLedgerVersion
                        ? new common_1.errors.PendingLedgerVersionError()
                        : new common_1.errors.MissingLedgerHistoryError();
                });
            }
            return _error;
        });
    }
    return Promise.resolve(_error);
}
function formatResponse(options, tx) {
    if (tx.validated !== true || !isTransactionInRange(tx, options)) {
        throw new common_1.errors.NotFoundError('Transaction not found');
    }
    return transaction_1.default(tx, options.includeRawTransaction);
}
function getTransaction(id, options = {}) {
    return __awaiter(this, void 0, void 0, function* () {
        common_1.validate.getTransaction({ id, options });
        const _options = yield utils.ensureLedgerVersion.call(this, options);
        try {
            const tx = yield this.request('tx', {
                transaction: id,
                binary: false
            });
            const txWithDate = yield attachTransactionDate(this.connection, tx);
            return formatResponse(_options, txWithDate);
        }
        catch (error) {
            throw yield convertError(this.connection, _options, error);
        }
    });
}
exports.default = getTransaction;
//# sourceMappingURL=transaction.js.map