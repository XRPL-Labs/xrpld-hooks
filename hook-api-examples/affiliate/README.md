A Hook that sets up an affiliate campain for your account. Every payment that specifies an account you registered as your affiliate will result in the account getting 15% of the payment and the sender getting a 5% cashback.

#### Usage:

##### Install the hook on your account:
```
node ulpoad.js <secret> 
```

##### Add an account to your affiliate system:

```
node setup.js <secret> <account address>
```

##### Payment example:
```
node pay.js <secret> <xrp amount> <destination (hook)> <referral address>
```
