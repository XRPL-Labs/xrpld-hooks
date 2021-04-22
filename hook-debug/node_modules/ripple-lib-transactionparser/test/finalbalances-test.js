'use strict'

var assert = require('assert-diff')
var fs = require('fs')
var parseFinalBalances = require('../src/index').parseFinalBalances

// Pay 100 XRP from rKmB to rLDY to create rLDY account
var createAccountBalanceChanges = {
  rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K: [
    {
      value: '100',
      currency: 'XRP',
      counterparty: ''
    }
  ],
  rKmBGxocj9Abgy25J51Mk1iqFzW9aVF9Tc: [
    {
      value: '339.903994',
      currency: 'XRP',
      counterparty: ''
    }
  ]
}

// Pay 0.01 USD from rKmB to rLDY where rLDY starts with no USD
var usdFirstPaymentBalanceChanges = {
  rKmBGxocj9Abgy25J51Mk1iqFzW9aVF9Tc: [
    {
      value: '1.535330905250352',
      currency: 'USD',
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q'
    },
    {
      value: '239.807992',
      currency: 'XRP',
      counterparty: ''
    }
  ],
  rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q: [
    {
      counterparty: 'rKmBGxocj9Abgy25J51Mk1iqFzW9aVF9Tc',
      currency: 'USD',
      value: '-1.535330905250352'
    },
    {
      counterparty: 'rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K',
      currency: 'USD',
      value: '-0.01'
    }
  ],
  rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K: [
    {
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q',
      currency: 'USD',
      value: '0.01'
    }
  ]
}

// Pay 0.2 USD from rLDY to rKmB where rLDY starts with 0.2 USD
var usdFullPaymentBalanceChanges = {
  rKmBGxocj9Abgy25J51Mk1iqFzW9aVF9Tc: [
    {
      value: '1.545330905250352',
      currency: 'USD',
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q'
    }
  ],
  rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q: [
    {
      counterparty: 'rKmBGxocj9Abgy25J51Mk1iqFzW9aVF9Tc',
      currency: 'USD',
      value: '-1.545330905250352'
    },
    {
      counterparty: 'rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K',
      currency: 'USD',
      value: '0'
    }
  ],
  rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K: [
    {
      value: '0',
      currency: 'USD',
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q'
    },
    {
      value: '99.976002',
      currency: 'XRP',
      counterparty: ''
    }
  ]
}

// Pay 0.01 USD from rKmB to rLDY where rLDY starts with 0.01 USD
var usdPaymentBalanceChanges = {
  rKmBGxocj9Abgy25J51Mk1iqFzW9aVF9Tc: [
    {
      value: '1.525330905250352',
      currency: 'USD',
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q'
    },
    {
      value: '239.555992',
      currency: 'XRP',
      counterparty: ''
    }
  ],
  rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q: [
    {
      counterparty: 'rKmBGxocj9Abgy25J51Mk1iqFzW9aVF9Tc',
      currency: 'USD',
      value: '-1.525330905250352'
    },
    {
      counterparty: 'rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K',
      currency: 'USD',
      value: '-0.02'
    }
  ],
  rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K: [
    {
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q',
      currency: 'USD',
      value: '0.02'
    }
  ]
}

// Set trust limit to 200 USD on rLDY when it has a trust limit of 100 USD
// and has a balance of 0.02 USD
var setTrustlineBalanceChanges = {
  rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K: [
    {
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q',
      currency: 'USD',
      value: '0.02'
    },
    {
      value: '99.940002',
      currency: 'XRP',
      counterparty: ''
    }
  ],
  rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q: [
    {
      counterparty: 'rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K',
      currency: 'USD',
      value: '-0.02'
    }
  ]
}

var setTrustlineBalanceChanges3 = {
  rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K: [
    {
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q',
      currency: 'USD',
      value: '0.02'
    },
    {
      counterparty: '',
      currency: 'XRP',
      value: '99.884302'
    }
  ],
  rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q: [
    {
      counterparty: 'rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K',
      currency: 'USD',
      value: '-0.02'
    }
  ]
}

