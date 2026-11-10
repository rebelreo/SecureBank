#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/file.h>
#include "bank.h"
#define TRANSACTION_FILE "transactions.dat"
int create_account(
    const char *name,
    const char *mobile,
    const char *password,
    const char *type,
    double opening_balance,
    int *new_account_no
) {
    Account account, old_account;
    FILE *fp;
    int next_account = 100001;
    int i;

    if (!name || !mobile || !password || !type ||
        !new_account_no)
        return 0;

    if (strlen(name) == 0 || strlen(name) >= MAX_NAME)
        return 0;

    if (strlen(mobile) != 10)
        return 0;

    for (i = 0; mobile[i]; i++) {
        if (!isdigit((unsigned char)mobile[i]))
            return 0;
    }

    if (strlen(password) < 4 ||
        strlen(password) >= MAX_PASS)
        return 0;

    if (strcmp(type, "Savings") != 0 &&
        strcmp(type, "Current") != 0)
        return 0;

    if (!isfinite(opening_balance) ||
        opening_balance < 0 || opening_balance >= 1e12)
        return 0;

    fp = fopen(DATA_FILE, "rb");

    if (fp != NULL) {
        while (fread(&old_account, sizeof(Account), 1, fp) == 1) {
            if (old_account.accountNo >= next_account &&
                old_account.accountNo < 2000000000)
                next_account = old_account.accountNo + 1;
        }
        fclose(fp);
    }

    memset(&account, 0, sizeof(Account));

    account.accountNo = next_account;

    snprintf(account.name, sizeof(account.name), "%s", name);
    snprintf(account.mobile, sizeof(account.mobile), "%s", mobile);
    snprintf(account.password, sizeof(account.password), "%s", password);
    snprintf(account.accountType, sizeof(account.accountType), "%s", type);

    account.balance = opening_balance;
    account.active = 1;

    fp = fopen(DATA_FILE, "ab");

    if (fp == NULL)
        return 0;

    if (fwrite(&account, sizeof(Account), 1, fp) != 1) {
        fclose(fp);
        return 0;
    }

    if (fclose(fp) != 0)
        return 0;

    *new_account_no = account.accountNo;

    return 1;
}

