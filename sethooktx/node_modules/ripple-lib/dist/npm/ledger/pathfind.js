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
const utils_1 = require("./utils");
const common_1 = require("../common");
const pathfind_1 = __importDefault(require("./parse/pathfind"));
const NotFoundError = common_1.errors.NotFoundError;
const ValidationError = common_1.errors.ValidationError;
function addParams(request, result) {
    return _.defaults(_.assign({}, result, {
        source_account: request.source_account,
        source_currencies: request.source_currencies
    }), { destination_amount: request.destination_amount });
}
function requestPathFind(connection, pathfind) {
    const destinationAmount = _.assign({
        value: pathfind.destination.amount.currency === 'XRP' ? common_1.dropsToXrp('-1') : '-1'
    }, pathfind.destination.amount);
    const request = {
        command: 'ripple_path_find',
        source_account: pathfind.source.address,
        destination_account: pathfind.destination.address,
        destination_amount: common_1.toRippledAmount(destinationAmount)
    };
    if (typeof request.destination_amount === 'object' &&
        !request.destination_amount.issuer) {
        request.destination_amount.issuer = request.destination_account;
    }
    if (pathfind.source.currencies && pathfind.source.currencies.length > 0) {
        request.source_currencies = pathfind.source.currencies.map(amount => utils_1.renameCounterpartyToIssuer(amount));
    }
    if (pathfind.source.amount) {
        if (pathfind.destination.amount.value !== undefined) {
            throw new ValidationError('Cannot specify both source.amount' +
                ' and destination.amount.value in getPaths');
        }
        request.send_max = common_1.toRippledAmount(pathfind.source.amount);
        if (typeof request.send_max !== 'string' && !request.send_max.issuer) {
            request.send_max.issuer = pathfind.source.address;
        }
    }
    return connection.request(request).then(paths => addParams(request, paths));
}
function addDirectXrpPath(paths, xrpBalance) {
    const destinationAmount = paths.destination_amount;
    if (new bignumber_js_1.default(xrpBalance).isGreaterThanOrEqualTo(destinationAmount)) {
        paths.alternatives.unshift({
            paths_computed: [],
            source_amount: paths.destination_amount
        });
    }
    return paths;
}
function isRippledIOUAmount(amount) {
    return (typeof amount === 'object' && amount.currency && amount.currency !== 'XRP');
}
function conditionallyAddDirectXRPPath(connection, address, paths) {
    if (isRippledIOUAmount(paths.destination_amount) ||
        !_.includes(paths.destination_currencies, 'XRP')) {
        return Promise.resolve(paths);
    }
    return utils_1.getXRPBalance(connection, address, undefined).then(xrpBalance => addDirectXrpPath(paths, xrpBalance));
}
function filterSourceFundsLowPaths(pathfind, paths) {
    if (pathfind.source.amount &&
        pathfind.destination.amount.value === undefined &&
        paths.alternatives) {
        paths.alternatives = _.filter(paths.alternatives, alt => {
            if (!alt.source_amount) {
                return false;
            }
            const pathfindSourceAmountValue = new bignumber_js_1.default(pathfind.source.amount.currency === 'XRP'
                ? common_1.xrpToDrops(pathfind.source.amount.value)
                : pathfind.source.amount.value);
            const altSourceAmountValue = new bignumber_js_1.default(typeof alt.source_amount === 'string'
                ? alt.source_amount
                : alt.source_amount.value);
            return altSourceAmountValue.eq(pathfindSourceAmountValue);
        });
    }
    return paths;
}
function formatResponse(pathfind, paths) {
    if (paths.alternatives && paths.alternatives.length > 0) {
        return pathfind_1.default(paths);
    }
    if (paths.destination_currencies !== undefined &&
        !_.includes(paths.destination_currencies, pathfind.destination.amount.currency)) {
        throw new NotFoundError('No paths found. ' +
            'The destination_account does not accept ' +
            pathfind.destination.amount.currency +
            ', they only accept: ' +
            paths.destination_currencies.join(', '));
    }
    else if (paths.source_currencies && paths.source_currencies.length > 0) {
        throw new NotFoundError('No paths found. Please ensure' +
            ' that the source_account has sufficient funds to execute' +
            ' the payment in one of the specified source_currencies. If it does' +
            ' there may be insufficient liquidity in the network to execute' +
            ' this payment right now');
    }
    else {
        throw new NotFoundError('No paths found.' +
            ' Please ensure that the source_account has sufficient funds to' +
            ' execute the payment. If it does there may be insufficient liquidity' +
            ' in the network to execute this payment right now');
    }
}
function getPaths(pathfind) {
    common_1.validate.getPaths({ pathfind });
    const address = pathfind.source.address;
    return requestPathFind(this.connection, pathfind)
        .then(paths => conditionallyAddDirectXRPPath(this.connection, address, paths))
        .then(paths => filterSourceFundsLowPaths(pathfind, paths))
        .then(paths => formatResponse(pathfind, paths));
}
exports.default = getPaths;
//# sourceMappingURL=pathfind.js.map