var setTrustlineBalanceChanges2 = {
  rsApBGKJmMfExxZBrGnzxEXyq7TMhMRg4e: [
    {
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q',
      currency: 'USD',
      value: '0'
    },
    {
      counterparty: '',
      currency: 'XRP',
      value: '9248.902096'
    }
  ],
  rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q: [
    {
      counterparty: 'rsApBGKJmMfExxZBrGnzxEXyq7TMhMRg4e',
      currency: 'USD',
      value: '0'
    },
    {
      counterparty: '',
      currency: 'XRP',
      value: '149.99998'
    }
  ]
}

// Set trust limit to 100 USD with balance of 10 USD on rLDY
// when it has no trustline
var createTrustlineBalanceChanges = {
  rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K: [
    {
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q',
      currency: 'USD',
      value: '10'
    },
    {
      counterparty: '',
      currency: 'XRP',
      value: '99.740302'
    }
  ],
  rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q: [
    {
      counterparty: 'rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K',
      currency: 'USD',
      value: '-10'
    }
  ]
}

// Pay 0.02 USD from rLDY to rKmB when rLDY has a trust limit of 0
// for USD, but still has a balance of 0.02 USD; which closes the trustline
var deleteTrustlineBalanceChanges = {
  rKmBGxocj9Abgy25J51Mk1iqFzW9aVF9Tc: [
    {
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q',
      currency: 'USD',
      value: '1.545330905250352'
    }
  ],
  rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q: [
    {
      counterparty: 'rKmBGxocj9Abgy25J51Mk1iqFzW9aVF9Tc',
      currency: 'USD',
      value: '-1.545330905250352'
    },
    {
      counterparty: 'rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K',
      currency: 'USD',
      value: '0'
    }
  ],
  rLDYrujdKUfVx28T9vRDAbyJ7G2WVXKo4K: [
    {
      counterparty: 'rMwjYedjc7qqtKYVLiAccJSmCwih4LnE2q',
      currency: 'USD',
      value: '0'
    },
    {
      counterparty: '',
      currency: 'XRP',
      value: '99.752302'
    }
  ]
}

var redeemBalanceChanges = {
  rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh: [
    {
      counterparty: 'rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK',
      currency: 'USD',
      value: '-100'
    }
  ],
  rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK: [
    {
      counterparty: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
      currency: 'USD',
      value: '100'
    },
    {
      counterparty: '',
      currency: 'XRP',
      value: '999.99998'
    }
  ]
}

var redeemThenIssueBalanceChanges = {
  rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh: [
    {
      counterparty: 'rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK',
      currency: 'USD',
      value: '100'
    }
  ],
  rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK: [
    {
      counterparty: 'rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh',
      currency: 'USD',
      value: '-100'
    },
    {
      counterparty: '',
      currency: 'XRP',
      value: '999.99997'
    }
  ]
}

var multipathBalanceChanges = {
  rrnsYgWn13Z28GtRgznrSUsLfMkvsXCZSu: [
    {
      counterparty: 'r4nmQNH4Fhjfh6cHDbvVSsBv7KySbj4cBf',
      currency: 'USD',
      value: '0'
    },
    {
      counterparty: 'rnYDWQaRdMb5neCGgvFfhw3MBoxmv5LtfH',
      currency: 'USD',
      value: '-100'
    }
  ],
  r4nmQNH4Fhjfh6cHDbvVSsBv7KySbj4cBf: [
    {
      counterparty: 'rrnsYgWn13Z28GtRgznrSUsLfMkvsXCZSu',
      currency: 'USD',
      value: '0'
    },
    {
      counterparty: '',
      currency: 'XRP',
      value: '999.99999'
    },
    {
      counterparty: 'rJsaPnGdeo7BhMnHjuc3n44Mf7Ra1qkSVJ',
      currency: 'USD',
      value: '0'
    },
    {
      counterparty: 'rGpeQzUWFu4fMhJHZ1Via5aqFC3A5twZUD',
      currency: 'USD',
      value: '0'
    }
  ],
  rJsaPnGdeo7BhMnHjuc3n44Mf7Ra1qkSVJ: [
    {
      counterparty: 'r4nmQNH4Fhjfh6cHDbvVSsBv7KySbj4cBf',
      currency: 'USD',
      value: '0'
    },
    {
      counterparty: 'rnYDWQaRdMb5neCGgvFfhw3MBoxmv5LtfH',
      currency: 'USD',
      value: '-100'
    }
  ],
  rGpeQzUWFu4fMhJHZ1Via5aqFC3A5twZUD: [
    {
      counterparty: 'r4nmQNH4Fhjfh6cHDbvVSsBv7KySbj4cBf',
      currency: 'USD',
      value: '0'
    },
    {
      counterparty: 'rnYDWQaRdMb5neCGgvFfhw3MBoxmv5LtfH',
      currency: 'USD',
      value: '-100'
    }
  ],
  rnYDWQaRdMb5neCGgvFfhw3MBoxmv5LtfH: [
    {
      counterparty: 'rJsaPnGdeo7BhMnHjuc3n44Mf7Ra1qkSVJ',
      currency: 'USD',
      value: '100'
    },
    {
      counterparty: 'rrnsYgWn13Z28GtRgznrSUsLfMkvsXCZSu',
      currency: 'USD',
      value: '100'
    },
    {
      counterparty: 'rGpeQzUWFu4fMhJHZ1Via5aqFC3A5twZUD',
      currency: 'USD',
      value: '100'
    }
  ]
}


