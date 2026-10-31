#include "bank.h"
#include "sha256.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define USERS_FILE "users.dat"
#define LOANS_FILE "loans.dat"
#define TXNS_FILE "transactions.dat"
#define FEEDBACK_FILE "feedback.dat"
#define JOURNAL_FILE "journal.log"
#define TRANSFER_JOURNAL_FILE "transfer_journal.dat"
#define LOAN_JOURNAL_FILE "loan_journal.dat"

static char data_dir[256] = "data";
static sem_t *write_gate;

static int send_all(int fd, const void *buf, size_t n) {
    const char *p = buf; ssize_t w;
    while (n) { w = send(fd, p, n, MSG_NOSIGNAL); if (w <= 0) return -1; p += w; n -= (size_t)w; }
    return 0;
}
static int recv_all(int fd, void *buf, size_t n) {
    char *p = buf; ssize_t r;
    while (n) { r = recv(fd, p, n, 0); if (r <= 0) return -1; p += r; n -= (size_t)r; }
    return 0;
}
static void path_for(char out[512], const char *file) { snprintf(out, 512, "%s/%s", data_dir, file); }
static int lock_file(int fd, short type) {
    struct flock l = { .l_type = type, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0 };
    return fcntl(fd, F_SETLKW, &l);
}
static void unlock_file(int fd) { struct flock l = { .l_type = F_UNLCK, .l_whence = SEEK_SET }; (void)fcntl(fd, F_SETLK, &l); }
static int db_open(const char *file, int flags) { char p[512]; path_for(p, file); return open(p, flags, 0600); }
static int read_at(int fd, off_t at, void *record, size_t size) { return lseek(fd, at, SEEK_SET) == at && read(fd, record, size) == (ssize_t)size; }
static int write_at(int fd, off_t at, const void *record, size_t size) { return lseek(fd, at, SEEK_SET) == at && write(fd, record, size) == (ssize_t)size; }
static void blank(Response *r) { memset(r, 0, sizeof(*r)); }
static void respond(Response *r, int ok, const char *fmt, ...) {
    va_list ap; r->ok = ok; va_start(ap, fmt); vsnprintf(r->message, sizeof r->message, fmt, ap); va_end(ap);
}
static const char *role_name(int role) { switch (role) { case ROLE_CUSTOMER:return "customer";case ROLE_EMPLOYEE:return "employee";case ROLE_MANAGER:return "manager";case ROLE_ADMIN:return "administrator";default:return "unknown"; } }
static int valid_text(const char *s, size_t max) { return s && s[0] && strnlen(s,max) < max; }
static int valid_amount(double value) { return value > 0.0 && value < 1000000000.0; }

