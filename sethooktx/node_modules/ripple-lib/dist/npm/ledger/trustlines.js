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
const common_1 = require("../common");
const account_trustline_1 = __importDefault(require("./parse/account-trustline"));
function currencyFilter(currency, trustline) {
    return currency === null || trustline.specification.currency === currency;
}
function getTrustlines(address, options = {}) {
    return __awaiter(this, void 0, void 0, function* () {
        common_1.validate.getTrustlines({ address, options });
        address = common_1.ensureClassicAddress(address);
        const responses = yield this._requestAll('account_lines', {
            account: address,
            ledger_index: yield this.getLedgerVersion(),
            limit: options.limit,
            peer: options.counterparty
        });
        const trustlines = _.flatMap(responses, response => response.lines);
        return trustlines.map(account_trustline_1.default).filter(trustline => {
            return currencyFilter(options.currency || null, trustline);
        });
    });
}
exports.default = getTrustlines;
//# sourceMappingURL=trustlines.js.map