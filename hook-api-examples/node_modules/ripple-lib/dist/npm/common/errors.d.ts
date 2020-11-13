declare class RippleError extends Error {
    name: string;
    message: string;
    data?: any;
    constructor(message?: string, data?: any);
    toString(): string;
    inspect(): string;
}
declare class RippledError extends RippleError {
}
declare class UnexpectedError extends RippleError {
}
declare class LedgerVersionError extends RippleError {
}
declare class ConnectionError extends RippleError {
}
declare class NotConnectedError extends ConnectionError {
}
declare class DisconnectedError extends ConnectionError {
}
declare class RippledNotInitializedError extends ConnectionError {
}
declare class TimeoutError extends ConnectionError {
}
declare class ResponseFormatError extends ConnectionError {
}
declare class ValidationError extends RippleError {
}
declare class NotFoundError extends RippleError {
    constructor(message?: string);
}
declare class MissingLedgerHistoryError extends RippleError {
    constructor(message?: string);
}
declare class PendingLedgerVersionError extends RippleError {
    constructor(message?: string);
}
export { RippleError, UnexpectedError, ConnectionError, RippledError, NotConnectedError, DisconnectedError, RippledNotInitializedError, TimeoutError, ResponseFormatError, ValidationError, NotFoundError, PendingLedgerVersionError, MissingLedgerHistoryError, LedgerVersionError };
//# sourceMappingURL=errors.d.ts.map