static void make_salt(char out[17], const char *username) {
    unsigned long seed = (unsigned long)time(NULL) ^ (unsigned long)getpid(); size_t i;
    for (i=0; username[i]; ++i) seed = seed * 33u + (unsigned char)username[i];
    for (i=0; i<16; ++i) { seed = seed * 1103515245u + 12345u; out[i] = "0123456789abcdef"[(seed >> 16) & 15u]; } out[16]=0;
}
static void new_user(User *u, const char *username, const char *password, const char *name, int role, double balance) {
    memset(u,0,sizeof *u); snprintf(u->username,sizeof u->username,"%s",username); snprintf(u->full_name,sizeof u->full_name,"%s",name);
    u->role=role;u->active=1;u->balance=balance;make_salt(u->salt,username);password_digest(password,u->salt,u->password_hash);
}
static int find_user_fd(int fd, const char *username, User *user, off_t *position) {
    off_t pos=0; User candidate;
    while (read_at(fd,pos,&candidate,sizeof candidate)) { if (!strncmp(candidate.username,username,MAX_USERNAME)) { if(user)*user=candidate;if(position)*position=pos;return 1;} pos += sizeof candidate; }
    return 0;
}
static int append_transaction(const char *customer, const char *kind, double amount, double balance, const char *other) {
    int fd=db_open(TXNS_FILE,O_RDWR|O_CREAT|O_APPEND); Transaction t; off_t end;
    if(fd<0) return -1;
    if(lock_file(fd,F_WRLCK)<0) { close(fd); return -1; }
    end=lseek(fd,0,SEEK_END); memset(&t,0,sizeof t);t.id=(int32_t)(end/(off_t)sizeof t+1);snprintf(t.customer,sizeof t.customer,"%s",customer);snprintf(t.kind,sizeof t.kind,"%s",kind);t.amount=amount;t.balance_after=balance;snprintf(t.counterparty,sizeof t.counterparty,"%s",other?other:"-");t.epoch=time(NULL);
    int good=write(fd,&t,sizeof t)==(ssize_t)sizeof t && fsync(fd)==0;unlock_file(fd);close(fd);return good?0:-1;
}
static int append_journal(const char *operation, const char *who, double amount, const char *detail) {
    int fd=db_open(JOURNAL_FILE,O_WRONLY|O_CREAT|O_APPEND); char entry[512]; int n;
    if(fd<0) return -1;
    if(lock_file(fd,F_WRLCK)<0) { close(fd); return -1; }
    n=snprintf(entry,sizeof entry,"%lld | %s | %s | %.2f | %s\n",(long long)time(NULL),operation,who,amount,detail?detail:"-");int good=write(fd,entry,(size_t)n)==n && fsync(fd)==0;unlock_file(fd);close(fd);return good?0:-1;
}
static int write_transfer_journal(TransferJournal *entry, int state) {
    int fd = db_open(TRANSFER_JOURNAL_FILE, O_RDWR|O_CREAT);
    int good = 0;
    if (fd < 0) return -1;
    if (lock_file(fd, F_WRLCK) == 0) {
        entry->state = state;
        good = write_at(fd, 0, entry, sizeof *entry) && ftruncate(fd, sizeof *entry) == 0 && fsync(fd) == 0;
        unlock_file(fd);
    }
    close(fd);
    return good ? 0 : -1;
}
static void recover_prepared_transfer(void) {
    int jfd = db_open(TRANSFER_JOURNAL_FILE, O_RDWR|O_CREAT);
    TransferJournal entry;
    int ufd; User from, to; off_t fp, tp;
    if (jfd < 0 || lock_file(jfd, F_WRLCK) < 0) { if (jfd >= 0) close(jfd); return; }
    if (read_at(jfd, 0, &entry, sizeof entry) && entry.state == 1) {
        ufd = db_open(USERS_FILE, O_RDWR);
        if (ufd >= 0 && lock_file(ufd, F_WRLCK) == 0) {
            if (find_user_fd(ufd, entry.from, &from, &fp) && find_user_fd(ufd, entry.to, &to, &tp)) {
                from.balance = entry.from_before;
                to.balance = entry.to_before;
                if (write_at(ufd, fp, &from, sizeof from) && write_at(ufd, tp, &to, sizeof to) && fsync(ufd) == 0) {
                    entry.state = 3;
                    (void)write_at(jfd, 0, &entry, sizeof entry);
                    (void)fsync(jfd);
                }
            }
            unlock_file(ufd); close(ufd);
        }
    }
    unlock_file(jfd); close(jfd);
}
static int write_loan_journal(LoanJournal *entry, int state) {
    int fd = db_open(LOAN_JOURNAL_FILE, O_RDWR|O_CREAT);
    int good = 0;
    if (fd < 0) return -1;
    if (lock_file(fd, F_WRLCK) == 0) {
        entry->state = state;
        good = write_at(fd, 0, entry, sizeof *entry) && ftruncate(fd, sizeof *entry) == 0 && fsync(fd) == 0;
        unlock_file(fd);
    }
    close(fd);
    return good ? 0 : -1;
}
static void recover_prepared_loan_credit(void) {
    int jfd = db_open(LOAN_JOURNAL_FILE, O_RDWR|O_CREAT), ufd, lfd;
    LoanJournal entry; User applicant; Loan loan; off_t upos, lpos = 0;
    if (jfd < 0 || lock_file(jfd, F_WRLCK) < 0) { if (jfd >= 0) close(jfd); return; }
    if (read_at(jfd, 0, &entry, sizeof entry) && entry.state == 1) {
        ufd = db_open(USERS_FILE, O_RDWR); lfd = db_open(LOANS_FILE, O_RDWR);
        if (ufd >= 0 && lfd >= 0 && lock_file(ufd, F_WRLCK) == 0 && lock_file(lfd, F_WRLCK) == 0) {
            if (find_user_fd(ufd, entry.applicant, &applicant, &upos)) { applicant.balance = entry.balance_before; (void)write_at(ufd, upos, &applicant, sizeof applicant); (void)fsync(ufd); }
            while (read_at(lfd, lpos, &loan, sizeof loan)) { if (loan.id == entry.loan_id) { loan.status = 0; (void)write_at(lfd, lpos, &loan, sizeof loan); (void)fsync(lfd); break; } lpos += sizeof loan; }
            entry.state = 3; (void)write_at(jfd, 0, &entry, sizeof entry); (void)fsync(jfd);
            unlock_file(lfd); unlock_file(ufd);
        }
        if (ufd >= 0) close(ufd);
        if (lfd >= 0) close(lfd);
    }
    unlock_file(jfd); close(jfd);
}

