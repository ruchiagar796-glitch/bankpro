# Data structures, algorithms and complexity

Notation: **A** = number of accounts, **T** = number of transactions, **S** = size of the undo stack, **Q** = people in the queue, **R** = reversed ids, **m** = length of a name. These figures describe the *actual code*.

## 1. Singly linked list - `AccountLinkedList`
- **Why:** required by the brief and the best structure for showing pointers, insertion and deletion by hand. Accounts are in creation order, so account numbers rise naturally.
- **How:** each `Node` holds `unique_ptr<BankAccount>` and a `next` pointer; the list keeps `head_`, `tail_`, `size_`.
- **Used by:** every account feature (create, find, search, delete, list, dashboard totals).

| Operation | Time | Space | Note |
|---|---|---|---|
| `insertBack` | **O(A)** | O(1) | it first calls `find` to reject duplicate numbers (O(A)); linking at the tail is O(1) |
| `find(accountNumber)` | O(A) | O(1) | linear scan |
| `findByName` | O(A * m) | O(k) for k matches | substring match, case-insensitive |
| `remove` | O(A) | O(1) | find + unlink; head/tail handled |
| `size()` / `empty()` | O(1) | O(1) | counter stored |
| traverse (`forEach`, `toVector`) | O(A) | O(1) / O(A) | |
| `clear` / destructor | O(A) | O(1) | iterative, no recursion |

**Why not a vector or hash table?** A `std::vector` has O(1) indexing and is faster in practice, and a hash table gives O(1) average lookup by account number. Both hide pointers and node management, so they would not demonstrate the linked list the project is meant to show. **Trade-off:** with a few hundred accounts the O(A) scans are not noticeable; with millions a hash table (plus an ordered index) would be the right choice. Mentioned in "Future improvements".

## 2. Stack - `TransactionStack` (undo)
- **Why:** undo is naturally *last in, first out*: you can only undo the newest action first.
- **How:** linked nodes, `top_` pointer; `push` makes a new top node, `pop` removes it.
- **Used by:** `Bank::deposit/withdraw/createAccount` push; `reverseLastTransaction` peeks, validates, then pops. Rebuilt from `transactions.txt` at start-up.

| Operation | Time | Space |
|---|---|---|
| `push`, `pop`, `top`, `empty`, `size` | O(1) | O(1) per push |
| `toVector` | O(S) | O(S) |
| `Bank::purgeStackFor(account)` (on close) | O(S) | O(S) | pops all, pushes back the ones to keep |
| `reverseLastTransaction` as a whole | O(A + T) | | O(A) to find the account, O(1) stack work, O(A+T) saving |

## 3. Queue - `CustomerServiceQueue`
- **Why:** customers are served in arrival order (first in, first out).
- **How:** linked nodes with `head_` (dequeue) and `tail_` (enqueue). Token numbers come from a persisted counter, so they only increase.

| Operation | Time | Space |
|---|---|---|
| `enqueue` / `issueToken`, `dequeue`, `front`, `empty`, `size` | O(1) | O(1) |
| `toVector` (display) | O(Q) | O(Q) |

## 4. Searching
- By account number: linear search in the list, **O(A)** time, O(1) space.
- By name: linear scan with case-insensitive substring match, **O(A * m)**.
- Transaction filters (`forAccount`, `findById`, `recent`): scan the history vector, O(T).

## 5. Sorting - merge sort (`SortingAlgorithms.h`)
- Hand-written, **stable**, recursive top-down. Sorts a *vector of pointers* copied from the list, so the list is never changed.
- **Time O(A log A)** in best, average and worst case. **Space O(A)** (pointer copy + merge buffer) plus O(log A) recursion.
- `std::sort` is not used. Tie-break by account number makes the order deterministic.
- Why merge sort: guaranteed O(n log n), stable, and suits linked data (it does not need random access to the *list*).

## 6. File loading and saving
| Operation | Time | Space |
|---|---|---|
| `FileStorage::load` (parse) | O(A + T + R + Q) | O(A + T + R + Q) |
| validation in `Bank::loadFromStorage` | O((A + T + Q) log N) using `std::set` for duplicate checks | |
| building the account list at load | **O(A^2)** worst case (each `insertBack` does a duplicate scan) | |
| rebuilding the undo stack | **O(T * A)** worst case (one `find` per reversible transaction) | O(S) |
| `FileStorage::save` | O(A + T + R + Q) | O(file size): whole files are built in memory and rewritten |

**Consequence:** every successful change costs O(A + T) because all five files are rewritten. That is simple and safe for a classroom project but would not scale.

## 7. Cost of whole operations
| Feature | Time |
|---|---|
| Deposit / withdraw | O(A) find + O(A + T) save |
| Transfer | O(A) two finds + O(A + T) save |
| Create account | O(A) duplicate check + O(A + T) save |
| Close account | O(A) + O(S) purge + save |
| Dashboard | O(A) totals + O(T) recent list |
| Sorted view | O(A log A) |

## 8. Trade-offs of the chosen design
- Simplicity and demonstrability over raw speed.
- Whole-file rewrite + rename gives a clear, explainable safety mechanism but is O(A+T) per change.
- The undo stack is rebuilt from the history, so it is persistent without a separate file; the price is the O(T*A) rebuild at start.
- A single mutex serialises requests: easy to reason about, no data races, no parallel speed-up.
