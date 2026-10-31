"""Build the Banking Management System viva study guide PDF."""
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.platypus import (
    KeepTogether,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)
from reportlab.pdfbase.pdfmetrics import stringWidth


OUT = Path("output/pdf/Banking_Management_System_Viva_Study_Guide.pdf")
OUT.parent.mkdir(parents=True, exist_ok=True)

NAVY = colors.HexColor("#0B1F3A")
BLUE = colors.HexColor("#1D5D9B")
TEAL = colors.HexColor("#087E8B")
LIGHT_BLUE = colors.HexColor("#EAF3FA")
PALE = colors.HexColor("#F6F8FB")
INK = colors.HexColor("#1C2733")
MUTED = colors.HexColor("#52606D")
GREEN = colors.HexColor("#DDF3E4")
ORANGE = colors.HexColor("#FFF0D6")
RED = colors.HexColor("#FBE6E6")

styles = getSampleStyleSheet()
styles.add(ParagraphStyle(name="GuideTitle", parent=styles["Title"], fontName="Helvetica-Bold", fontSize=25, leading=30, textColor=NAVY, alignment=TA_CENTER, spaceAfter=8))
styles.add(ParagraphStyle(name="Subtitle", parent=styles["Normal"], fontName="Helvetica", fontSize=11, leading=15, textColor=MUTED, alignment=TA_CENTER, spaceAfter=14))
styles.add(ParagraphStyle(name="H1Guide", parent=styles["Heading1"], fontName="Helvetica-Bold", fontSize=17, leading=22, textColor=NAVY, spaceBefore=2, spaceAfter=8))
styles.add(ParagraphStyle(name="H2Guide", parent=styles["Heading2"], fontName="Helvetica-Bold", fontSize=11.5, leading=15, textColor=BLUE, spaceBefore=8, spaceAfter=4))
styles.add(ParagraphStyle(name="BodyGuide", parent=styles["BodyText"], fontName="Helvetica", fontSize=9.2, leading=13, textColor=INK, spaceAfter=5))
styles.add(ParagraphStyle(name="Small", parent=styles["BodyText"], fontName="Helvetica", fontSize=8, leading=10.5, textColor=INK))
styles.add(ParagraphStyle(name="CodeGuide", parent=styles["BodyText"], fontName="Courier", fontSize=8.4, leading=11, textColor=NAVY, backColor=PALE, borderColor=colors.HexColor("#D9E2EC"), borderWidth=0.5, borderPadding=6, spaceAfter=7))
styles.add(ParagraphStyle(name="Question", parent=styles["BodyText"], fontName="Helvetica-Bold", fontSize=9.3, leading=12, textColor=NAVY, spaceBefore=4, spaceAfter=1))


def p(text, style="BodyGuide"):
    return Paragraph(text, styles[style])


def bullet(text):
    return Paragraph(f"<bullet>&bull;</bullet> {text}", styles["BodyGuide"])