static void bootstrap(void) {
    int fd; User u; struct stat s;
    if (mkdir(data_dir,0700)<0 && errno!=EEXIST) { perror("mkdir data directory"); exit(EXIT_FAILURE); }
    fd=db_open(USERS_FILE,O_RDWR|O_CREAT); if(fd<0){perror("open users");exit(EXIT_FAILURE);} if(fstat(fd,&s)<0){perror("stat users");close(fd);exit(EXIT_FAILURE);}
    if(s.st_size==0) { lock_file(fd,F_WRLCK);
        new_user(&u,"admin","admin123","System Administrator",ROLE_ADMIN,0);if(write(fd,&u,sizeof u)!=(ssize_t)sizeof u) goto bootstrap_failed;
        new_user(&u,"manager","manager123","Bank Manager",ROLE_MANAGER,0);if(write(fd,&u,sizeof u)!=(ssize_t)sizeof u) goto bootstrap_failed;
        new_user(&u,"employee","employee123","Loan Officer",ROLE_EMPLOYEE,0);if(write(fd,&u,sizeof u)!=(ssize_t)sizeof u) goto bootstrap_failed;
        new_user(&u,"customer","customer123","Demo Customer",ROLE_CUSTOMER,1000);if(write(fd,&u,sizeof u)!=(ssize_t)sizeof u) goto bootstrap_failed;
        if(fsync(fd)<0) goto bootstrap_failed;
        unlock_file(fd);
    } close(fd);
    (void)db_open(LOANS_FILE,O_RDWR|O_CREAT); (void)db_open(TXNS_FILE,O_RDWR|O_CREAT); (void)db_open(FEEDBACK_FILE,O_RDWR|O_CREAT);
    recover_prepared_transfer();
    recover_prepared_loan_credit();
    return;
bootstrap_failed:
    perror("write initial users"); unlock_file(fd); close(fd); exit(EXIT_FAILURE);
}
static int current_user(const char *username, User *out, off_t *pos) {
    int fd=db_open(USERS_FILE,O_RDONLY);int found=0;if(fd>=0){lock_file(fd,F_RDLCK);found=find_user_fd(fd,username,out,pos);unlock_file(fd);close(fd);}return found;
}

