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
const fields_1 = __importDefault(require("./parse/fields"));
const common_1 = require("../common");
const AccountFlags = common_1.constants.AccountFlags;
function parseAccountFlags(value, options = {}) {
    const settings = {};
    for (const flagName in AccountFlags) {
        if (value & AccountFlags[flagName]) {
            settings[flagName] = true;
        }
        else {
            if (!options.excludeFalse) {
                settings[flagName] = false;
            }
        }
    }
    return settings;
}
exports.parseAccountFlags = parseAccountFlags;
function formatSettings(response) {
    const data = response.account_data;
    const parsedFlags = parseAccountFlags(data.Flags, { excludeFalse: true });
    const parsedFields = fields_1.default(data);
    return _.assign({}, parsedFlags, parsedFields);
}
function getSettings(address, options = {}) {
    return __awaiter(this, void 0, void 0, function* () {
        common_1.validate.getSettings({ address, options });
        address = common_1.ensureClassicAddress(address);
        const response = yield this.request('account_info', {
            account: address,
            ledger_index: options.ledgerVersion || 'validated',
            signer_lists: true
        });
        return formatSettings(response);
    });
}
exports.getSettings = getSettings;
//# sourceMappingURL=settings.js.map