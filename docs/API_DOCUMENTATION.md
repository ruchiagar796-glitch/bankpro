# HTTP API

Base URL: `http://localhost:8080` (server listens on `127.0.0.1` only). Same origin as the web page, so **no CORS** setup is needed. Not secure enough for public deployment: no authentication, no HTTPS.

## Conventions
- Requests with a body use `Content-Type: application/json`. Maximum body size 64 KB.
- **Amounts are rupees** sent as a string (`"250.25"`) or a JSON number (`250.25`); up to 2 decimals, no sign, no exponent. The server converts to integer paise. Responses give `balance` as a rupee string and `balancePaise` as an integer.
- Account numbers may be sent as a number or a digit string.
- Success: `{"success":true,"message":"...","data":...}`
- Error: `{"success":false,"error":{"code":"...","message":"..."}}`

| HTTP | `error.code` | Meaning |
|---|---|---|
| 400 | `INVALID_INPUT` | missing/invalid field, bad amount, same-account transfer, bad name, ... |
| 400 | `MALFORMED_JSON` | body is not valid JSON |
| 404 | `NOT_FOUND` | account/transaction/token missing, nothing to reverse, unknown route or wrong method |
| 409 | `CONFLICT` | closing with a non-zero balance, reversing an already-reversed or not-newest transaction |
| 413 | `BAD_REQUEST` | body larger than 64 KB |
| 422 | `INSUFFICIENT_FUNDS`, `LIMIT_EXCEEDED` | not enough balance, above per-transaction or balance limits |
| 500 | `STORAGE_FAILURE`, `DATA_CORRUPT`, `INTERNAL_ERROR` | file could not be written (memory is rolled back), unexpected error |

Account object: `{"accountNumber":1001,"holderName":"Asha Verma","accountType":"Savings","balance":"1000.00","balancePaise":100000,"createdAt":"2026-10-10 14:06:44"}`

Transaction object: `{"id":3,"accountNumber":1001,"type":"deposit","amount":"250.25","amountPaise":25025,"balanceAfter":"1250.25","timestamp":"2026-10-10 14:06:44","description":"Cash deposit","relatedId":0}`; `type` is one of `deposit`, `withdrawal`, `transfer_out`, `transfer_in`, `reversal`. For a reversal `relatedId` is the reversed transaction; for transfer legs it is the other leg.

## Endpoints

### GET /api/dashboard
`200`. `data`: `totalAccounts`, `totalBalance`, `totalBalancePaise`, `totalTransactions`, `customersWaiting`, `nextTokenNumber`, `recentTransactions` (up to 5, newest first), `notice`.

### GET /api/account-types
`200`. `data`: array of `{type, minimumOpeningDeposit, maxWithdrawalPerTransaction}`.

### GET /api/accounts[?search=text]
`200`, array of accounts in creation order. `search` all digits = account number lookup; otherwise case-insensitive name substring. Empty result is `[]`.

### GET /api/accounts/sorted?by=name|balance&order=asc|desc
`200`, merge-sorted array. Defaults: `by=name`, `order=asc`. `400` for other values.

### GET /api/accounts/{accountNumber}
`200` account. `400` not a positive number. `404` not found.

### POST /api/accounts
Body: `{"holderName":"Asha Verma","accountType":"Savings","initialDeposit":"1000"}`.
`201` account. `400` empty/invalid name, unknown type, invalid amount, below the minimum opening deposit. `422` above limits.

### DELETE /api/accounts/{accountNumber}
`200` `{accountNumber}`. `404` not found. `409` balance not zero.

### POST /api/transactions/deposit
Body: `{"accountNumber":1001,"amount":"250.25"}`. `201` `data`: `{transaction, account}`. Errors: 400, 404, 422.

### POST /api/transactions/withdraw
Same body. `201` `{transaction, account}`. `422` `INSUFFICIENT_FUNDS` or `LIMIT_EXCEEDED` (Savings Rs 50,000 / Current Rs 5,00,000 per withdrawal).

### POST /api/transactions/transfer
Body: `{"fromAccount":1002,"toAccount":1001,"amount":"1000"}`. `201` `data`: `{debit, credit, fromAccount, toAccount}`. `400` same account, `404` either account missing, `422` insufficient/limit. Both sides are validated before either is changed.

### GET /api/transactions[?limit=n]
`200` newest first; default 50, `limit` 1..1000.

### GET /api/transactions/account/{accountNumber}
`200` newest first. `404` unknown account.

### GET /api/transactions/reversible
`200` `data`: `{transaction: <the newest reversible transaction or null>, stackSize}`.

### POST /api/transactions/reverse
Body: `{}` (reverse the newest reversible transaction) or `{"transactionId":7}` (must be that newest one). `201` `data`: `{reversal, account}`.
Errors: `404` nothing to reverse / unknown id; `409` already reversed, or not the newest (LIFO); `400` transfers and reversals cannot be reversed; `422` reversal would make the balance negative or exceed the cap (nothing changes).

### GET /api/queue
`200` `data`: `{waiting:[{token,customerName,purpose,issuedAt}], count, nextTokenNumber}` (front first).

### POST /api/queue
Body: `{"customerName":"Walk In","purpose":"Passbook update"}` (`purpose` optional, max 60 chars). `201` the token. `400` empty name.

### POST /api/queue/serve
Body ignored. `200` the served token. `404` queue empty.

## Other behaviour
- Unknown route or wrong method under `/api/` -> `404` JSON `NOT_FOUND`.
- `GET /healthz` returns `ok` (no login) for hosting health checks.
- If `BMS_PASSWORD` is set, every other URL (pages and API) needs HTTP Basic login; otherwise `401`.
- Everything else is served from the `frontend/` folder (`/` -> `index.html`).
- Responses under `/api/` carry `Cache-Control: no-store`.
