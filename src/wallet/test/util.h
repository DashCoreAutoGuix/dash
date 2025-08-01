// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_TEST_UTIL_H
#define BITCOIN_WALLET_TEST_UTIL_H

#include <memory>

class ArgsManager;
class CChain;
class CKey;
namespace interfaces {
class Chain;
namespace CoinJoin {
class Loader;
} // namespace CoinJoin
} // namespace interfaces

namespace wallet {
class CWallet;

static const DatabaseFormat DATABASE_FORMATS[] = {
#ifdef USE_SQLITE
       DatabaseFormat::SQLITE,
#endif
#ifdef USE_BDB
       DatabaseFormat::BERKELEY,
#endif
};

std::unique_ptr<CWallet> CreateSyncedWallet(interfaces::Chain& chain, interfaces::CoinJoin::Loader& coinjoin_loader, CChain& cchain, ArgsManager& args, const CKey& key);

// Creates a copy of the provided database
std::unique_ptr<WalletDatabase> DuplicateMockDatabase(WalletDatabase& database);

} // namespace wallet

#endif // BITCOIN_WALLET_TEST_UTIL_H
