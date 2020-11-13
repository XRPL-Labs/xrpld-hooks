import { CheckLedgerEntry, RippleStateLedgerEntry, OfferLedgerEntry, SignerListLedgerEntry, EscrowLedgerEntry, PayChannelLedgerEntry, DepositPreauthLedgerEntry } from '../objects';
export interface GetAccountObjectsOptions {
    type?: string | ('check' | 'escrow' | 'offer' | 'payment_channel' | 'signer_list' | 'state');
    ledgerHash?: string;
    ledgerIndex?: number | ('validated' | 'closed' | 'current');
    limit?: number;
    marker?: string;
}
export interface AccountObjectsRequest {
    account: string;
    type?: string | ('check' | 'escrow' | 'offer' | 'payment_channel' | 'signer_list' | 'state');
    ledger_hash?: string;
    ledger_index?: number | ('validated' | 'closed' | 'current');
    limit?: number;
    marker?: string;
}
export interface AccountObjectsResponse {
    account: string;
    account_objects: Array<CheckLedgerEntry | RippleStateLedgerEntry | OfferLedgerEntry | SignerListLedgerEntry | EscrowLedgerEntry | PayChannelLedgerEntry | DepositPreauthLedgerEntry>;
    ledger_hash?: string;
    ledger_index?: number;
    ledger_current_index?: number;
    limit?: number;
    marker?: string;
    validated?: boolean;
}
//# sourceMappingURL=account_objects.d.ts.map