function loadFixture(filename) {
  var path = __dirname + '/fixtures/' + filename
  return JSON.parse(fs.readFileSync(path))
}

describe('parseFinalBalances', function() {
  it('XRP create account', function() {
    var paymentResponse = loadFixture('payment-xrp-create-account.json')
    var result = parseFinalBalances(paymentResponse.metadata)
    assert.deepEqual(result, createAccountBalanceChanges)
  })
  it('USD payment to account with no USD', function() {
    var filename = 'payment-iou-destination-no-balance.json'
    var paymentResponse = loadFixture(filename)
    var result = parseFinalBalances(paymentResponse.metadata)
    assert.deepEqual(result, usdFirstPaymentBalanceChanges)
  })
  it('USD payment of all USD in source account', function() {
    var paymentResponse = loadFixture('payment-iou-spend-full-balance.json')
    var result = parseFinalBalances(paymentResponse.metadata)
    assert.deepEqual(result, usdFullPaymentBalanceChanges)
  })
  it('USD payment to account with USD', function() {
    var paymentResponse = loadFixture('payment-iou.json')
    var result = parseFinalBalances(paymentResponse.metadata)
    assert.deepEqual(result, usdPaymentBalanceChanges)
  })
  it('Set trust limit to 0 with balance remaining', function() {
    var paymentResponse = loadFixture('trustline-set-limit-to-zero.json')
    var result = parseFinalBalances(paymentResponse.metadata)
    assert.deepEqual(result, setTrustlineBalanceChanges)
  })
  it('Create trustline', function() {
    var paymentResponse = loadFixture('trustline-create.json')
    var result = parseFinalBalances(paymentResponse.metadata)
    assert.deepEqual(result, createTrustlineBalanceChanges)
  })
  it('Set trustline', function() {
    var paymentResponse = loadFixture('trustline-set-limit.json')
    var result = parseFinalBalances(paymentResponse.metadata)
    assert.deepEqual(result, setTrustlineBalanceChanges3)
  })
  it('Set trustline 2', function() {
    var paymentResponse = loadFixture('trustline-set-limit-2.json')
    var result = parseFinalBalances(paymentResponse.metadata)
    assert.deepEqual(result, setTrustlineBalanceChanges2)
  })
  it('Delete trustline', function() {
    var paymentResponse = loadFixture('trustline-delete.json')
    var result = parseFinalBalances(paymentResponse.metadata)
    assert.deepEqual(result, deleteTrustlineBalanceChanges)
  })
  it('Redeem USD', function() {
    var paymentResponse = loadFixture('payment-iou-redeem.json')
    var result = parseFinalBalances(paymentResponse.result.meta)
    assert.deepEqual(result, redeemBalanceChanges)
  })
  it('Redeem then issue USD', function() {
    var paymentResponse = loadFixture('payment-iou-redeem-then-issue.json')
    var result = parseFinalBalances(paymentResponse.result.meta)
    assert.deepEqual(result, redeemThenIssueBalanceChanges)
  })
  it('Multipath USD payment', function() {
    var paymentResponse = loadFixture('payment-iou-multipath.json')
    var result = parseFinalBalances(paymentResponse.result.meta)
    assert.deepEqual(result, multipathBalanceChanges)
  })
})
