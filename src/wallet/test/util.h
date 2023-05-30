// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_TEST_UTIL_H
#define BITCOIN_WALLET_TEST_UTIL_H

#include <memory>
#include <outputtype.h>
#include <script/standard.h>

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
class WalletDatabase;
struct WalletContext;

const std::string ADDRESS_BCRT1_UNSPENDABLE = "bcrt1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq3xueyj";

std::unique_ptr<CWallet> CreateSyncedWallet(interfaces::Chain& chain, interfaces::CoinJoin::Loader& coinjoin_loader, CChain& cchain, ArgsManager& args, const CKey& key);

std::shared_ptr<CWallet> TestLoadWallet(WalletContext& context);
std::shared_ptr<CWallet> TestLoadWallet(std::unique_ptr<WalletDatabase> database, WalletContext& context, uint64_t create_flags);
void TestUnloadWallet(std::shared_ptr<CWallet>&& wallet);

// Creates a copy of the provided database
std::unique_ptr<WalletDatabase> DuplicateMockDatabase(WalletDatabase& database);

/** Returns a new encoded destination from the wallet (hardcoded to BECH32) */
std::string getnewaddress(CWallet& w);
/** Returns a new destination, of an specific type, from the wallet */
CTxDestination getNewDestination(CWallet& w, OutputType output_type);

} // namespace wallet

#endif // BITCOIN_WALLET_TEST_UTIL_H