def table(rows, widths, header=True, small=False):
    processed = []
    style = "Small" if small else "BodyGuide"
    for row in rows:
        processed.append([cell if isinstance(cell, Paragraph) else p(str(cell), style) for cell in row])
    t = Table(processed, colWidths=widths, repeatRows=1 if header else 0, hAlign="LEFT")
    commands = [
        ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#C8D4E3")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 6),
        ("RIGHTPADDING", (0, 0), (-1, -1), 6),
        ("TOPPADDING", (0, 0), (-1, -1), 5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
        ("BACKGROUND", (0, 1), (-1, -1), colors.white),
    ]
    if header:
        commands += [("BACKGROUND", (0, 0), (-1, 0), NAVY), ("TEXTCOLOR", (0, 0), (-1, 0), colors.white)]
    t.setStyle(TableStyle(commands))
    return t


def page_frame(canvas, doc):
    canvas.saveState()
    w, h = A4
    canvas.setFillColor(NAVY)
    canvas.rect(0, h - 12 * mm, w, 12 * mm, fill=1, stroke=0)
    canvas.setFillColor(colors.white)
    canvas.setFont("Helvetica-Bold", 8.5)
    canvas.drawString(17 * mm, h - 7.5 * mm, "BANKING MANAGEMENT SYSTEM - VIVA STUDY GUIDE")
    canvas.setFillColor(MUTED)
    canvas.setFont("Helvetica", 8)
    canvas.drawRightString(w - 17 * mm, 10 * mm, f"Page {doc.page}")
    canvas.restoreState()


def box(canvas, x, y, width, height, title, body, fill=LIGHT_BLUE):
    canvas.saveState()
    canvas.setFillColor(fill)
    canvas.setStrokeColor(BLUE)
    canvas.roundRect(x, y, width, height, 4, fill=1, stroke=1)
    canvas.setFillColor(NAVY)
    canvas.setFont("Helvetica-Bold", 8.5)
    canvas.drawCentredString(x + width / 2, y + height - 12, title)
    canvas.setFillColor(INK)
    canvas.setFont("Helvetica", 7.4)
    yy = y + height - 24
    for line in body.split("\n"):
        canvas.drawCentredString(x + width / 2, yy, line)
        yy -= 9
    canvas.restoreState()


def arrow(canvas, x1, y1, x2, y2, label=""):
    canvas.saveState()
    canvas.setStrokeColor(TEAL)
    canvas.setFillColor(TEAL)
    canvas.setLineWidth(1.2)
    canvas.line(x1, y1, x2, y2)
    angle = 0
    if x2 != x1:
        angle = 0 if x2 > x1 else 180
    else:
        angle = 90 if y2 > y1 else -90
    canvas.translate(x2, y2)
    canvas.rotate(angle)
    canvas.line(0, 0, -5, 3)
    canvas.line(0, 0, -5, -3)
    canvas.restoreState()
    if label:
        canvas.setFillColor(MUTED)
        canvas.setFont("Helvetica", 7)
        canvas.drawCentredString((x1 + x2) / 2, (y1 + y2) / 2 + 4, label)


def architecture_diagram(canvas, doc):
    w, h = A4
    x = 22 * mm
    box(canvas, x, h - 82 * mm, 36 * mm, 22 * mm, "CLIENT", "Menu input\nRequest struct", colors.HexColor("#E8F1FB"))
    box(canvas, x + 55 * mm, h - 82 * mm, 44 * mm, 22 * mm, "TCP SERVER", "accept()\npthread per client", colors.HexColor("#E5F6F4"))
    box(canvas, x + 117 * mm, h - 82 * mm, 45 * mm, 22 * mm, "REQUEST HANDLER", "Role authorization\nBusiness rules", colors.HexColor("#FFF0D6"))
    arrow(canvas, x + 36 * mm, h - 71 * mm, x + 55 * mm, h - 71 * mm, "socket")
    arrow(canvas, x + 99 * mm, h - 71 * mm, x + 117 * mm, h - 71 * mm, "dispatch")
    box(canvas, x + 18 * mm, h - 129 * mm, 40 * mm, 29 * mm, "USERS", "users.dat\nUser records\nbalance / sessions", colors.HexColor("#EAF3FA"))
    box(canvas, x + 70 * mm, h - 129 * mm, 40 * mm, 29 * mm, "LOANS", "loans.dat\nLoan records\nassign / decision", colors.HexColor("#EAF3FA"))
    box(canvas, x + 122 * mm, h - 129 * mm, 40 * mm, 29 * mm, "AUDIT DATA", "transactions.dat\nfeedback.dat\njournal files", colors.HexColor("#EAF3FA"))
    arrow(canvas, x + 139 * mm, h - 82 * mm, x + 38 * mm, h - 100 * mm, "fcntl lock")
    arrow(canvas, x + 139 * mm, h - 82 * mm, x + 90 * mm, h - 100 * mm, "fcntl lock")
    arrow(canvas, x + 139 * mm, h - 82 * mm, x + 142 * mm, h - 100 * mm, "fsync")
    canvas.setFillColor(MUTED)
    canvas.setFont("Helvetica-Oblique", 7.6)
    canvas.drawString(22 * mm, h - 142 * mm, "The named POSIX semaphore serializes balance-changing operations; file locks protect records across processes.")


def structure_diagram(canvas, doc):
    w, h = A4
    x = 20 * mm
    box(canvas, x, h - 78 * mm, 46 * mm, 32 * mm, "Request", "operation, role\nusername, password\ntarget, amount, text", colors.HexColor("#FFF0D6"))
    box(canvas, x + 58 * mm, h - 78 * mm, 46 * mm, 32 * mm, "Response", "ok, balance\nmessage\ndata", colors.HexColor("#E5F6F4"))
    box(canvas, x + 116 * mm, h - 78 * mm, 46 * mm, 32 * mm, "User", "username, hash, salt\nrole, active, logged_in\nbalance", colors.HexColor("#EAF3FA"))
    box(canvas, x + 15 * mm, h - 126 * mm, 42 * mm, 32 * mm, "Loan", "id, applicant, amount\nstatus, assigned_to\nnote", colors.HexColor("#EAF3FA"))
    box(canvas, x + 68 * mm, h - 126 * mm, 42 * mm, 32 * mm, "Transaction", "id, customer, kind\namount, balance_after\ncounterparty", colors.HexColor("#EAF3FA"))
    box(canvas, x + 121 * mm, h - 126 * mm, 42 * mm, 32 * mm, "Feedback", "id, customer\ntext, epoch", colors.HexColor("#EAF3FA"))
    box(canvas, x + 43 * mm, h - 170 * mm, 58 * mm, 27 * mm, "TransferJournal", "state, from, to, amount\npre-transfer balances", colors.HexColor("#F3EAFB"))
    box(canvas, x + 110 * mm, h - 170 * mm, 52 * mm, 27 * mm, "LoanJournal", "state, loan_id, applicant\namount, prior balance", colors.HexColor("#F3EAFB"))
    arrow(canvas, x + 23 * mm, h - 78 * mm, x + 36 * mm, h - 94 * mm, "creates")
    arrow(canvas, x + 139 * mm, h - 78 * mm, x + 36 * mm, h - 94 * mm, "applicant")
    arrow(canvas, x + 139 * mm, h - 78 * mm, x + 89 * mm, h - 94 * mm, "customer")
    arrow(canvas, x + 139 * mm, h - 78 * mm, x + 142 * mm, h - 94 * mm, "customer")
    arrow(canvas, x + 56 * mm, h - 126 * mm, x + 72 * mm, h - 143 * mm, "protects")
    arrow(canvas, x + 89 * mm, h - 126 * mm, x + 136 * mm, h - 143 * mm, "protects")


def diagram_block(draw_fn, height):
    def painter(canvas, doc):
        draw_fn(canvas, doc)
    # Canvas drawings happen in a dedicated page template callback. This spacer reserves the diagram area.
    return Spacer(1, height)


def decorated_page(canvas, doc):
    page_frame(canvas, doc)
    if doc.page == 2:
        architecture_diagram(canvas, doc)
    elif doc.page == 3:
        structure_diagram(canvas, doc)


def build():
    doc = SimpleDocTemplate(
        str(OUT), pagesize=A4,
        leftMargin=17 * mm, rightMargin=17 * mm, topMargin=18 * mm, bottomMargin=16 * mm,
        title="Banking Management System Viva Study Guide",
        author="Codex",
    )
    story = []
    story += [Spacer(1, 24 * mm), p("Banking Management System", "GuideTitle"), p("Blueprint, C structure diagram, code walkthrough, and viva preparation", "Subtitle")]
    story += [p("What this project demonstrates", "H1Guide")]
    story += [p("This is a client-server banking simulation written in C for Linux. A TCP server owns the persistent database files. Clients authenticate by role and send fixed-size <b>Request</b> records; the server returns <b>Response</b> records. The design intentionally demonstrates sockets, POSIX threads, file locking, semaphores, persistence, recovery journals, and role-based access control.")]
    story += [table([
        ["Viva topic", "Where it appears in the project", "One sentence to say"],
        ["Socket programming", "bank_server accepts TCP clients; bank_client connects to it", "The server centralizes the database and lets many clients connect concurrently."],
        ["Threads", "pthread_create creates one detached worker per client", "Each client is handled independently without blocking new connections."],
        ["File management", "Binary .dat files store users, loans, transactions and feedback", "Records remain after the program stops, unlike in-memory arrays."],
        ["Synchronization", "fcntl locks plus a named semaphore", "Locks prevent race conditions while a balance or record is updated."],
        ["Security", "salted SHA-256 password digest and one active session", "The password is verified as a digest and duplicate login is rejected."],
    ], [34 * mm, 67 * mm, 60 * mm])]
    story += [Spacer(1, 5 * mm), p("Suggested demonstration order", "H2Guide"), bullet("Customer applies for a loan."), bullet("Manager lists pending loan IDs and assigns one to an employee."), bullet("Employee approves it; the amount is credited and a passbook entry is created."), bullet("Customer checks loan status, balance, and transaction history.")]
    story.append(PageBreak())

    story += [p("1. System Blueprint", "H1Guide"), Spacer(1, 77 * mm)]
    story += [p("Request lifecycle", "H2Guide"), table([
        ["Step", "What happens", "Key code concept"],
        ["1", "Client takes menu input and fills a Request structure.", "Fixed binary request protocol"],
        ["2", "Client sends Request over TCP; server receives it fully.", "send()/recv() loops"],
        ["3", "A server thread checks the logged-in role and dispatches the operation.", "Role-based authorization"],
        ["4", "The operation locks the relevant file, reads or changes a record, then fsyncs mutations.", "fcntl advisory lock + durability"],
        ["5", "Server sends a Response containing success/failure, message, balance, and optional rows.", "Request-response protocol"],
    ], [14 * mm, 102 * mm, 45 * mm])]
    story += [p("Files created in the data directory", "H2Guide"), table([
        ["File", "Purpose"],
        ["users.dat", "User credentials (salted digest), role, active/session state and customer balance."],
        ["loans.dat", "Loan applications, employee assignment, status and decision note."],
        ["transactions.dat", "Passbook-style deposits, withdrawals, transfers and approved-loan credits."],
        ["feedback.dat", "Customer feedback visible to managers."],
        ["journal.log plus transfer/loan journal files", "Audit entries and recovery data for incomplete critical updates."],
    ], [55 * mm, 106 * mm])]
    story.append(PageBreak())

    story += [p("2. C Structure Diagram", "H1Guide"), Spacer(1, 119 * mm)]
    story += [p("How to explain the diagram", "H2Guide"), bullet("<b>Request</b> and <b>Response</b> are network messages. They keep the client and server protocol simple and predictable."), bullet("<b>User</b> is the core record. For customers, its <i>balance</i> field is meaningful; for staff it remains zero."), bullet("<b>Loan</b>, <b>Transaction</b>, and <b>Feedback</b> are appendable business records linked by username."), bullet("The two journal structures capture a previous balance before a critical transfer or loan credit. If the server restarts while an entry is prepared but not committed, startup recovery rolls it back.")]
    story += [p("Important enum values", "H2Guide"), table([
        ["Enum", "Meaning"],
        ["Role", "ROLE_CUSTOMER, ROLE_EMPLOYEE, ROLE_MANAGER, ROLE_ADMIN"],
        ["Loan status", "0 PENDING, 1 APPROVED, 2 REJECTED"],
        ["Journal state", "1 PREPARED, 2 COMMITTED, 3 ROLLED BACK"],
    ], [45 * mm, 116 * mm])]
    story.append(PageBreak())

    story += [p("3. Core Workflows", "H1Guide")]
    story += [p("Login and one-session rule", "H2Guide"), p("During login, the server locks <b>users.dat</b>, finds the username, computes a SHA-256 digest using the saved salt, verifies the selected role, checks that the account is active and not already logged in, then writes <i>logged_in = 1</i>. Logout or a disconnected client clears this flag.")]
    story += [p("Transfer workflow", "H2Guide"), table([
        ["Order", "Action", "Why it matters"],
        ["1", "Validate positive amount, recipient, active customer accounts and sufficient funds.", "Rejects invalid business operations before changing data."],
        ["2", "Acquire named semaphore and exclusive users.dat lock.", "Avoids two simultaneous withdrawals or transfers reading the same old balance."],
        ["3", "Write PREPARED TransferJournal with both previous balances and fsync it.", "Provides a recovery point before account values change."],
        ["4", "Debit sender and credit receiver under one users.dat lock, then fsync.", "The paired account update is serialized."],
        ["5", "Mark journal COMMITTED and append two transaction/passbook records.", "Leaves audit evidence for both customers."],
    ], [13 * mm, 99 * mm, 49 * mm])]
    story += [p("Loan workflow", "H2Guide"), table([
        ["Actor", "Action", "Result"],
        ["Customer", "Applies for a loan with amount and purpose.", "A PENDING Loan record receives an ID."],
        ["Manager", "Views pending IDs and assigns a loan to an active employee.", "Only that employee can decide it."],
        ["Employee", "Approves or rejects an assigned PENDING loan.", "Approval uses a LoanJournal, credits customer balance, commits decision, then adds LOAN_CREDIT to history."],
        ["Customer", "Uses My loan status and Balance/History.", "Sees the decision and credited money after approval."],
    ], [22 * mm, 71 * mm, 68 * mm])]
    story += [p("Quick command reference", "H2Guide"), p("make clean && make<br/>./bank_server data<br/>./bank_client 127.0.0.1", "CodeGuide")]
    story.append(PageBreak())

    story += [p("4. Roles and Menus", "H1Guide")]
    story += [table([
        ["Role", "Main permissions", "Good viva explanation"],
        ["Customer", "Balance, deposit, withdraw, transfer, loan application/status, password, feedback, history.", "A customer can only affect their own account and records."],
        ["Employee", "Open/modify/close customer accounts, view assigned loans, approve/reject, view passbook.", "Loan authorization is constrained by assigned_to, so an employee cannot decide another employee's loan."],
        ["Manager", "Activate/deactivate customers, list pending loans, assign loans, review feedback.", "The manager supervises allocation and account state but does not directly transfer money."],
        ["Administrator", "Create employees, edit user details, change roles, change password.", "The administrator manages identities and permissions, not routine banking operations."],
    ], [27 * mm, 76 * mm, 58 * mm])]
    story += [p("Authorization pattern", "H2Guide"), p("The server never trusts a client menu by itself. After login, it stores the session username and role on the server-side connection. <b>handle_request()</b> selects operations by that role. Each sensitive operation also performs its own checks - for example, loan_decision verifies that the loan is still pending and is assigned to the current employee.")]
    story += [p("Useful demo data", "H2Guide"), table([
        ["Role", "Username", "Default password"],
        ["Administrator", "admin", "admin123"],
        ["Manager", "manager", "manager123"],
        ["Employee", "employee", "employee123"],
        ["Customer", "customer", "customer123"],
    ], [50 * mm, 53 * mm, 58 * mm])]
    story += [p("Important: change the demo passwords before a real demonstration and do not run multiple server instances against the same data folder.", "Small")]
    story.append(PageBreak())

    story += [p("5. Concurrency, ACID and Security", "H1Guide")]
    story += [p("Race condition example", "H2Guide"), p("Without locking, two customer threads can both read balance 1000, both decide that a withdrawal of 800 is allowed, and each write 200. The bank has allowed withdrawal of 1600. In this project, the named semaphore and exclusive file lock make the read-check-write sequence run one at a time for balance-changing operations.")]
    story += [table([
        ["Property", "How the project addresses it", "Viva wording"],
        ["Atomicity", "Prepared journal stores prior values; failed or incomplete critical updates are rolled back.", "A transfer or loan credit is treated as all-or-nothing."],
        ["Consistency", "Validation checks account state, role, positive amount, sufficient balance, assigned pending loan.", "Every operation preserves the banking rules."],
        ["Isolation", "Named semaphore and fcntl locks serialize conflicting updates.", "Concurrent clients cannot interleave a balance update."],
        ["Durability", "fsync is called after mutation and journal transitions.", "Acknowledged changes are flushed to disk."],
    ], [28 * mm, 84 * mm, 49 * mm])]
    story += [p("Security points", "H2Guide"), bullet("Passwords are stored as a salted SHA-256 digest, not printed back to the client."), bullet("The client provides a role at login, but the server compares it with the stored role before opening a session."), bullet("The logged_in flag enforces one session per user; cleanup runs when a client disconnects."), bullet("All inputs are validated for required strings, IDs, role bounds, and safe monetary values before writes.")]
    story += [p("Limitations to acknowledge honestly", "H2Guide"), p("This is an educational local-network simulation, not a production bank. It does not use TLS, a real relational database, multi-factor authentication, encrypted files, interest calculation, or a full crash-safe transaction database. The goal is to demonstrate Linux system-programming concepts in a manageable mini-project.")]
    story.append(PageBreak())

    story += [p("6. Viva Questions and Short Answers", "H1Guide")]
    qa = [
        ("Why use a client-server model?", "It centralizes persistent bank data on the server. Many clients can use the same database without each client editing local files."),
        ("Why use TCP instead of UDP?", "Bank requests need reliable, ordered delivery. TCP provides a connected byte stream, while the code still uses send_all and recv_all to handle partial transfers."),
        ("Why are file locks required if we already use threads?", "pthread synchronization only covers threads in one process. fcntl file locks also coordinate access if another process opens the same data file."),
        ("What is the role of the named semaphore?", "It serializes balance-changing operations so the read-check-write sequence cannot race."),
        ("What happens if a customer tries to log in twice?", "The first login sets logged_in to 1 under a users.dat lock. The second login sees that flag and is rejected."),
        ("How does transfer avoid partial update?", "A prepared journal records old balances before the debit/credit. Both records are changed while locked and fsynced; startup recovery rolls back an unfinished journal."),
        ("How is an approved loan reflected?", "The employee's approval credits the applicant balance, changes the loan status to APPROVED, commits a loan journal, and writes a LOAN_CREDIT transaction."),
        ("How is employee loan access controlled?", "The loan has assigned_to. The server rejects a decision unless assigned_to matches the logged-in employee and the loan is still pending."),
        ("Why use fixed-size structs?", "They make binary file records and the socket protocol easy to read/write at known offsets. The tradeoff is client and server must use compatible struct layouts."),
        ("What would you improve for production?", "TLS, a real DBMS with transactions, stronger password handling such as Argon2, audit authentication, authorization policies, input framing, backups, and monitoring."),
    ]
    for question, answer in qa:
        story += [p(question, "Question"), p(answer, "BodyGuide")]
    story += [Spacer(1, 4 * mm), p("Last-minute viva checklist", "H2Guide"), bullet("Be ready to explain one complete loan flow and one complete transfer flow."), bullet("Mention both pthreads and fcntl locking, then distinguish their scope."), bullet("Use the four ACID words with the concrete code mechanisms above."), bullet("State one limitation clearly; examiners value a realistic answer.")]

    # We use onPage to add the running frame; diagrams are placed through callbacks on their pages.
    doc.build(story, onFirstPage=decorated_page, onLaterPages=decorated_page)


if __name__ == "__main__":
    build()
