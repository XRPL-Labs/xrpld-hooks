"use strict";
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
const bignumber_js_1 = __importDefault(require("bignumber.js"));
const ripple_keypairs_1 = require("ripple-keypairs");
const errors_1 = require("./errors");
function isValidSecret(secret) {
    try {
        ripple_keypairs_1.deriveKeypair(secret);
        return true;
    }
    catch (err) {
        return false;
    }
}
exports.isValidSecret = isValidSecret;
function dropsToXrp(drops) {
    if (typeof drops === 'string') {
        if (!drops.match(/^-?[0-9]*\.?[0-9]*$/)) {
            throw new errors_1.ValidationError(`dropsToXrp: invalid value '${drops}',` +
                ` should be a number matching (^-?[0-9]*\\.?[0-9]*$).`);
        }
        else if (drops === '.') {
            throw new errors_1.ValidationError(`dropsToXrp: invalid value '${drops}',` +
                ` should be a BigNumber or string-encoded number.`);
        }
    }
    drops = new bignumber_js_1.default(drops).toString(10);
    if (drops.includes('.')) {
        throw new errors_1.ValidationError(`dropsToXrp: value '${drops}' has` + ` too many decimal places.`);
    }
    if (!drops.match(/^-?[0-9]+$/)) {
        throw new errors_1.ValidationError(`dropsToXrp: failed sanity check -` +
            ` value '${drops}',` +
            ` does not match (^-?[0-9]+$).`);
    }
    return new bignumber_js_1.default(drops).dividedBy(1000000.0).toString(10);
}
exports.dropsToXrp = dropsToXrp;
function xrpToDrops(xrp) {
    if (typeof xrp === 'string') {
        if (!xrp.match(/^-?[0-9]*\.?[0-9]*$/)) {
            throw new errors_1.ValidationError(`xrpToDrops: invalid value '${xrp}',` +
                ` should be a number matching (^-?[0-9]*\\.?[0-9]*$).`);
        }
        else if (xrp === '.') {
            throw new errors_1.ValidationError(`xrpToDrops: invalid value '${xrp}',` +
                ` should be a BigNumber or string-encoded number.`);
        }
    }
    xrp = new bignumber_js_1.default(xrp).toString(10);
    if (!xrp.match(/^-?[0-9.]+$/)) {
        throw new errors_1.ValidationError(`xrpToDrops: failed sanity check -` +
            ` value '${xrp}',` +
            ` does not match (^-?[0-9.]+$).`);
    }
    const components = xrp.split('.');
    if (components.length > 2) {
        throw new errors_1.ValidationError(`xrpToDrops: failed sanity check -` +
            ` value '${xrp}' has` +
            ` too many decimal points.`);
    }
    const fraction = components[1] || '0';
    if (fraction.length > 6) {
        throw new errors_1.ValidationError(`xrpToDrops: value '${xrp}' has` + ` too many decimal places.`);
    }
    return new bignumber_js_1.default(xrp)
        .times(1000000.0)
        .integerValue(bignumber_js_1.default.ROUND_FLOOR)
        .toString(10);
}
exports.xrpToDrops = xrpToDrops;
function toRippledAmount(amount) {
    if (amount.currency === 'XRP') {
        return xrpToDrops(amount.value);
    }
    if (amount.currency === 'drops') {
        return amount.value;
    }
    return {
        currency: amount.currency,
        issuer: amount.counterparty
            ? amount.counterparty
            : amount.issuer
                ? amount.issuer
                : undefined,
        value: amount.value
    };
}
exports.toRippledAmount = toRippledAmount;
function convertKeysFromSnakeCaseToCamelCase(obj) {
    if (typeof obj === 'object') {
        const accumulator = Array.isArray(obj) ? [] : {};
        let newKey;
        return _.reduce(obj, (result, value, key) => {
            newKey = key;
            const FINDSNAKE = /([a-zA-Z]_[a-zA-Z])/g;
            if (FINDSNAKE.test(key)) {
                newKey = key.replace(FINDSNAKE, r => r[0] + r[2].toUpperCase());
            }
            result[newKey] = convertKeysFromSnakeCaseToCamelCase(value);
            return result;
        }, accumulator);
    }
    return obj;
}
exports.convertKeysFromSnakeCaseToCamelCase = convertKeysFromSnakeCaseToCamelCase;
function removeUndefined(obj) {
    return _.omitBy(obj, _.isUndefined);
}
exports.removeUndefined = removeUndefined;
function rippleToUnixTimestamp(rpepoch) {
    return (rpepoch + 0x386d4380) * 1000;
}
function unixToRippleTimestamp(timestamp) {
    return Math.round(timestamp / 1000) - 0x386d4380;
}
function rippleTimeToISO8601(rippleTime) {
    return new Date(rippleToUnixTimestamp(rippleTime)).toISOString();
}
exports.rippleTimeToISO8601 = rippleTimeToISO8601;
function iso8601ToRippleTime(iso8601) {
    return unixToRippleTimestamp(Date.parse(iso8601));
}
exports.iso8601ToRippleTime = iso8601ToRippleTime;
//# sourceMappingURL=utils.js.map