# Affiliate hook 

A HOOK that sets up an affiliate campain for your account.

SERVICE - a service account, where customers pay for a service.  
AFFILIATE - an account of a registered partner of the SERVICE, which will get a referral commision.  
CUSTOMER - a customer account.

1. SERVICE installs the HOOK
2. SERVICE registers AFFILIATES

Every payment (by CUSTOMER) that specifies a registered AFFILIATE will result in the AFFILIATE getting 15% of the payment and the CUSTOMER getting a 5% cashback.
(5% and 15% are hardcoded in the contract affiliate.c)

## Usage:

### Install the hook on your SERVICE account:
```
node upload.js <SERVICE secret> 
```

### Add an AFFILIATE account to your affiliate system:

```
node setup.js <SERVICE secret> <AFFILIATE address>
```

### Payment example:
```
node pay.js <CUSTOMER secret> <XRP amount> <SERVICE address> <AFFILIATE address>
```

## Credits
The hook example designed and developed by [@kromsten](https://github.com/kromsten) 👍 
