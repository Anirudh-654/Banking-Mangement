#!/usr/bin/env bash
set -euo pipefail

work_dir=$(mktemp -d)
port=19090
cleanup() {
  kill "$server_pid" 2>/dev/null || true
  wait "$server_pid" 2>/dev/null || true
  rm -rf "$work_dir"
}

./bank_server "$work_dir/data" "$port" >"$work_dir/server.log" 2>&1 &
server_pid=$!
trap cleanup EXIT
sleep 0.4

printf '1\ncustomer\ncustomer123\n2\n50\n5\n250\ncar repair\n7\nHelpful service\n8\n10\n' | ./bank_client 127.0.0.1 "$port" >"$work_dir/customer.out"
printf '3\nmanager\nmanager123\n2\n3\n1\nemployee\n4\n6\n' | ./bank_client 127.0.0.1 "$port" >"$work_dir/manager.out"
printf '2\nemployee\nemployee123\n4\n5\n1\n1\napproved\n1\nnewcustomer\nnewpass\nNew Customer\n200\n6\ncustomer\n8\n' | ./bank_client 127.0.0.1 "$port" >"$work_dir/employee.out"
printf '1\ncustomer\ncustomer123\n9\n1\n4\nnewcustomer\n100\n10\n' | ./bank_client 127.0.0.1 "$port" >"$work_dir/transfer.out"
printf '2\nemployee\nemployee123\n3\nnewcustomer\n8\n' | ./bank_client 127.0.0.1 "$port" >"$work_dir/close.out"
printf '4\nadmin\nadmin123\n1\nnewemployee\nnewpass\nNew Employee\n3\nnewcustomer\n1\n5\n' | ./bank_client 127.0.0.1 "$port" >"$work_dir/admin.out"

grep -q 'Deposit completed' "$work_dir/customer.out"
grep -q 'Loan application #1 submitted' "$work_dir/customer.out"
grep -q 'Feedback recorded' "$work_dir/customer.out"
grep -q 'Loan assigned' "$work_dir/manager.out"
grep -q '#1 applicant=customer' "$work_dir/manager.out"
grep -q 'Helpful service' "$work_dir/manager.out"
grep -q 'Loan approved' "$work_dir/employee.out"
grep -q 'Loan status returned' "$work_dir/transfer.out"
grep -q 'APPROVED' "$work_dir/transfer.out"
grep -q 'Current balance: 1300.00' "$work_dir/transfer.out"
grep -q 'customer account created' "$work_dir/employee.out"
grep -q 'Transfer completed' "$work_dir/transfer.out"
grep -q 'Customer account closed' "$work_dir/close.out"
grep -q 'employee account created' "$work_dir/admin.out"
grep -q 'User details updated' "$work_dir/admin.out"
printf 'Integration test passed.\n'
