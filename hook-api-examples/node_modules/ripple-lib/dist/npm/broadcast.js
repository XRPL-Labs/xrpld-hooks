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
Object.defineProperty(exports, "__esModule", { value: true });
const _ = __importStar(require("lodash"));
const api_1 = require("./api");
class RippleAPIBroadcast extends api_1.RippleAPI {
    constructor(servers, options = {}) {
        super(options);
        this.ledgerVersion = undefined;
        const apis = servers.map(server => new api_1.RippleAPI(_.assign({}, options, { server })));
        this._apis = apis;
        this.getMethodNames().forEach(name => {
            this[name] = function () {
                return Promise.race(apis.map(api => api[name](...arguments)));
            };
        });
        this.connect = function () {
            return __awaiter(this, void 0, void 0, function* () {
                yield Promise.all(apis.map(api => api.connect()));
            });
        };
        this.disconnect = function () {
            return __awaiter(this, void 0, void 0, function* () {
                yield Promise.all(apis.map(api => api.disconnect()));
            });
        };
        this.isConnected = function () {
            return apis.map(api => api.isConnected()).every(Boolean);
        };
        const defaultAPI = apis[0];
        const syncMethods = ['sign', 'generateAddress', 'computeLedgerHash'];
        syncMethods.forEach(name => {
            this[name] = defaultAPI[name].bind(defaultAPI);
        });
        apis.forEach(api => {
            api.on('ledger', this.onLedgerEvent.bind(this));
            api.on('error', (errorCode, errorMessage, data) => this.emit('error', errorCode, errorMessage, data));
        });
    }
    onLedgerEvent(ledger) {
        if (ledger.ledgerVersion > this.ledgerVersion ||
            this.ledgerVersion === undefined) {
            this.ledgerVersion = ledger.ledgerVersion;
            this.emit('ledger', ledger);
        }
    }
    getMethodNames() {
        const methodNames = [];
        const rippleAPI = this._apis[0];
        for (const name of Object.getOwnPropertyNames(rippleAPI)) {
            if (typeof rippleAPI[name] === 'function') {
                methodNames.push(name);
            }
        }
        return methodNames;
    }
}
exports.RippleAPIBroadcast = RippleAPIBroadcast;
//# sourceMappingURL=broadcast.js.map