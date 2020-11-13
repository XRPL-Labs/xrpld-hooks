'use strict'

var assert = require('assert-diff')
var parseChannelChanges = require('../src/index').parseChannelChanges

describe('parseChannelChanges', function() {
  it('parses PayChannel metadata', function() {
    var metadata = {
      'AffectedNodes': [
        {
          'ModifiedNode': {
            'FinalFields': {
              'Account': 'rJNa71cLCjzQG68oNjh4fCUqCZSGNkWDrM',
              'Balance': '512724201',
              'Flags': 0,
              'OwnerCount': 98,
              'Sequence': 24166
            },
            'LedgerEntryType': 'AccountRoot',
            'LedgerIndex': 'D16BF3F23AFB01CA5AC7860F9AF8037117972D5A389DDFDBB5A1064742B154D8',
            'PreviousFields': {
              'Balance': '512714213',
              'Sequence': 24165
            },
            'PreviousTxnID': '9345E3B3578F8F6C8DC55F7B39F7C48F9E79E7BD2A77ADE2714D5910C501980F',
            'PreviousTxnLgrSeq': 39792515
          }
        },
        {
          'ModifiedNode': {
            'FinalFields': {
              'Account': 'rpyC4JM5kifsNG6YbARDAxAJQLBDZw9ZFQ',
              'Amount': '10000000',
              'Balance': '40000',
              'Destination': 'rJNa71cLCjzQG68oNjh4fCUqCZSGNkWDrM',
              'Flags': 0,
              'OwnerNode': '0000000000000000',
              'PublicKey': 'ED4DB1CE76AB25FAACE3E13BF57EA7767614FB52A250E1D04426A28B383A31A652',
              'SettleDelay': 3600,
              'SourceTag': 3382712545
            },
            'LedgerEntryType': 'PayChannel',
            'LedgerIndex': 'EC4DACE3360DCBF76FE80874931F2C75C5B4B6A05D615FA3E62DFF2BE34A8ACB',
            'PreviousFields': {
              'Balance': '30000'
            },
            'PreviousTxnID': '5E854E78A51C45FC626C61924C20EA25670EF18E5609DA5F1F7898E833DCB257',
            'PreviousTxnLgrSeq': 39749577
          }
        }
      ],
      'TransactionIndex': 24,
      'TransactionResult': 'tesSUCCESS'
    }
    var result = parseChannelChanges(metadata)
    var expectedResult = {
      status: 'modified',
      channelId: 'EC4DACE3360DCBF76FE80874931F2C75C5B4B6A05D615FA3E62DFF2BE34A8ACB',
      source: 'rpyC4JM5kifsNG6YbARDAxAJQLBDZw9ZFQ',
      destination: 'rJNa71cLCjzQG68oNjh4fCUqCZSGNkWDrM',
      channelBalanceChangeDrops: '10000',
      channelAmountDrops: '10000000',
      channelBalanceDrops: '40000',
      previousTxnId: '5E854E78A51C45FC626C61924C20EA25670EF18E5609DA5F1F7898E833DCB257'
    }
    assert.deepEqual(result, expectedResult)
  })
})
