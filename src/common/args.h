// Copyright (c) 2023 The Bitcoin Core developers
// Copyright (c) 2024-2025 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COMMON_ARGS_H
#define BITCOIN_COMMON_ARGS_H

// For compatibility with Bitcoin Core's common/args.h include path.
// In Dash, ArgsManager is still in util/system.h.
// This header provides the expected include path for code expecting common/args.h.

#include <util/system.h>

#endif // BITCOIN_COMMON_ARGS_H
