// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TEST_FUZZ_UTIL_NET_H
#define BITCOIN_TEST_FUZZ_UTIL_NET_H

#include <netaddress.h>

class FuzzedDataProvider;
class FastRandomContext;

/**
 * Create a CNetAddr. It may have `addr.IsValid() == false`.
 * @param[in,out] fuzzed_data_provider Take data for the address from this, if `rand` is `nullptr`.
 * @param[in,out] rand If not nullptr, take data from it instead of from `fuzzed_data_provider`.
 * Prefer generating addresses using `fuzzed_data_provider` because it is not uniform. Only use
 * `rand` if `fuzzed_data_provider` is exhausted or its data is needed for other things.
 * @return a "random" network address.
 */
CNetAddr ConsumeNetAddr(FuzzedDataProvider& fuzzed_data_provider, FastRandomContext* rand = nullptr) noexcept;

#endif // BITCOIN_TEST_FUZZ_UTIL_NET_H
