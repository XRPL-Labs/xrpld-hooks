"use strict";
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (Object.hasOwnProperty.call(mod, k)) result[k] = mod[k];
    result["default"] = mod;
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
const util_1 = require("util");
const browserHacks = __importStar(require("./browser-hacks"));
class RippleError extends Error {
    constructor(message = '', data) {
        super(message);
        this.name = browserHacks.getConstructorName(this);
        this.message = message;
        this.data = data;
        if (Error.captureStackTrace) {
            Error.captureStackTrace(this, this.constructor);
        }
    }
    toString() {
        let result = '[' + this.name + '(' + this.message;
        if (this.data) {
            result += ', ' + util_1.inspect(this.data);
        }
        result += ')]';
        return result;
    }
    inspect() {
        return this.toString();
    }
}
exports.RippleError = RippleError;
class RippledError extends RippleError {
}
exports.RippledError = RippledError;
class UnexpectedError extends RippleError {
}
exports.UnexpectedError = UnexpectedError;
class LedgerVersionError extends RippleError {
}
exports.LedgerVersionError = LedgerVersionError;
class ConnectionError extends RippleError {
}
exports.ConnectionError = ConnectionError;
class NotConnectedError extends ConnectionError {
}
exports.NotConnectedError = NotConnectedError;
class DisconnectedError extends ConnectionError {
}
exports.DisconnectedError = DisconnectedError;
class RippledNotInitializedError extends ConnectionError {
}
exports.RippledNotInitializedError = RippledNotInitializedError;
class TimeoutError extends ConnectionError {
}
exports.TimeoutError = TimeoutError;
class ResponseFormatError extends ConnectionError {
}
exports.ResponseFormatError = ResponseFormatError;
class ValidationError extends RippleError {
}
exports.ValidationError = ValidationError;
class NotFoundError extends RippleError {
    constructor(message = 'Not found') {
        super(message);
    }
}
exports.NotFoundError = NotFoundError;
class MissingLedgerHistoryError extends RippleError {
    constructor(message) {
        super(message || 'Server is missing ledger history in the specified range');
    }
}
exports.MissingLedgerHistoryError = MissingLedgerHistoryError;
class PendingLedgerVersionError extends RippleError {
    constructor(message) {
        super(message ||
            "maxLedgerVersion is greater than server's most recent" +
                ' validated ledger');
    }
}
exports.PendingLedgerVersionError = PendingLedgerVersionError;
//# sourceMappingURL=errors.js.map