int authenticate_account(
    int account_no,
    const char *password,
    Account *result
) {
    FILE *fp;
    Account account;

    if (account_no <= 0 || password == NULL || result == NULL)
        return 0;

    fp = fopen(DATA_FILE, "rb");

    if (fp == NULL)
        return 0;

    while (fread(&account, sizeof(Account), 1, fp) == 1) {

        if (account.active == 1 &&
            account.accountNo == account_no &&
            strcmp(account.password, password) == 0) {

            *result = account;
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

int get_account_details(int account_no, Account *result)
{
    FILE *fp;
    Account account;

    if (account_no <= 0 || result == NULL)
        return 0;

    fp = fopen(DATA_FILE, "rb");

    if (fp == NULL)
        return 0;

    while (fread(&account, sizeof(Account), 1, fp) == 1)
    {
        if (account.active == 1 &&
            account.accountNo == account_no)
        {
            *result = account;
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}
int get_all_accounts(Account accounts[], int max_records)
{
    FILE *fp;
    Account account;
    int count = 0;

    if (accounts == NULL || max_records <= 0)
        return -1;

    fp = fopen(DATA_FILE, "rb");

    if (fp == NULL)
        return 0;

    while (count < max_records &&
           fread(&account, sizeof(Account), 1, fp) == 1) {

        if (account.active == 1) {
            accounts[count] = account;
            count++;
        }
    }

    fclose(fp);
    return count;
}
static int record_transaction(int account_no, const char *type,
                              double amount, double balance_after)
{
    FILE *fp;
    Transaction transaction;
    Transaction existing;
    unsigned long long max_id = 0;

    fp = fopen(TRANSACTION_FILE, "a+b");

    if (fp == NULL)
        return 0;

    rewind(fp);

    while (fread(&existing, sizeof(Transaction), 1, fp) == 1)
    {
        if (existing.transaction_id > max_id)
            max_id = existing.transaction_id;
    }

    if (ferror(fp) || max_id == ULLONG_MAX)
    {
        fclose(fp);
        return 0;
    }

    memset(&transaction, 0, sizeof(Transaction));

    transaction.transaction_id = max_id + 1;
    transaction.account_no = account_no;
    snprintf(transaction.type, sizeof(transaction.type), "%s", type);
    transaction.amount = amount;
    transaction.timestamp = (long long)time(NULL);
    transaction.balance_after = balance_after;

    if (fseek(fp, 0, SEEK_END) != 0 ||
        fwrite(&transaction, sizeof(Transaction), 1, fp) != 1 ||
        fflush(fp) != 0)
    {
        fclose(fp);
        return 0;
    }

    fclose(fp);
    return 1;
}
int deposit_money(int account_no, double amount,
                  double *new_balance)
{
    FILE *fp;
Account account, old_account;
long record_offset;

    if (account_no <= 0 || new_balance == NULL ||
        !isfinite(amount) || amount <= 0)
        return 0;

    fp = fopen(DATA_FILE, "r+b");

    if (fp == NULL)
        return 0;

    while (fread(&account, sizeof(Account), 1, fp) == 1)
    {
        if (account.active == 1 &&
            account.accountNo == account_no)
        {
record_offset = ftell(fp) - (long)sizeof(Account);
old_account = account;
            if (!isfinite(account.balance) ||
                account.balance + amount >= 1e12)
            {
                fclose(fp);
                return 0;
            }

            account.balance += amount;

            if (fseek(fp, -(long)sizeof(Account), SEEK_CUR) != 0)
            {
                fclose(fp);
                return 0;
            }

            if (fwrite(&account, sizeof(Account), 1, fp) != 1)
            {
                fclose(fp);
                return 0;
            }

            if (fclose(fp) != 0)
    return 0;

if (!record_transaction(account_no, "DEPOSIT",
                        amount, account.balance))
{
    fp = fopen(DATA_FILE, "r+b");

    if (fp != NULL)
    {
        if (fseek(fp, record_offset, SEEK_SET) == 0)
            fwrite(&old_account, sizeof(Account), 1, fp);

        fclose(fp);
    }

    return 0;
}

*new_balance = account.balance;
return 1;
        }
    }

    fclose(fp);
    return 0;
}

int withdraw_money(int account_no, double amount,
                   double *new_balance)
{
    FILE *fp;
Account account, old_account;
long record_offset;

    if (account_no <= 0 || new_balance == NULL ||
        !isfinite(amount) || amount <= 0)
        return 0;

    fp = fopen(DATA_FILE, "r+b");

    if (fp == NULL)
        return 0;

    while (fread(&account, sizeof(Account), 1, fp) == 1)
    {
        if (account.active == 1 &&
            account.accountNo == account_no)
        {
         record_offset = ftell(fp) - (long)sizeof(Account);
old_account = account;
            if (!isfinite(account.balance) ||
                amount > account.balance)
            {
                fclose(fp);
                return 0;
            }

            account.balance -= amount;

            if (fseek(fp, -(long)sizeof(Account), SEEK_CUR) != 0)
            {
                fclose(fp);
                return 0;
            }

            if (fwrite(&account, sizeof(Account), 1, fp) != 1)
            {
                fclose(fp);
                return 0;
            }

            if (fclose(fp) != 0)
    return 0;

if (!record_transaction(account_no, "WITHDRAW",
                        amount, account.balance))
{
    fp = fopen(DATA_FILE, "r+b");

    if (fp != NULL)
    {
        if (fseek(fp, record_offset, SEEK_SET) == 0)
            fwrite(&old_account, sizeof(Account), 1, fp);

        fclose(fp);
    }

    return 0;
}

*new_balance = account.balance;
return 1;
        }
    }

    fclose(fp);
    return 0;
}
// TRANSFER MONEY BETWEEN ACCOUNTS
int transfer_money(
    int sender_account,
    int receiver_account,
    double amount,
    double *sender_balance,
    double *receiver_balance
)
{
    FILE *afp = NULL;
    FILE *tfp = NULL;

    Account sender, receiver;
    Account old_sender, old_receiver;
    Transaction out_tx, in_tx, existing;

    long sender_offset = -1;
    long receiver_offset = -1;
    long transaction_offset;

    unsigned long long max_id = 0;

    int found_sender = 0;
    int found_receiver = 0;
    int success = 0;

    if (sender_account <= 0 ||
        receiver_account <= 0 ||
        sender_account == receiver_account ||
        !isfinite(amount) || amount <= 0 ||
        amount >= 1e12 ||
        sender_balance == NULL ||
        receiver_balance == NULL)
        return 0;

    afp = fopen(DATA_FILE, "r+b");

    if (afp == NULL)
        goto cleanup;

    tfp = fopen(TRANSACTION_FILE, "a+b");

    if (tfp == NULL)
        goto cleanup;

    if (flock(fileno(afp), LOCK_EX) != 0)
        goto cleanup;

    if (flock(fileno(tfp), LOCK_EX) != 0)
        goto cleanup;

    rewind(afp);

    Account current;
    long position;

    while (1)
    {
        position = ftell(afp);

        if (fread(&current, sizeof(Account), 1, afp) != 1)
            break;

        if (current.active != 1)
            continue;

        if (current.accountNo == sender_account)
        {
            sender = current;
            sender_offset = position;
            found_sender = 1;
        }

        if (current.accountNo == receiver_account)
        {
            receiver = current;
            receiver_offset = position;
            found_receiver = 1;
        }
    }

    if (ferror(afp) || !found_sender || !found_receiver)
        goto cleanup;

    if (!isfinite(sender.balance) ||
        !isfinite(receiver.balance) ||
        sender.balance < amount ||
        receiver.balance + amount >= 1e12)
        goto cleanup;

    old_sender = sender;
    old_receiver = receiver;

    sender.balance -= amount;
    receiver.balance += amount;

    rewind(tfp);

    while (fread(&existing, sizeof(Transaction), 1, tfp) == 1)
    {
        if (existing.transaction_id > max_id)
            max_id = existing.transaction_id;
    }

    if (ferror(tfp) || max_id > ULLONG_MAX - 2)
        goto cleanup;

    memset(&out_tx, 0, sizeof(Transaction));
    memset(&in_tx, 0, sizeof(Transaction));

    out_tx.transaction_id = max_id + 1;
    out_tx.account_no = sender_account;
snprintf(out_tx.type, sizeof(out_tx.type), "XFER_OUT");
    out_tx.amount = amount;
    out_tx.timestamp = (long long)time(NULL);
    out_tx.balance_after = sender.balance;

    in_tx.transaction_id = max_id + 2;
    in_tx.account_no = receiver_account;
snprintf(in_tx.type, sizeof(in_tx.type), "XFER_IN");
    in_tx.amount = amount;
    in_tx.timestamp = out_tx.timestamp;
    in_tx.balance_after = receiver.balance;

    if (fseek(afp, sender_offset, SEEK_SET) != 0 ||
        fwrite(&sender, sizeof(Account), 1, afp) != 1)
    {
        goto rollback_accounts;
    }

    if (fseek(afp, receiver_offset, SEEK_SET) != 0 ||
        fwrite(&receiver, sizeof(Account), 1, afp) != 1)
        goto rollback_accounts;

    if (fflush(afp) != 0)
        goto rollback_accounts;

    if (fseek(tfp, 0, SEEK_END) != 0)
        goto rollback_accounts;

    transaction_offset = ftell(tfp);

    if (transaction_offset < 0)
        goto rollback_accounts;

    if (fwrite(&out_tx, sizeof(Transaction), 1, tfp) != 1 ||
        fwrite(&in_tx, sizeof(Transaction), 1, tfp) != 1 ||
        fflush(tfp) != 0)
    {
        clearerr(tfp);
        fflush(tfp);
        ftruncate(fileno(tfp), transaction_offset);
        goto rollback_accounts;
    }

    *sender_balance = sender.balance;
    *receiver_balance = receiver.balance;

    success = 1;
    goto cleanup;

rollback_accounts:

    clearerr(afp);

    if (fseek(afp, sender_offset, SEEK_SET) == 0)
        fwrite(&old_sender, sizeof(Account), 1, afp);

    if (fseek(afp, receiver_offset, SEEK_SET) == 0)
        fwrite(&old_receiver, sizeof(Account), 1, afp);

    fflush(afp);

cleanup:

    if (tfp != NULL)
    {
        flock(fileno(tfp), LOCK_UN);
        fclose(tfp);
    }

    if (afp != NULL)
    {
        flock(fileno(afp), LOCK_UN);
        fclose(afp);
    }

    return success;
}
int get_transactions(int account_no, Transaction transactions[], int max_records)
{
    FILE *fp;
    Transaction transaction;
    int count = 0;

    if (account_no <= 0 || transactions == NULL || max_records <= 0)
        return 0;

    fp = fopen(TRANSACTION_FILE, "rb");

    if (fp == NULL)
        return 0;

    while (fread(&transaction, sizeof(Transaction), 1, fp) == 1)
    {
        if (transaction.account_no == account_no)
        {
            if (count < max_records)
                transactions[count++] = transaction;
        }
    }

    fclose(fp);
    return count;
}
// UPDATE CUSTOMER NAME
int update_customer_name(int account_no, const char *new_name)
{
    FILE *fp;
    Account account;

    if (account_no <= 0 || new_name == NULL ||
        strlen(new_name) == 0 || strlen(new_name) >= MAX_NAME)
        return 0;

    fp = fopen(DATA_FILE, "r+b");

    if (fp == NULL)
        return 0;

    while (fread(&account, sizeof(Account), 1, fp) == 1)
    {
        if (account.active == 1 && account.accountNo == account_no)
        {
            snprintf(account.name, sizeof(account.name), "%s", new_name);

            if (fseek(fp, -(long)sizeof(Account), SEEK_CUR) != 0)
            {
                fclose(fp);
                return 0;
            }

            int success = fwrite(&account, sizeof(Account), 1, fp) == 1;
            fclose(fp);
            return success;
        }
    }

    fclose(fp);
    return 0;
}


// UPDATE CUSTOMER MOBILE
int update_customer_mobile(int account_no, const char *new_mobile)
{
    FILE *fp;
    Account account;
    int i;

    if (account_no <= 0 || new_mobile == NULL ||
        strlen(new_mobile) != 10)
        return 0;

    for (i = 0; new_mobile[i]; i++)
    {
        if (!isdigit((unsigned char)new_mobile[i]))
            return 0;
    }

    fp = fopen(DATA_FILE, "r+b");

    if (fp == NULL)
        return 0;

    while (fread(&account, sizeof(Account), 1, fp) == 1)
    {
        if (account.active == 1 && account.accountNo == account_no)
        {
            snprintf(account.mobile, sizeof(account.mobile), "%s", new_mobile);

            if (fseek(fp, -(long)sizeof(Account), SEEK_CUR) != 0)
            {
                fclose(fp);
                return 0;
            }

            int success = fwrite(&account, sizeof(Account), 1, fp) == 1;
            fclose(fp);
            return success;
        }
    }

    fclose(fp);
    return 0;
}


// CHANGE CUSTOMER PASSWORD
int change_customer_password(
    int account_no,
    const char *old_password,
    const char *new_password
)
{
    FILE *fp;
    Account account;

    if (account_no <= 0 ||
        old_password == NULL ||
        new_password == NULL ||
        strlen(new_password) < 4 ||
        strlen(new_password) >= MAX_PASS)
        return 0;

    fp = fopen(DATA_FILE, "r+b");

    if (fp == NULL)
        return 0;

    while (fread(&account, sizeof(Account), 1, fp) == 1)
    {
        if (account.active == 1 &&
            account.accountNo == account_no &&
            strcmp(account.password, old_password) == 0)
        {
            snprintf(account.password, sizeof(account.password),
                     "%s", new_password);

            if (fseek(fp, -(long)sizeof(Account), SEEK_CUR) != 0)
            {
                fclose(fp);
                return 0;
            }

            int success = fwrite(&account, sizeof(Account), 1, fp) == 1;
            fclose(fp);
            return success;
        }
    }

    fclose(fp);
    return 0;
}
// UPDATE CUSTOMER PROFILE
int update_customer_profile(
    int account_no,
    const char *new_name,
    const char *new_mobile
)
{
    FILE *fp;
    Account account;

    if (account_no <= 0 ||
        new_name == NULL ||
        new_mobile == NULL ||
        strlen(new_name) == 0 ||
        strlen(new_name) >= MAX_NAME ||
        strlen(new_mobile) != 10)
        return 0;

    for (int i = 0; i < 10; i++) {
        if (!isdigit((unsigned char)new_mobile[i]))
            return 0;
    }

    fp = fopen(DATA_FILE, "r+b");

    if (fp == NULL)
        return 0;

    while (fread(&account, sizeof(Account), 1, fp) == 1) {
        if (account.active == 1 &&
            account.accountNo == account_no) {

            snprintf(account.name, sizeof(account.name),
                     "%s", new_name);

            snprintf(account.mobile, sizeof(account.mobile),
                     "%s", new_mobile);

            if (fseek(fp, -(long)sizeof(Account), SEEK_CUR) != 0) {
                fclose(fp);
                return 0;
            }

            int success = fwrite(&account, sizeof(Account), 1, fp) == 1;
            fclose(fp);
            return success;
        }
    }

    fclose(fp);
    return 0;
}
