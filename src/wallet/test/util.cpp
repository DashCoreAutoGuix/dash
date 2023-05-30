// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/test/util.h>

#include <chain.h>
#include <key.h>
#include <key_io.h>
#include <test/util/setup_common.h>
#include <util/translation.h>
#include <wallet/context.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>

#include <boost/test/unit_test.hpp>

#include <memory>

namespace wallet {
std::unique_ptr<CWallet> CreateSyncedWallet(interfaces::Chain& chain, interfaces::CoinJoin::Loader& coinjoin_loader, CChain& cchain, ArgsManager& args, const CKey& key)
{
    auto wallet = std::make_unique<CWallet>(&chain, &coinjoin_loader, "", args, CreateMockWalletDatabase());
    {
        LOCK(wallet->cs_wallet);
        wallet->SetLastBlockProcessed(cchain.Height(), cchain.Tip()->GetBlockHash());
    }
    wallet->LoadWallet();
    {
        LOCK(wallet->cs_wallet);
        wallet->SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
        wallet->SetupDescriptorScriptPubKeyMans("", "");

        FlatSigningProvider provider;
        std::string error;
        std::unique_ptr<Descriptor> desc = Parse("combo(" + EncodeSecret(key) + ")", provider, error, /* require_checksum=*/ false);
        assert(desc);
        WalletDescriptor w_desc(std::move(desc), 0, 0, 1, 1);
        if (!wallet->AddWalletDescriptor(w_desc, provider, "", false)) assert(false);
    }
    WalletRescanReserver reserver(*wallet);
    reserver.reserve();
    CWallet::ScanResult result = wallet->ScanForWalletTransactions(cchain.Genesis()->GetBlockHash(), /*start_height=*/0, /*max_height=*/{}, reserver, /*fUpdate=*/false, /*save_progress=*/false);
    BOOST_CHECK_EQUAL(result.status, CWallet::ScanResult::SUCCESS);
    BOOST_CHECK_EQUAL(result.last_scanned_block, cchain.Tip()->GetBlockHash());
    BOOST_CHECK_EQUAL(*result.last_scanned_height, cchain.Height());
    BOOST_CHECK(result.last_failed_block.IsNull());
    return wallet;
}

std::shared_ptr<CWallet> TestLoadWallet(std::unique_ptr<WalletDatabase> database, WalletContext& context, uint64_t create_flags)
{
    bilingual_str error;
    std::vector<bilingual_str> warnings;
    auto wallet = CWallet::Create(context, "", std::move(database), create_flags, error, warnings);
    NotifyWalletLoaded(context, wallet);
    if (context.chain) {
        wallet->postInitProcess();
    }
    return wallet;
}

std::shared_ptr<CWallet> TestLoadWallet(WalletContext& context)
{
    DatabaseOptions options;
    options.create_flags = WALLET_FLAG_DESCRIPTORS;
    DatabaseStatus status;
    bilingual_str error;
    std::vector<bilingual_str> warnings;
    auto database = MakeWalletDatabase("", options, status, error);
    return TestLoadWallet(std::move(database), context, options.create_flags);
}

void TestUnloadWallet(std::shared_ptr<CWallet>&& wallet)
{
    SyncWithValidationInterfaceQueue();
    wallet->m_chain_notifications_handler.reset();
    UnloadWallet(std::move(wallet));
}

std::unique_ptr<WalletDatabase> DuplicateMockDatabase(WalletDatabase& database)
{
    // For now, return a new mock database (not a true duplicate)
    return CreateMockWalletDatabase();
}

std::string getnewaddress(CWallet& w)
{
    CTxDestination dest;
    bilingual_str error;
    if (!w.GetNewDestination("", dest, error)) {
        assert(false); // Should not fail
    }
    return EncodeDestination(dest);
}

CTxDestination getNewDestination(CWallet& w, OutputType output_type)
{
    CTxDestination dest;
    bilingual_str error;
    if (!w.GetNewDestination("", dest, error)) {
        assert(false); // Should not fail
    }
    return dest;
}

} // namespace wallet