static void login(const Request *q, Response *r, char session[MAX_USERNAME], int *session_role) {
    int fd; User u; off_t pos; char digest[65];
    if(!valid_text(q->username,MAX_USERNAME)||!valid_text(q->password,MAX_PASSWORD)){respond(r,0,"Username and password are required.");return;}
    if(*session){respond(r,0,"This connection already has a logged-in user.");return;}
    fd=db_open(USERS_FILE,O_RDWR);if(fd<0||lock_file(fd,F_WRLCK)<0){if(fd>=0)close(fd);respond(r,0,"Database unavailable.");return;}
    if(!find_user_fd(fd,q->username,&u,&pos)){unlock_file(fd);close(fd);respond(r,0,"Invalid credentials.");return;}
    password_digest(q->password,u.salt,digest);
    if(strcmp(digest,u.password_hash)||u.role!=q->role){unlock_file(fd);close(fd);respond(r,0,"Invalid credentials or role.");return;}
    if(!u.active){unlock_file(fd);close(fd);respond(r,0,"Account is deactivated.");return;}
    if(u.logged_in){unlock_file(fd);close(fd);respond(r,0,"User is already logged in elsewhere.");return;}
    u.logged_in=1;if(!write_at(fd,pos,&u,sizeof u)||fsync(fd)<0){unlock_file(fd);close(fd);respond(r,0,"Could not start session.");return;}
    unlock_file(fd);close(fd);snprintf(session,MAX_USERNAME,"%s",u.username);*session_role=u.role;r->balance=u.balance;respond(r,1,"Login successful. Welcome, %s.",u.full_name);
}
static void logout_session(char session[MAX_USERNAME]) {
    int fd;User u;off_t pos;if(!session[0])return;fd=db_open(USERS_FILE,O_RDWR);if(fd>=0&&lock_file(fd,F_WRLCK)==0){if(find_user_fd(fd,session,&u,&pos)){u.logged_in=0;write_at(fd,pos,&u,sizeof u);fsync(fd);}unlock_file(fd);close(fd);}session[0]=0;
}
static void balance(const char *session, Response *r) { User u;if(!current_user(session,&u,NULL)){respond(r,0,"Account not found.");return;}r->balance=u.balance;respond(r,1,"Current balance: %.2f",u.balance); }
static void deposit(const char *session,const Request *q,Response *r) {
    int fd;User u;off_t pos;if(!valid_amount(q->amount)){respond(r,0,"Amount must be positive.");return;}sem_wait(write_gate);fd=db_open(USERS_FILE,O_RDWR);if(fd<0||lock_file(fd,F_WRLCK)<0){if(fd>=0)close(fd);sem_post(write_gate);respond(r,0,"Database unavailable.");return;}if(!find_user_fd(fd,session,&u,&pos)||!u.active){unlock_file(fd);close(fd);sem_post(write_gate);respond(r,0,"Account is unavailable.");return;}append_journal("BEGIN_DEPOSIT",session,q->amount,"customer deposit");u.balance+=q->amount;int good=write_at(fd,pos,&u,sizeof u)&&fsync(fd)==0;unlock_file(fd);close(fd);if(good)append_transaction(session,"DEPOSIT",q->amount,u.balance,"-");sem_post(write_gate);r->balance=u.balance;respond(r,good,"Deposit %s. Balance: %.2f",good?"completed":"failed",u.balance);
}
static void withdraw_money(const char *session,const Request *q,Response *r) {
    int fd;User u;off_t pos;if(!valid_amount(q->amount)){respond(r,0,"Amount must be positive.");return;}sem_wait(write_gate);fd=db_open(USERS_FILE,O_RDWR);if(fd<0||lock_file(fd,F_WRLCK)<0){if(fd>=0)close(fd);sem_post(write_gate);respond(r,0,"Database unavailable.");return;}if(!find_user_fd(fd,session,&u,&pos)||!u.active){unlock_file(fd);close(fd);sem_post(write_gate);respond(r,0,"Account is unavailable.");return;}if(u.balance<q->amount){unlock_file(fd);close(fd);sem_post(write_gate);respond(r,0,"Insufficient balance.");return;}append_journal("BEGIN_WITHDRAW",session,q->amount,"customer withdrawal");u.balance-=q->amount;int good=write_at(fd,pos,&u,sizeof u)&&fsync(fd)==0;unlock_file(fd);close(fd);if(good)append_transaction(session,"WITHDRAW",q->amount,u.balance,"-");sem_post(write_gate);r->balance=u.balance;respond(r,good,"Withdrawal %s. Balance: %.2f",good?"completed":"failed",u.balance);
}
static void transfer(const char *session,const Request *q,Response *r) {
    int fd;User from,to;off_t fp,tp;TransferJournal entry;
    if(!valid_amount(q->amount)||!valid_text(q->target,MAX_USERNAME)||!strcmp(session,q->target)){respond(r,0,"Provide a different recipient and a positive amount.");return;}
    sem_wait(write_gate);fd=db_open(USERS_FILE,O_RDWR);if(fd<0||lock_file(fd,F_WRLCK)<0){if(fd>=0)close(fd);sem_post(write_gate);respond(r,0,"Database unavailable.");return;}
    if(!find_user_fd(fd,session,&from,&fp)||!find_user_fd(fd,q->target,&to,&tp)||from.role!=ROLE_CUSTOMER||to.role!=ROLE_CUSTOMER||!from.active||!to.active){unlock_file(fd);close(fd);sem_post(write_gate);respond(r,0,"Both active customer accounts are required.");return;}
    if(from.balance<q->amount){unlock_file(fd);close(fd);sem_post(write_gate);respond(r,0,"Insufficient balance.");return;}
    memset(&entry, 0, sizeof entry); entry.epoch=time(NULL); entry.amount=q->amount; entry.from_before=from.balance; entry.to_before=to.balance; snprintf(entry.from,sizeof entry.from,"%s",session); snprintf(entry.to,sizeof entry.to,"%s",q->target);
    if (write_transfer_journal(&entry, 1) != 0) { unlock_file(fd); close(fd); sem_post(write_gate); respond(r,0,"Could not prepare durable transfer."); return; }
    append_journal("BEGIN_TRANSFER",session,q->amount,q->target);from.balance-=q->amount;to.balance+=q->amount;int good=write_at(fd,fp,&from,sizeof from)&&write_at(fd,tp,&to,sizeof to)&&fsync(fd)==0;
    if (!good) { from.balance=entry.from_before;to.balance=entry.to_before;(void)write_at(fd,fp,&from,sizeof from);(void)write_at(fd,tp,&to,sizeof to);(void)fsync(fd);(void)write_transfer_journal(&entry,3); }
    if (good && write_transfer_journal(&entry, 2) != 0) { from.balance=entry.from_before;to.balance=entry.to_before;good=write_at(fd,fp,&from,sizeof from)&&write_at(fd,tp,&to,sizeof to)&&fsync(fd)==0;write_transfer_journal(&entry,3); }
    unlock_file(fd);close(fd);if(good && entry.state==2){append_transaction(session,"TRANSFER_OUT",q->amount,from.balance,q->target);append_transaction(q->target,"TRANSFER_IN",q->amount,to.balance,session);append_journal("COMMIT_TRANSFER",session,q->amount,q->target);}sem_post(write_gate);r->balance=from.balance;respond(r,good && entry.state==2,"Transfer %s. Balance: %.2f",good&&entry.state==2?"completed":"failed",from.balance);
}
static void apply_loan(const char *session,const Request *q,Response *r) {
    int fd;Loan loan;off_t end;if(!valid_amount(q->amount)){respond(r,0,"Loan amount must be positive.");return;}fd=db_open(LOANS_FILE,O_RDWR|O_CREAT);if(fd<0||lock_file(fd,F_WRLCK)<0){if(fd>=0)close(fd);respond(r,0,"Loan database unavailable.");return;}end=lseek(fd,0,SEEK_END);memset(&loan,0,sizeof loan);loan.id=(int32_t)(end/(off_t)sizeof loan+1);snprintf(loan.applicant,sizeof loan.applicant,"%s",session);loan.amount=q->amount;loan.status=0;snprintf(loan.note,sizeof loan.note,"%s",q->text[0]?q->text:"Pending review");int good=write(fd,&loan,sizeof loan)==(ssize_t)sizeof loan&&fsync(fd)==0;unlock_file(fd);close(fd);if(good)append_journal("LOAN_APPLIED",session,q->amount,"pending");respond(r,good,good?"Loan application #%d submitted.":"Could not submit loan.",loan.id);
}
static void change_password(const char *session,const Request *q,Response *r) {
    int fd;User u;off_t pos;if(!valid_text(q->password,MAX_PASSWORD)){respond(r,0,"New password is required.");return;}fd=db_open(USERS_FILE,O_RDWR);if(fd<0||lock_file(fd,F_WRLCK)<0){if(fd>=0)close(fd);respond(r,0,"Database unavailable.");return;}if(!find_user_fd(fd,session,&u,&pos)){unlock_file(fd);close(fd);respond(r,0,"Account unavailable.");return;}make_salt(u.salt,u.username);password_digest(q->password,u.salt,u.password_hash);int good=write_at(fd,pos,&u,sizeof u)&&fsync(fd)==0;unlock_file(fd);close(fd);respond(r,good,good?"Password changed.":"Password change failed.");
}
static void add_feedback(const char *session,const Request *q,Response *r) { int fd;Feedback f;off_t end;if(!valid_text(q->text,MAX_TEXT)){respond(r,0,"Feedback cannot be empty.");return;}fd=db_open(FEEDBACK_FILE,O_RDWR|O_CREAT);if(fd<0||lock_file(fd,F_WRLCK)<0){if(fd>=0)close(fd);respond(r,0,"Feedback storage unavailable.");return;}end=lseek(fd,0,SEEK_END);memset(&f,0,sizeof f);f.id=(int32_t)(end/(off_t)sizeof f+1);snprintf(f.customer,sizeof f.customer,"%s",session);snprintf(f.text,sizeof f.text,"%s",q->text);f.epoch=time(NULL);int good=write(fd,&f,sizeof f)==(ssize_t)sizeof f&&fsync(fd)==0;unlock_file(fd);close(fd);respond(r,good,good?"Feedback recorded.":"Feedback could not be recorded."); }
static void history(const char *username,Response *r) { int fd=db_open(TXNS_FILE,O_RDONLY|O_CREAT);Transaction t;size_t used=0;if(fd<0||lock_file(fd,F_RDLCK)<0){if(fd>=0)close(fd);respond(r,0,"Transaction database unavailable.");return;}while(read(fd,&t,sizeof t)==(ssize_t)sizeof t)if(!strncmp(t.customer,username,MAX_USERNAME)){int n=snprintf(r->data+used,sizeof r->data-used,"#%d %-13s %10.2f balance %10.2f other %s at %lld\n",t.id,t.kind,t.amount,t.balance_after,t.counterparty,(long long)t.epoch);if(n<0||(size_t)n>=sizeof r->data-used)break;used+=(size_t)n;}unlock_file(fd);close(fd);respond(r,1,used?"Transaction history returned.":"No transactions yet."); }

