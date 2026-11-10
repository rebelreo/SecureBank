#ifndef BANK_H
#define BANK_H

#define MAX_NAME 50
#define MAX_PASS 32
#define MAX_MOBILE 16
#define MAX_TYPE 16
#define DATA_FILE "accounts.dat"

typedef struct {
    int accountNo;
    char name[MAX_NAME];
    char mobile[MAX_MOBILE];
    char password[MAX_PASS];
    char accountType[MAX_TYPE];
    double balance;
    int active;
} Account;
typedef struct {
    unsigned long long transaction_id;
    int account_no;
    char type[12];
    double amount;
    long long timestamp;
    double balance_after;
} Transaction;

int create_account(
    const char *name,
    const char *mobile,
    const char *password,
    const char *type,
    double opening_balance,
    int *new_account_no
);
int authenticate_account(
    int account_no,
    const char *password,
    Account *result
);
int get_account_details(int account_no, Account *result);
int get_all_accounts(Account accounts[], int max_records);
int deposit_money(
    int account_no,
    double amount,
    double *new_balance
);

int withdraw_money(
    int account_no,
    double amount,
    double *new_balance
);
int transfer_money(
    int sender_account,
    int receiver_account,
    double amount,
    double *sender_balance,
    double *receiver_balance
);
int get_transactions(int account_no, Transaction transactions[], int max_records);
int update_customer_name(int account_no, const char *new_name);

int update_customer_mobile(int account_no, const char *new_mobile);
int update_customer_profile(
    int account_no,
    const char *new_name,
    const char *new_mobile
);
int change_customer_password(
    int account_no,
    const char *old_password,
    const char *new_password
);
#endif
