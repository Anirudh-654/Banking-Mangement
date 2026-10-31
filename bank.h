#ifndef BANK_H
#define BANK_H

#include <stdint.h>

#define BANK_PORT 9090
#define MAX_USERNAME 32
#define MAX_PASSWORD 64
#define MAX_NAME 64
#define MAX_TEXT 256
#define MAX_MESSAGE 256

typedef enum { ROLE_CUSTOMER = 1, ROLE_EMPLOYEE = 2, ROLE_MANAGER = 3, ROLE_ADMIN = 4 } Role;
typedef enum {
    OP_LOGIN = 1, OP_LOGOUT, OP_BALANCE, OP_DEPOSIT, OP_WITHDRAW, OP_TRANSFER,
    OP_APPLY_LOAN, OP_CHANGE_PASSWORD, OP_FEEDBACK, OP_HISTORY,
    OP_ADD_CUSTOMER, OP_MODIFY_USER, OP_LOAN_DECISION, OP_LIST_ASSIGNED_LOANS,
    OP_CUSTOMER_HISTORY, OP_ACCOUNT_STATE, OP_ASSIGN_LOAN, OP_LIST_FEEDBACK,
    OP_ADD_EMPLOYEE, OP_SET_ROLE, OP_CLOSE_ACCOUNT, OP_LIST_PENDING_LOANS,
    OP_LIST_MY_LOANS, OP_EXIT
} Operation;

typedef struct {
    int32_t operation;
    int32_t role;
    double amount;
    int32_t id;
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char target[MAX_USERNAME];
    char text[MAX_TEXT];
} Request;

typedef struct {
    int32_t ok;
    double balance;
    char message[MAX_MESSAGE];
    char data[4096];
} Response;

typedef struct {
    char username[MAX_USERNAME];
    char password_hash[65];
    char salt[17];
    char full_name[MAX_NAME];
    int32_t role;
    int32_t active;
    int32_t logged_in;
    double balance;
} User;

typedef struct {
    int32_t id;
    char applicant[MAX_USERNAME];
    double amount;
    int32_t status;       /* 0 pending, 1 approved, 2 rejected */
    char assigned_to[MAX_USERNAME];
    char note[MAX_TEXT];
} Loan;

typedef struct {
    int32_t id;
    char customer[MAX_USERNAME];
    char kind[24];
    double amount;
    double balance_after;
    char counterparty[MAX_USERNAME];
    int64_t epoch;
} Transaction;

typedef struct {
    int32_t id;
    char customer[MAX_USERNAME];
    char text[MAX_TEXT];
    int64_t epoch;
} Feedback;

typedef struct {
    int32_t state;       /* 1 prepared, 2 committed, 3 rolled back */
    int64_t epoch;
    char from[MAX_USERNAME];
    char to[MAX_USERNAME];
    double amount;
    double from_before;
    double to_before;
} TransferJournal;

typedef struct {
    int32_t state;       /* 1 prepared, 2 committed, 3 rolled back */
    int32_t loan_id;
    int64_t epoch;
    char applicant[MAX_USERNAME];
    double amount;
    double balance_before;
} LoanJournal;

#endif
