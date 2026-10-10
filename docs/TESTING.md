# Testing

## What exists
A standalone executable `bank_tests` (no external framework). `TEST(name) { CHECK(...); CHECK_EQ(...); CHECK_THROWS_CODE(expr, ErrorCode::X); }` in `tests/test_framework.h`.

| File | Covers |
|---|---|
| `test_accounts.cpp` | create, unique/never-reused numbers, duplicate prevention, validation, Savings vs Current rules, search found/not found, close with balance / zero balance, sorted view keeps original order, money parsing |
| `test_transactions.cpp` | deposit ok/invalid, balance cap, withdraw ok/insufficient, transfer ok/invalid (nothing changes on failure), reversal LIFO, duplicate reversal, no negative balance on reversal, history filter, queue through Bank |
| `test_file_storage.cpp` | save + reload, escaping of `\|` and `\`, ID persistence after restart, reversal state persists, stack rebuilt in order, empty and missing files, directory creation, **14 malformed-file cases** (file left untouched), failed save rolls memory back, CRLF files |
| `test_data_structures.cpp` | linked list (empty, insert, find, delete head/middle/tail, 2000 nodes), stack LIFO, queue FIFO, stable merge sort |
| `test_service.cpp` | JSON validation (missing/invalid/huge/negative amounts, bad account numbers, wrong types), error shape, success flow |

## How to run
```powershell
cmake -S . -B build -G Ninja
cmake --build build
.\build\bank_tests.exe            # all tests
.\build\bank_tests.exe reversal   # only tests whose name contains "reversal"
ctest --test-dir build --output-on-failure
```
Exit code 0 means all passed.

## What I actually ran (October 2026, in a Linux sandbox)
- Compiler: g++ 13.3 with `-Wall -Wextra`: **0 warnings, 0 errors**.
- `bank_tests`: **41/41 passed**. One test initially failed because of an arithmetic mistake in the *test* (it needed 9,999 deposits, not 999); I fixed the test, not the code.
- Same tests with **AddressSanitizer + UBSan**: 41/41 passed, no leak or undefined-behaviour reports.
- **Live server test with curl** (create, deposit, withdraw, transfer, reverse, errors, malformed JSON, unknown routes, 64 KB+ body -> 413, restart persistence, duplicate reversal after restart, corrupt file -> refusal to start, file left untouched). This found one real bug (the generic HTTP error handler replaced the service's specific error messages); it is fixed.
- **Headless Chromium UI test (Playwright), 34/34 checks:** every navigation link, form validation, backend error messages displayed, create/deposit/withdraw/transfer, search, reversal confirmation dialog, queue (including HTML escaping of `<b>`), sorted view, close-account refusal, mobile layout has no horizontal scroll. No JavaScript exceptions.

## What I did NOT run
- **Nothing was compiled or run on Windows** (MSYS2/MinGW, MSVC). Please run the tests locally with the commands above.
- No test of simultaneous clients (the service uses a mutex, but this was not stress-tested).
- No test of a real power cut / crash during the multi-file save.
- Firefox/Safari were not tried.

## Manual test scenarios and expected results
| # | Steps | Expected |
|---|---|---|
| 1 | Create `Asha Verma`, Savings, `499` | error: minimum Rs 500.00; no account created |
| 2 | Create Savings `1000` | account 1001, balance Rs 1,000.00 |
| 3 | Deposit `0`, `-5`, `abc`, `1.234` | each rejected with a message; balance unchanged |
| 4 | Withdraw `1000.01` from 1001 | "Insufficient balance. Available: Rs 1000.00" |
| 5 | Withdraw `50001` from a Savings account with enough money | "Savings accounts allow at most Rs 50000.00 per withdrawal" |
| 6 | Transfer 1001 -> 1001 | "Cannot transfer to the same account" |
| 7 | Transfer 1001 -> 9999 | "Receiver account 9999 not found"; 1001 unchanged |
| 8 | Transfer `300` 1001 -> 1002 | both balances change by Rs 300, two history rows |
| 9 | Deposit `200`, Reverse | balance back; history shows a Reversal row |
| 10 | Reverse the same transaction id again (API) | 409 "has already been reversed" |
| 11 | Restart server; repeat #10 | still refused |
| 12 | Close account with balance | refused; withdraw everything, close -> succeeds; number never reused |
| 13 | Edit `data/accounts.txt`, add a junk line, start server | server refuses to start, names file and line, file unchanged |
| 14 | Issue 3 tokens, serve | served in order 1, 2, 3; serving an empty queue -> "No customers are waiting" |
