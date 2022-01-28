# Affiliate hook 

A HOOK that sets up an affiliate campain for your account.

SERVICE - a service account, where customers pay for a service.
AFFILIATE - an account of a registered partner of the SERVICE, which will get a referral commision. 
CUSTOMER - a customer account.

1. SERVICE installs the HOOK
2. SERVICE register AFFILIATES

Every payment (by CUSTOMER) that specifies a registered AFFILIATE will result in the AFFILIATE getting 15% of the payment and the CUSTOMER getting a 5% cashback.
(5% and 15% are hardcoded in the contract affiliate.c)

## Usage:

### Install the hook on your account:
```
node upload.js <secret> 
```

### Add an account to your affiliate system:

```
node setup.js <secret> <account address>
```

### Payment example:
```
node pay.js <secret> <xrp amount> <destination (hook)> <referral address>
```

## Credits
The hook example designed and developed by [@kromsten](https://github.com/kromsten) 👍 
