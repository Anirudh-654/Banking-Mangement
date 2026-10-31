# Banking Management System

This is a complete Linux C mini-project implementing the roles and operations in the supplied specification. It uses a TCP server with concurrent pthread-based client handling, POSIX file locks, a named semaphore, binary persistent records, write-ahead audit entries, and `fsync` for durable mutation paths.

## Build and run

On Linux with GCC and POSIX threads:

```bash
make
./bank_server data
./bank_client 127.0.0.1
```

Start the server before one or more clients. The server creates its database files in the directory passed as the first argument (`data` by default). The optional second argument for both executables is the TCP port, for example `./bank_server data 9091` and `./bank_client 127.0.0.1 9091`. Never run multiple server instances against the same data directory.

Initial accounts are created only for a new data directory:

| Role | Username | Password |
| --- | --- | --- |
| Administrator | `admin` | `admin123` |
| Manager | `manager` | `manager123` |
| Employee | `employee` | `employee123` |
| Customer | `customer` | `customer123` |

Change all default passwords before a demonstration. Passwords are SHA-256 digests with a per-user salt in the local database; the client never prints passwords.

## Functional coverage

- Customer: one-session login, balance, deposit, withdrawal, transfer, loan application, loan-status tracking, password change, feedback, transaction history, logout, and exit. An approved loan is credited to the customer's balance and passbook.
- Employee: one-session login, open, modify, and close customer accounts, view and decide assigned loans, customer passbook/history, password change, logout, and exit.
- Manager: one-session login, activate/deactivate customer accounts, view the employee directory and pending loan application IDs, atomically assign an unassigned pending loan to an employee, review feedback, password change, logout, and exit.
- Administrator: one-session login, create customers, employees, and managers; activate/deactivate customer, employee, and manager accounts; modify user details; change user roles; password change, logout, and exit.

## Concurrency and persistence

- A detached POSIX thread serves each client; several clients may connect at once.
- `fcntl(F_SETLKW)` advisory locks protect database reads/writes across processes, with fixed-record byte-range locking available for individual records. Server-side synchronization additionally serializes competing in-process loan assignments.
- A named POSIX semaphore coordinates balance-changing operations in the running server.
- Transfers prepare a durable rollback record, update both customers in one locked `users.dat` write path, call `fsync`, and mark the transfer committed. On startup, a prepared-but-uncommitted transfer is rolled back before clients are accepted; committed transfers produce passbook records for both parties.
- All append and update mutation paths call `fsync`. Deposits, withdrawals, transfers, and loan disbursements use durable prepare/commit recovery journals. Persistent data remains in `users.dat`, `loans.dat`, `transactions.dat`, `feedback.dat`, and journal files. Every request revalidates the session, so deactivating a customer invalidates their open connection before its next operation.

The protocol deliberately uses fixed-size C records, so the client and server must be compiled from the same source revision and run on compatible Linux architectures.