static void add_person(const Request *q,Response *r,int role) { int fd;User u;if(!valid_text(q->username,MAX_USERNAME)||!valid_text(q->password,MAX_PASSWORD)||!valid_text(q->text,MAX_TEXT)){respond(r,0,"Username, password, and full name are required.");return;}fd=db_open(USERS_FILE,O_RDWR);if(fd<0||lock_file(fd,F_WRLCK)<0){if(fd>=0)close(fd);respond(r,0,"Database unavailable.");return;}if(find_user_fd(fd,q->username,NULL,NULL)){unlock_file(fd);close(fd);respond(r,0,"Username already exists.");return;}new_user(&u,q->username,q->password,q->text,role,role==ROLE_CUSTOMER?q->amount:0);int good=write(fd,&u,sizeof u)==(ssize_t)sizeof u&&fsync(fd)==0;unlock_file(fd);close(fd);respond(r,good,good?"%s account created.":"Account creation failed.",role_name(role)); }
static void modify_user(const Request *q,Response *r,int allow_role) { int fd;User u;off_t pos;if(!valid_text(q->target,MAX_USERNAME)){respond(r,0,"Target username is required.");return;}fd=db_open(USERS_FILE,O_RDWR);if(fd<0||lock_file(fd,F_WRLCK)<0){if(fd>=0)close(fd);respond(r,0,"Database unavailable.");return;}if(!find_user_fd(fd,q->target,&u,&pos)){unlock_file(fd);close(fd);respond(r,0,"Target user not found.");return;}if(q->text[0])snprintf(u.full_name,sizeof u.full_name,"%.*s",MAX_NAME-1,q->text);if(q->amount>=0&&u.role==ROLE_CUSTOMER)u.balance=q->amount;if(allow_role&&q->role>=ROLE_CUSTOMER&&q->role<=ROLE_ADMIN)u.role=q->role;int good=write_at(fd,pos,&u,sizeof u)&&fsync(fd)==0;unlock_file(fd);close(fd);respond(r,good,good?"User details updated.":"Update failed."); }
static void set_account_active(const Request *q,Response *r) { int fd;User u;off_t pos;if(!valid_text(q->target,MAX_USERNAME)){respond(r,0,"Customer username is required.");return;}fd=db_open(USERS_FILE,O_RDWR);if(fd<0||lock_file(fd,F_WRLCK)<0){if(fd>=0)close(fd);respond(r,0,"Database unavailable.");return;}if(!find_user_fd(fd,q->target,&u,&pos)||u.role!=ROLE_CUSTOMER){unlock_file(fd);close(fd);respond(r,0,"Customer not found.");return;}u.active=q->id?1:0;if(!u.active)u.logged_in=0;int good=write_at(fd,pos,&u,sizeof u)&&fsync(fd)==0;unlock_file(fd);close(fd);respond(r,good,good?"Customer account %s.":"Account update failed.",q->id?"activated":"deactivated"); }
static void close_customer(const Request *q, Response *r) {
    Request deactivate = *q;
    deactivate.id = 0;
    set_account_active(&deactivate, r);
    if (r->ok) snprintf(r->message, sizeof r->message, "Customer account closed (deactivated).");
}
static void assign_loan(const Request *q,Response *r) { int lfd,ufd;Loan loan;User e;off_t pos=0;if(q->id<=0||!valid_text(q->target,MAX_USERNAME)){respond(r,0,"Loan id and employee username are required.");return;}ufd=db_open(USERS_FILE,O_RDONLY);if(ufd<0||lock_file(ufd,F_RDLCK)<0){if(ufd>=0)close(ufd);respond(r,0,"User database unavailable.");return;}int employee=find_user_fd(ufd,q->target,&e,NULL)&&e.role==ROLE_EMPLOYEE&&e.active;unlock_file(ufd);close(ufd);if(!employee){respond(r,0,"Active employee not found.");return;}lfd=db_open(LOANS_FILE,O_RDWR);if(lfd<0||lock_file(lfd,F_WRLCK)<0){if(lfd>=0)close(lfd);respond(r,0,"Loan database unavailable.");return;}while(read_at(lfd,pos,&loan,sizeof loan)){if(loan.id==q->id)break;pos+=sizeof loan;}if(loan.id!=q->id||loan.status){unlock_file(lfd);close(lfd);respond(r,0,"Pending loan not found.");return;}snprintf(loan.assigned_to,sizeof loan.assigned_to,"%s",q->target);int good=write_at(lfd,pos,&loan,sizeof loan)&&fsync(lfd)==0;unlock_file(lfd);close(lfd);respond(r,good,good?"Loan assigned.":"Loan assignment failed."); }
static void assigned_loans(const char *session,Response *r) { int fd=db_open(LOANS_FILE,O_RDONLY|O_CREAT);Loan loan;size_t used=0;if(fd<0||lock_file(fd,F_RDLCK)<0){if(fd>=0)close(fd);respond(r,0,"Loan database unavailable.");return;}while(read(fd,&loan,sizeof loan)==(ssize_t)sizeof loan)if(!strncmp(loan.assigned_to,session,MAX_USERNAME)){int n=snprintf(r->data+used,sizeof r->data-used,"#%d applicant=%s amount=%.2f status=%s note=%s\n",loan.id,loan.applicant,loan.amount,loan.status==0?"PENDING":loan.status==1?"APPROVED":"REJECTED",loan.note);if(n<0||(size_t)n>=sizeof r->data-used)break;used+=(size_t)n;}unlock_file(fd);close(fd);respond(r,1,used?"Assigned loans returned.":"No assigned loans."); }
static void pending_loans(Response *r) { int fd=db_open(LOANS_FILE,O_RDONLY|O_CREAT);Loan loan;size_t used=0;if(fd<0||lock_file(fd,F_RDLCK)<0){if(fd>=0)close(fd);respond(r,0,"Loan database unavailable.");return;}while(read(fd,&loan,sizeof loan)==(ssize_t)sizeof loan)if(loan.status==0){int n=snprintf(r->data+used,sizeof r->data-used,"#%d applicant=%s amount=%.2f assigned=%s note=%s\n",loan.id,loan.applicant,loan.amount,loan.assigned_to[0]?loan.assigned_to:"UNASSIGNED",loan.note);if(n<0||(size_t)n>=sizeof r->data-used)break;used+=(size_t)n;}unlock_file(fd);close(fd);respond(r,1,used?"Pending loan applications returned.":"No pending loan applications."); }
static void my_loans(const char *customer, Response *r) { int fd=db_open(LOANS_FILE,O_RDONLY|O_CREAT);Loan loan;size_t used=0;if(fd<0||lock_file(fd,F_RDLCK)<0){if(fd>=0)close(fd);respond(r,0,"Loan database unavailable.");return;}while(read(fd,&loan,sizeof loan)==(ssize_t)sizeof loan)if(!strncmp(loan.applicant,customer,MAX_USERNAME)){int n=snprintf(r->data+used,sizeof r->data-used,"#%d amount=%.2f status=%s assigned=%s note=%s\n",loan.id,loan.amount,loan.status==0?"PENDING":loan.status==1?"APPROVED":"REJECTED",loan.assigned_to[0]?loan.assigned_to:"UNASSIGNED",loan.note);if(n<0||(size_t)n>=sizeof r->data-used)break;used+=(size_t)n;}unlock_file(fd);close(fd);respond(r,1,used?"Loan status returned.":"No loan applications yet."); }
static void loan_decision(const char *employee,const Request *q,Response *r) { int fd,ufd=-1;Loan loan;User applicant;LoanJournal entry;off_t pos=0,upos;if(q->id<=0||(q->amount!=1&&q->amount!=2)){respond(r,0,"Use loan id and decision 1 (approve) or 2 (reject).");return;}fd=db_open(LOANS_FILE,O_RDWR);if(fd<0||lock_file(fd,F_WRLCK)<0){if(fd>=0)close(fd);respond(r,0,"Loan database unavailable.");return;}while(read_at(fd,pos,&loan,sizeof loan)){if(loan.id==q->id)break;pos+=sizeof loan;}if(loan.id!=q->id||strcmp(loan.assigned_to,employee)||loan.status){unlock_file(fd);close(fd);respond(r,0,"Assigned pending loan not found.");return;}
    if (q->amount == 1) { ufd=db_open(USERS_FILE,O_RDWR);if(ufd<0||lock_file(ufd,F_WRLCK)<0){if(ufd>=0)close(ufd);unlock_file(fd);close(fd);respond(r,0,"Customer database unavailable.");return;}if(!find_user_fd(ufd,loan.applicant,&applicant,&upos)||!applicant.active){unlock_file(ufd);close(ufd);unlock_file(fd);close(fd);respond(r,0,"Active applicant account not found.");return;}memset(&entry,0,sizeof entry);entry.loan_id=loan.id;entry.epoch=time(NULL);snprintf(entry.applicant,sizeof entry.applicant,"%s",loan.applicant);entry.amount=loan.amount;entry.balance_before=applicant.balance;if(write_loan_journal(&entry,1)!=0){unlock_file(ufd);close(ufd);unlock_file(fd);close(fd);respond(r,0,"Could not prepare loan disbursement.");return;}applicant.balance+=loan.amount;if(!write_at(ufd,upos,&applicant,sizeof applicant)||fsync(ufd)<0){applicant.balance=entry.balance_before;(void)write_at(ufd,upos,&applicant,sizeof applicant);(void)fsync(ufd);(void)write_loan_journal(&entry,3);unlock_file(ufd);close(ufd);unlock_file(fd);close(fd);respond(r,0,"Loan disbursement failed.");return;} }
    loan.status=(int)q->amount;if(q->text[0])snprintf(loan.note,sizeof loan.note,"%s",q->text);int good=write_at(fd,pos,&loan,sizeof loan)&&fsync(fd)==0;
    if(q->amount==1&&(!good||write_loan_journal(&entry,2)!=0)){applicant.balance=entry.balance_before;(void)write_at(ufd,upos,&applicant,sizeof applicant);(void)fsync(ufd);loan.status=0;(void)write_at(fd,pos,&loan,sizeof loan);(void)fsync(fd);(void)write_loan_journal(&entry,3);good=0;}
    if(ufd>=0){unlock_file(ufd);close(ufd);}unlock_file(fd);close(fd);if(good&&q->amount==1)append_transaction(loan.applicant,"LOAN_CREDIT",loan.amount,applicant.balance,"BANK");respond(r,good,good?(q->amount==1?"Loan approved and credited to customer account.":"Loan rejected."):"Loan decision failed."); }
