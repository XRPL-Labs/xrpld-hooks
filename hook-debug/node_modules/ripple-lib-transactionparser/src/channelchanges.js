'use strict'

const normalizeNodes = require('./utils').normalizeNodes
const BigNumber = require('bignumber.js')

function parsePaymentChannelStatus(node) {
  if (node.diffType === 'CreatedNode') {
    return 'created'
  }

  if (node.diffType === 'ModifiedNode') {
    return 'modified'
  }

  if (node.diffType === 'DeletedNode') {
    return 'deleted'
  }
  return undefined
}

function summarizePaymentChannel(node) {

  const final = (node.diffType === 'CreatedNode') ?
    node.newFields : node.finalFields
  const prev = node.previousFields || {}

  const summary = {
    // Status may be 'created', 'modified', or 'deleted'.
    status: parsePaymentChannelStatus(node),

    // The LedgerIndex indicates the Channel ID,
    // which is necessary to sign claims.
    channelId: node.ledgerIndex,

    // The source address that owns this payment channel.
    // This comes from the sending address of the
    // transaction that created the channel.
    source: final.Account,

    // The destination address for this payment channel.
    // While the payment channel is open, this address is the only one that can receive
    // XRP from the channel. This comes from the Destination field of the transaction
    // that created the channel.
    destination: final.Destination,

    // Total XRP, in drops, that has been allocated to this channel.
    // This includes XRP that has been paid to the destination address.
    // This is initially set by the transaction that created the channel and
    // can be increased if the source address sends a PaymentChannelFund transaction.
    channelAmountDrops:
          new BigNumber(final.Amount || 0).toString(10),

    // Total XRP, in drops, already paid out by the channel.
    // The difference between this value and the Amount field is how much XRP can still
    // be paid to the destination address with PaymentChannelClaim transactions.
    // If the channel closes, the remaining difference is returned to the source address.
    channelBalanceDrops:
          new BigNumber(final.Balance || 0).toString(10)
  }

  if (prev.Amount) {
    // The change in the number of XRP drops allocated to this channel.
    // This is positive if this is a PaymentChannelFund transaction.
    summary.channelAmountChangeDrops = new BigNumber(final.Amount)
      .minus(new BigNumber(prev.Amount || 0))
      .toString(10)
  }

  if (prev.Balance) {
    // The change in the number of XRP drops already paid out by the channel.
    summary.channelBalanceChangeDrops = new BigNumber(final.Balance)
      .minus(new BigNumber(prev.Balance || 0))
      .toString(10)
  }

  if (node.PreviousTxnID) {
    // The identifying hash of the transaction that
    // most recently modified this payment channel object.
    // You can use this to retrieve the object's history.
    summary.previousTxnId = node.PreviousTxnID
  }

  return summary
}

function parseChannelChanges(metadata) {
  const paymentChannels = normalizeNodes(metadata)
    .filter(n => {
      return n.entryType === 'PayChannel'
    })

  return (paymentChannels.length === 1) ?
    summarizePaymentChannel(paymentChannels[0]) :
    undefined
}

module.exports.parseChannelChanges = parseChannelChanges
