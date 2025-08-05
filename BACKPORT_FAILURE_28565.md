# Backport Failure: Bitcoin PR #28565

## Issue
Bitcoin PR #28565 "rpc: getaddrmaninfo followups" cannot be backported to Dash because it modifies RPCs that don't exist in Dash yet.

## Details
This PR makes followup changes to:
- `getaddrmaninfo` RPC (added in Bitcoin PR #27511)
- `getrawaddrman` RPC (added in Bitcoin PR #28523)

These RPCs have not been backported to Dash yet.

## Required Prerequisites
Before this PR can be backported, the following must be backported first:
1. Bitcoin PR #27511 - "rpc: Add test-only RPC getaddrmaninfo for new/tried table address count"
2. Bitcoin PR #28523 - "rpc: add hidden getrawaddrman RPC to list addrman table entries"

## Recommendation
Skip this PR for now and add the prerequisite PRs to the backporting queue.