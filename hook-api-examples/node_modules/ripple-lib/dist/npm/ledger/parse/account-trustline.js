"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const utils_1 = require("./utils");
const common_1 = require("../../common");
function parseAccountTrustline(trustline) {
    const specification = common_1.removeUndefined({
        limit: trustline.limit,
        currency: trustline.currency,
        counterparty: trustline.account,
        qualityIn: utils_1.parseQuality(trustline.quality_in) || undefined,
        qualityOut: utils_1.parseQuality(trustline.quality_out) || undefined,
        ripplingDisabled: trustline.no_ripple || undefined,
        frozen: trustline.freeze || undefined,
        authorized: trustline.authorized || undefined
    });
    const counterparty = common_1.removeUndefined({
        limit: trustline.limit_peer,
        ripplingDisabled: trustline.no_ripple_peer || undefined,
        frozen: trustline.freeze_peer || undefined,
        authorized: trustline.peer_authorized || undefined
    });
    const state = {
        balance: trustline.balance
    };
    return { specification, counterparty, state };
}
exports.default = parseAccountTrustline;
//# sourceMappingURL=account-trustline.js.map