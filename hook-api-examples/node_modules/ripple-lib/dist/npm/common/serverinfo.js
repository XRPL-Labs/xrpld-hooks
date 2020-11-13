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
const utils_1 = require("./utils");
const bignumber_js_1 = __importDefault(require("bignumber.js"));
function renameKeys(object, mapping) {
    _.forEach(mapping, (to, from) => {
        object[to] = object[from];
        delete object[from];
    });
}
function getServerInfo() {
    return this.request('server_info').then(response => {
        const info = utils_1.convertKeysFromSnakeCaseToCamelCase(response.info);
        renameKeys(info, { hostid: 'hostID' });
        if (info.validatedLedger) {
            renameKeys(info.validatedLedger, {
                baseFeeXrp: 'baseFeeXRP',
                reserveBaseXrp: 'reserveBaseXRP',
                reserveIncXrp: 'reserveIncrementXRP',
                seq: 'ledgerVersion'
            });
            info.validatedLedger.baseFeeXRP = info.validatedLedger.baseFeeXRP.toString();
            info.validatedLedger.reserveBaseXRP = info.validatedLedger.reserveBaseXRP.toString();
            info.validatedLedger.reserveIncrementXRP = info.validatedLedger.reserveIncrementXRP.toString();
        }
        return info;
    });
}
exports.getServerInfo = getServerInfo;
function getFee(cushion) {
    return __awaiter(this, void 0, void 0, function* () {
        if (cushion === undefined) {
            cushion = this._feeCushion;
        }
        if (cushion === undefined) {
            cushion = 1.2;
        }
        const serverInfo = (yield this.request('server_info')).info;
        const baseFeeXrp = new bignumber_js_1.default(serverInfo.validated_ledger.base_fee_xrp);
        let fee = baseFeeXrp.times(serverInfo.load_factor).times(cushion);
        fee = bignumber_js_1.default.min(fee, this._maxFeeXRP);
        return new bignumber_js_1.default(fee.toFixed(6)).toString(10);
    });
}
exports.getFee = getFee;
//# sourceMappingURL=serverinfo.js.map