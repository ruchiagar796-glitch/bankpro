# Project structure

```
BankManagementSystem/
|-- CMakeLists.txt            build configuration (downloads the 2 header-only libraries)
|-- README.md                 start here
|-- PROJECT_STRUCTURE.md      this file
|-- .gitignore
|
|-- include/                  headers (interfaces)
|   |-- BankError.h           exception type + error codes -> HTTP status mapping
|   |-- Money.h               integer-paise helpers (parse/format), limits
|   |-- BankAccount.h         abstract BankAccount + SavingsAccount + CurrentAccount
|   |-- Transaction.h         transaction record
|   |-- AccountLinkedList.h   hand-written singly linked list of accounts
|   |-- TransactionStack.h    hand-written stack (undo)
|   |-- CustomerServiceQueue.h hand-written queue (tokens)
|   |-- SortingAlgorithms.h   hand-written merge sort (template)
|   |-- TransactionHistory.h  ordered transaction records
|   |-- FileStorage.h         text-file persistence
|   |-- Bank.h                business logic, composes all of the above
|   |-- BankService.h         request validation, JSON results (no HTTP)
|   `-- ApiServer.h           HTTP routes + static files
|
|-- src/                      implementations (.cpp for every header with logic) + main.cpp
|
|-- tests/                    standalone test executable (own tiny framework)
|   |-- test_framework.h  test_main.cpp
|   |-- test_accounts.cpp  test_transactions.cpp  test_file_storage.cpp
|   |-- test_data_structures.cpp  test_service.cpp
|
|-- frontend/
|   |-- index.html
|   |-- css/style.css
|   `-- js/api.js  js/app.js
|
|-- data/.gitkeep             data files are created here at run time (git-ignored)
|-- third_party/              downloaded by CMake (git-ignored)
`-- docs/
    |-- OOP_CONCEPTS.md  DSA_COMPLEXITY.md  API_DOCUMENTATION.md
    |-- TESTING.md  VIVA_QUESTIONS.md
```
Differences from the suggested layout: `Money`, `BankError` and `TransactionHistory` were added; each data structure has a small `.cpp`; tests use `test_framework.h` plus `test_service.cpp`; `CustomerServiceQueue` is a separate class (not inside `Bank`).
