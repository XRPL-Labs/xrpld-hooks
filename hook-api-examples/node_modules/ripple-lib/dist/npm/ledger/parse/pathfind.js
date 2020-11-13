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
const amount_1 = __importDefault(require("./amount"));
function parsePaths(paths) {
    return paths.map(steps => steps.map(step => _.omit(step, ['type', 'type_hex'])));
}
function removeAnyCounterpartyEncoding(address, amount) {
    return amount.counterparty === address
        ? _.omit(amount, 'counterparty')
        : amount;
}
function createAdjustment(address, adjustmentWithoutAddress) {
    const amountKey = _.keys(adjustmentWithoutAddress)[0];
    const amount = adjustmentWithoutAddress[amountKey];
    return _.set({ address: address }, amountKey, removeAnyCounterpartyEncoding(address, amount));
}
function parseAlternative(sourceAddress, destinationAddress, destinationAmount, alternative) {
    const amounts = alternative.destination_amount !== undefined
        ? {
            source: { amount: amount_1.default(alternative.source_amount) },
            destination: { minAmount: amount_1.default(alternative.destination_amount) }
        }
        : {
            source: { maxAmount: amount_1.default(alternative.source_amount) },
            destination: { amount: amount_1.default(destinationAmount) }
        };
    return {
        source: createAdjustment(sourceAddress, amounts.source),
        destination: createAdjustment(destinationAddress, amounts.destination),
        paths: JSON.stringify(parsePaths(alternative.paths_computed))
    };
}
function parsePathfind(pathfindResult) {
    const sourceAddress = pathfindResult.source_account;
    const destinationAddress = pathfindResult.destination_account;
    const destinationAmount = pathfindResult.destination_amount;
    return pathfindResult.alternatives.map(alt => parseAlternative(sourceAddress, destinationAddress, destinationAmount, alt));
}
exports.default = parsePathfind;
//# sourceMappingURL=pathfind.js.map