static void list_feedback(Response *r) { int fd=db_open(FEEDBACK_FILE,O_RDONLY|O_CREAT);Feedback f;size_t used=0;if(fd<0||lock_file(fd,F_RDLCK)<0){if(fd>=0)close(fd);respond(r,0,"Feedback storage unavailable.");return;}while(read(fd,&f,sizeof f)==(ssize_t)sizeof f){int n=snprintf(r->data+used,sizeof r->data-used,"#%d %s at %lld: %s\n",f.id,f.customer,(long long)f.epoch,f.text);if(n<0||(size_t)n>=sizeof r->data-used)break;used+=(size_t)n;}unlock_file(fd);close(fd);respond(r,1,used?"Feedback returned.":"No feedback yet."); }

static void handle_request(const Request *q,Response *r,char session[MAX_USERNAME],int *session_role) {
    blank(r);if(q->operation==OP_LOGIN){login(q,r,session,session_role);return;}if(!session[0]){respond(r,0,"Please log in first.");return;}if(q->operation==OP_LOGOUT||q->operation==OP_EXIT){logout_session(session);*session_role=0;respond(r,1,"Logged out.");return;}if(q->operation==OP_CHANGE_PASSWORD){change_password(session,q,r);return;}
    switch(*session_role) {
    case ROLE_CUSTOMER:
        if(q->operation==OP_BALANCE)balance(session,r);else if(q->operation==OP_DEPOSIT)deposit(session,q,r);else if(q->operation==OP_WITHDRAW)withdraw_money(session,q,r);else if(q->operation==OP_TRANSFER)transfer(session,q,r);else if(q->operation==OP_APPLY_LOAN)apply_loan(session,q,r);else if(q->operation==OP_FEEDBACK)add_feedback(session,q,r);else if(q->operation==OP_HISTORY)history(session,r);else if(q->operation==OP_LIST_MY_LOANS)my_loans(session,r);else respond(r,0,"Operation not permitted for customer.");break;
    case ROLE_EMPLOYEE:
        if(q->operation==OP_ADD_CUSTOMER)add_person(q,r,ROLE_CUSTOMER);else if(q->operation==OP_MODIFY_USER)modify_user(q,r,0);else if(q->operation==OP_CLOSE_ACCOUNT)close_customer(q,r);else if(q->operation==OP_LOAN_DECISION)loan_decision(session,q,r);else if(q->operation==OP_LIST_ASSIGNED_LOANS)assigned_loans(session,r);else if(q->operation==OP_CUSTOMER_HISTORY){if(valid_text(q->target,MAX_USERNAME))history(q->target,r);else respond(r,0,"Customer username required.");}else respond(r,0,"Operation not permitted for employee.");break;
    case ROLE_MANAGER:
        if(q->operation==OP_ACCOUNT_STATE)set_account_active(q,r);else if(q->operation==OP_ASSIGN_LOAN)assign_loan(q,r);else if(q->operation==OP_LIST_PENDING_LOANS)pending_loans(r);else if(q->operation==OP_LIST_FEEDBACK)list_feedback(r);else respond(r,0,"Operation not permitted for manager.");break;
    case ROLE_ADMIN:
        if(q->operation==OP_ADD_EMPLOYEE)add_person(q,r,ROLE_EMPLOYEE);else if(q->operation==OP_MODIFY_USER)modify_user(q,r,0);else if(q->operation==OP_SET_ROLE)modify_user(q,r,1);else respond(r,0,"Operation not permitted for administrator.");break;
    default: respond(r,0,"Invalid session role.");
    }
}
static void *client_thread(void *arg) { int fd=(int)(intptr_t)arg;Request q;Response r;char session[MAX_USERNAME]={0};int role=0;while(recv_all(fd,&q,sizeof q)==0){handle_request(&q,&r,session,&role);if(send_all(fd,&r,sizeof r)<0)break;if(q.operation==OP_EXIT)break;}logout_session(session);close(fd);return NULL; }
int main(int argc,char **argv) { int server,yes=1,port=BANK_PORT;struct sockaddr_in addr;if(argc>1)snprintf(data_dir,sizeof data_dir,"%s",argv[1]);if(argc>2)port=atoi(argv[2]);if(port<1024||port>65535){fprintf(stderr,"Port must be between 1024 and 65535.\n");return EXIT_FAILURE;}signal(SIGPIPE,SIG_IGN);bootstrap();write_gate=sem_open("/bank_management_write_gate",O_CREAT,0600,1);if(write_gate==SEM_FAILED){perror("sem_open");return EXIT_FAILURE;}server=socket(AF_INET,SOCK_STREAM,0);if(server<0){perror("socket");return EXIT_FAILURE;}setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes);memset(&addr,0,sizeof addr);addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_ANY);addr.sin_port=htons((uint16_t)port);if(bind(server,(struct sockaddr*)&addr,sizeof addr)<0||listen(server,16)<0){perror("bind/listen");close(server);return EXIT_FAILURE;}printf("Bank server listening on port %d using %s\n",port,data_dir);for(;;){int client=accept(server,NULL,NULL);pthread_t t;if(client<0){if(errno==EINTR)continue;perror("accept");break;}if(pthread_create(&t,NULL,client_thread,(void*)(intptr_t)client)==0)pthread_detach(t);else close(client);}close(server);sem_close(write_gate);return 0; }
