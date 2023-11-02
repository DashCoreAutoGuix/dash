// Copyright (c) 2019-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
#include <chainparams.h>
#include <consensus/validation.h>
#include <evo/evodb.h>
#include <index/txindex.h>
#include <llmq/blockprocessor.h>
#include <llmq/context.h>
#include <node/chainstate.h>
#include <node/utxo_snapshot.h>
#include <random.h>
#include <rpc/blockchain.h>
#include <spork.h>
#include <sync.h>
#include <test/util/chainstate.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <validation.h>
#include <validationinterface.h>

#include <tinyformat.h>

#include <vector>

#include <boost/test/unit_test.hpp>

using node::SnapshotMetadata;

BOOST_FIXTURE_TEST_SUITE(validation_chainstatemanager_tests, ChainTestingSetup)

//! Basic tests for ChainstateManager.
//!
//! First create a legacy (IBD) chainstate, then create a snapshot chainstate.
BOOST_AUTO_TEST_CASE(chainstatemanager)
{
    const Consensus::Params& consensus_params = Params().GetConsensus();
    ChainstateManager& manager = *m_node.chainman;
    CTxMemPool& mempool = *m_node.mempool;
    CEvoDB& evodb = *m_node.evodb;
    std::vector<CChainState*> chainstates;

    BOOST_CHECK(!manager.SnapshotBlockhash().has_value());

    // Create a legacy (IBD) chainstate.
    //
    CChainState& c1 = WITH_LOCK(::cs_main, return manager.InitializeChainstate(&mempool, evodb, m_node.chain_helper));
    chainstates.push_back(&c1);
    c1.InitCoinsDB(
        /*cache_size_bytes=*/1 << 23, /*in_memory=*/true, /*should_wipe=*/false);
    WITH_LOCK(::cs_main, c1.InitCoinsCache(1 << 23));

    DashChainstateSetup(manager, m_node, /*fReset=*/false, /*fReindexChainState=*/false, consensus_params);

    BOOST_CHECK(!manager.IsSnapshotActive());
    BOOST_CHECK(WITH_LOCK(::cs_main, return !manager.IsSnapshotValidated()));
    auto all = manager.GetAll();
    BOOST_CHECK_EQUAL_COLLECTIONS(all.begin(), all.end(), chainstates.begin(), chainstates.end());

    auto& active_chain = manager.ActiveChain();
    BOOST_CHECK_EQUAL(&active_chain, &c1.m_chain);

    BOOST_CHECK_EQUAL(manager.ActiveHeight(), -1);

    auto active_tip = manager.ActiveTip();
    auto exp_tip = c1.m_chain.Tip();
    BOOST_CHECK_EQUAL(active_tip, exp_tip);

    BOOST_CHECK(!manager.SnapshotBlockhash().has_value());

    if (m_node.llmq_ctx) {
        m_node.llmq_ctx->Interrupt();
        m_node.llmq_ctx->Stop();
    }
    DashChainstateSetupClose(m_node);

    // Create a snapshot-based chainstate.
    //
    const uint256 snapshot_blockhash = GetRandHash();
    CChainState& c2 = WITH_LOCK(::cs_main, return manager.InitializeChainstate(
        &mempool, evodb, m_node.chain_helper,
        snapshot_blockhash)
    );
    chainstates.push_back(&c2);

    DashChainstateSetup(manager, m_node, /*fReset=*/false, /*fReindexChainState=*/false, consensus_params);

    BOOST_CHECK_EQUAL(manager.SnapshotBlockhash().value(), snapshot_blockhash);

    c2.InitCoinsDB(
        /*cache_size_bytes=*/1 << 23, /*in_memory=*/true, /*should_wipe=*/false);
    WITH_LOCK(::cs_main, c2.InitCoinsCache(1 << 23));
    // Unlike c1, which doesn't have any blocks. Gets us different tip, height.
    c2.LoadGenesisBlock();
    BlockValidationState dummy_state;
    BOOST_CHECK(c2.ActivateBestChain(dummy_state, nullptr));

    BOOST_CHECK(manager.IsSnapshotActive());
    BOOST_CHECK(WITH_LOCK(::cs_main, return !manager.IsSnapshotValidated()));
    BOOST_CHECK_EQUAL(&c2, &manager.ActiveChainstate());
    BOOST_CHECK(&c1 != &manager.ActiveChainstate());
    auto all2 = manager.GetAll();
    BOOST_CHECK_EQUAL_COLLECTIONS(all2.begin(), all2.end(), chainstates.begin(), chainstates.end());

    auto& active_chain2 = manager.ActiveChain();
    BOOST_CHECK_EQUAL(&active_chain2, &c2.m_chain);

    BOOST_CHECK_EQUAL(manager.ActiveHeight(), 0);

    auto active_tip2 = manager.ActiveTip();
    auto exp_tip2 = c2.m_chain.Tip();
    BOOST_CHECK_EQUAL(active_tip2, exp_tip2);

    // Ensure that these pointers actually correspond to different
    // CCoinsViewCache instances.
    BOOST_CHECK(exp_tip != exp_tip2);

    // Let scheduler events finish running to avoid accessing memory that is going to be unloaded
    SyncWithValidationInterfaceQueue();

    if (m_node.llmq_ctx) {
        m_node.llmq_ctx->Interrupt();
        m_node.llmq_ctx->Stop();
    }
    DashChainstateSetupClose(m_node);
}

//! Test rebalancing the caches associated with each chainstate.
BOOST_AUTO_TEST_CASE(chainstatemanager_rebalance_caches)
{
    ChainstateManager& manager = *m_node.chainman;
    CTxMemPool& mempool = *m_node.mempool;
    CEvoDB& evodb = *m_node.evodb;
    size_t max_cache = 10000;
    manager.m_total_coinsdb_cache = max_cache;
    manager.m_total_coinstip_cache = max_cache;

    std::vector<CChainState*> chainstates;

    // Create a legacy (IBD) chainstate.
    //
    CChainState& c1 = WITH_LOCK(cs_main, return manager.InitializeChainstate(&mempool, evodb, m_node.chain_helper));
    chainstates.push_back(&c1);
    c1.InitCoinsDB(
        /*cache_size_bytes=*/1 << 23, /*in_memory=*/true, /*should_wipe=*/false);

    {
        LOCK(::cs_main);
        c1.InitCoinsCache(1 << 23);
        BOOST_REQUIRE(c1.LoadGenesisBlock());
        c1.CoinsTip().SetBestBlock(InsecureRand256());
        manager.MaybeRebalanceCaches();
    }

    BOOST_CHECK_EQUAL(c1.m_coinstip_cache_size_bytes, max_cache);
    BOOST_CHECK_EQUAL(c1.m_coinsdb_cache_size_bytes, max_cache);

    // Create a snapshot-based chainstate.
    //
    CChainState& c2 = WITH_LOCK(cs_main, return manager.InitializeChainstate(&mempool, evodb, m_node.chain_helper, GetRandHash()));
    chainstates.push_back(&c2);
    c2.InitCoinsDB(
        /*cache_size_bytes=*/1 << 23, /*in_memory=*/true, /*should_wipe=*/false);

    {
        LOCK(::cs_main);
        c2.InitCoinsCache(1 << 23);
        BOOST_REQUIRE(c2.LoadGenesisBlock());
        c2.CoinsTip().SetBestBlock(InsecureRand256());
        manager.MaybeRebalanceCaches();
    }

    // Since both chainstates are considered to be in initial block download,
    // the snapshot chainstate should take priority.
    BOOST_CHECK_CLOSE(c1.m_coinstip_cache_size_bytes, max_cache * 0.05, 1);
    BOOST_CHECK_CLOSE(c1.m_coinsdb_cache_size_bytes, max_cache * 0.05, 1);
    BOOST_CHECK_CLOSE(c2.m_coinstip_cache_size_bytes, max_cache * 0.95, 1);
    BOOST_CHECK_CLOSE(c2.m_coinsdb_cache_size_bytes, max_cache * 0.95, 1);
}

//! Test basic snapshot activation.
BOOST_FIXTURE_TEST_CASE(chainstatemanager_activate_snapshot, TestChain100Setup)
{
    ChainstateManager& chainman = *Assert(m_node.chainman);

    size_t initial_size;
    size_t initial_total_coins{100};

    // Make some initial assertions about the contents of the chainstate.
    {
        LOCK(::cs_main);
        CCoinsViewCache& ibd_coinscache = chainman.ActiveChainstate().CoinsTip();
        initial_size = ibd_coinscache.GetCacheSize();
        size_t total_coins{0};

        for (CTransactionRef& txn : m_coinbase_txns) {
            COutPoint op{txn->GetHash(), 0};
            BOOST_CHECK(ibd_coinscache.HaveCoin(op));
            total_coins++;
        }

        BOOST_CHECK_EQUAL(total_coins, initial_total_coins);
        BOOST_CHECK_EQUAL(initial_size, initial_total_coins);
    }

    // Snapshot should refuse to load at this height.
    BOOST_REQUIRE(!CreateAndActivateUTXOSnapshot(m_node, m_path_root));
    BOOST_CHECK(!chainman.ActiveChainstate().m_from_snapshot_blockhash);
    BOOST_CHECK(!chainman.SnapshotBlockhash());

    // Mine 10 more blocks, putting at us height 110 where a valid assumeutxo value can
    // be found.
    constexpr int snapshot_height = 110;
    mineBlocks(10);
    initial_size += 10;
    initial_total_coins += 10;

    // Should not load malleated snapshots
    BOOST_REQUIRE(!CreateAndActivateUTXOSnapshot(
        m_node, m_path_root, [](CAutoFile& auto_infile, SnapshotMetadata& metadata) {
            // A UTXO is missing but count is correct
            metadata.m_coins_count -= 1;

            COutPoint outpoint;
            Coin coin;

            auto_infile >> outpoint;
            auto_infile >> coin;
    }));
    BOOST_REQUIRE(!CreateAndActivateUTXOSnapshot(
        m_node, m_path_root, [](CAutoFile& auto_infile, SnapshotMetadata& metadata) {
            // Coins count is larger than coins in file
            metadata.m_coins_count += 1;
    }));
    BOOST_REQUIRE(!CreateAndActivateUTXOSnapshot(
        m_node, m_path_root, [](CAutoFile& auto_infile, SnapshotMetadata& metadata) {
            // Coins count is smaller than coins in file
            metadata.m_coins_count -= 1;
    }));
    BOOST_REQUIRE(!CreateAndActivateUTXOSnapshot(
        m_node, m_path_root, [](CAutoFile& auto_infile, SnapshotMetadata& metadata) {
            // Wrong hash
            metadata.m_base_blockhash = uint256::ZERO;
    }));
    BOOST_REQUIRE(!CreateAndActivateUTXOSnapshot(
        m_node, m_path_root, [](CAutoFile& auto_infile, SnapshotMetadata& metadata) {
            // Wrong hash
            metadata.m_base_blockhash = uint256::ONE;
    }));

    BOOST_REQUIRE(CreateAndActivateUTXOSnapshot(m_node, m_path_root));

    // Ensure our active chain is the snapshot chainstate.
    BOOST_CHECK(!chainman.ActiveChainstate().m_from_snapshot_blockhash->IsNull());
    BOOST_CHECK_EQUAL(
        *chainman.ActiveChainstate().m_from_snapshot_blockhash,
        *chainman.SnapshotBlockhash());

    // Ensure that the genesis block was not marked assumed-valid.
    BOOST_CHECK(WITH_LOCK(::cs_main, return !chainman.ActiveChain().Genesis()->IsAssumedValid()));

    const AssumeutxoData& au_data = *ExpectedAssumeutxo(snapshot_height, ::Params());
    const CBlockIndex* tip = chainman.ActiveTip();

    BOOST_CHECK_EQUAL(tip->nChainTx, au_data.nChainTx);

    // To be checked against later when we try loading a subsequent snapshot.
    uint256 loaded_snapshot_blockhash{*chainman.SnapshotBlockhash()};

    // Make some assertions about the both chainstates. These checks ensure the
    // legacy chainstate hasn't changed and that the newly created chainstate
    // reflects the expected content.
    {
        LOCK(::cs_main);
        int chains_tested{0};

        for (CChainState* chainstate : chainman.GetAll()) {
            BOOST_TEST_MESSAGE("Checking coins in " << chainstate->ToString());
            CCoinsViewCache& coinscache = chainstate->CoinsTip();

            // Both caches will be empty initially.
            BOOST_CHECK_EQUAL((unsigned int)0, coinscache.GetCacheSize());

            size_t total_coins{0};

            for (CTransactionRef& txn : m_coinbase_txns) {
                COutPoint op{txn->GetHash(), 0};
                BOOST_CHECK(coinscache.HaveCoin(op));
                total_coins++;
            }

            BOOST_CHECK_EQUAL(initial_size , coinscache.GetCacheSize());
            BOOST_CHECK_EQUAL(total_coins, initial_total_coins);
            chains_tested++;
        }

        BOOST_CHECK_EQUAL(chains_tested, 2);
    }

    // Mine some new blocks on top of the activated snapshot chainstate.
    constexpr size_t new_coins{100};
    mineBlocks(new_coins);  // Defined in TestChain100Setup.

    {
        LOCK(::cs_main);
        size_t coins_in_active{0};
        size_t coins_in_background{0};
        size_t coins_missing_from_background{0};

        for (CChainState* chainstate : chainman.GetAll()) {
            BOOST_TEST_MESSAGE("Checking coins in " << chainstate->ToString());
            CCoinsViewCache& coinscache = chainstate->CoinsTip();
            bool is_background = chainstate != &chainman.ActiveChainstate();

            for (CTransactionRef& txn : m_coinbase_txns) {
                COutPoint op{txn->GetHash(), 0};
                if (coinscache.HaveCoin(op)) {
                    (is_background ? coins_in_background : coins_in_active)++;
                } else if (is_background) {
                    coins_missing_from_background++;
                }
            }
        }

        BOOST_CHECK_EQUAL(coins_in_active, initial_total_coins + new_coins);
        BOOST_CHECK_EQUAL(coins_in_background, initial_total_coins);
        BOOST_CHECK_EQUAL(coins_missing_from_background, new_coins);
    }

    // Snapshot should refuse to load after one has already loaded.
    BOOST_REQUIRE(!CreateAndActivateUTXOSnapshot(m_node, m_path_root));

    // Snapshot blockhash should be unchanged.
    BOOST_CHECK_EQUAL(
        *chainman.ActiveChainstate().m_from_snapshot_blockhash,
        loaded_snapshot_blockhash);
}

//! Test LoadBlockIndex behavior when multiple chainstates are in use.
//!
//! - First, verfiy that setBlockIndexCandidates is as expected when using a single,
//!   fully-validating chainstate.
//!
//! - Then mark a region of the chain BLOCK_ASSUMED_VALID and introduce a second chainstate
//!   that will tolerate assumed-valid blocks. Run LoadBlockIndex() and ensure that the first
//!   chainstate only contains fully validated blocks and the other chainstate contains all blocks,
//!   even those assumed-valid.
//!
BOOST_FIXTURE_TEST_CASE(chainstatemanager_loadblockindex, TestChain100Setup)
{
    ChainstateManager& chainman = *Assert(m_node.chainman);
    CTxMemPool& mempool = *m_node.mempool;
    CChainState& cs1 = chainman.ActiveChainstate();

    int num_indexes{0};
    int num_assumed_valid{0};
    const int expected_assumed_valid{20};
    const int last_assumed_valid_idx{40};
    const int assumed_valid_start_idx = last_assumed_valid_idx - expected_assumed_valid;

    CBlockIndex* validated_tip{nullptr};
    CBlockIndex* assumed_tip{chainman.ActiveChain().Tip()};

    auto reload_all_block_indexes = [&]() {
        for (CChainState* cs : chainman.GetAll()) {
            LOCK(::cs_main);
            cs->UnloadBlockIndex();
            BOOST_CHECK(cs->setBlockIndexCandidates.empty());
        }

        WITH_LOCK(::cs_main, chainman.LoadBlockIndex());
    };

    // Ensure that without any assumed-valid BlockIndex entries, all entries are considered
    // tip candidates.
    reload_all_block_indexes();
    BOOST_CHECK_EQUAL(cs1.setBlockIndexCandidates.size(), cs1.m_chain.Height() + 1);

    // Mark some region of the chain assumed-valid.
    for (int i = 0; i <= cs1.m_chain.Height(); ++i) {
        LOCK(::cs_main);
        auto index = cs1.m_chain[i];

        if (i < last_assumed_valid_idx && i >= assumed_valid_start_idx) {
            index->nStatus = BlockStatus::BLOCK_VALID_TREE | BlockStatus::BLOCK_ASSUMED_VALID;
        }

        ++num_indexes;
        if (index->IsAssumedValid()) ++num_assumed_valid;

        // Note the last fully-validated block as the expected validated tip.
        if (i == (assumed_valid_start_idx - 1)) {
            validated_tip = index;
            BOOST_CHECK(!index->IsAssumedValid());
        }
    }

    BOOST_CHECK_EQUAL(expected_assumed_valid, num_assumed_valid);

    CChainState& cs2 = WITH_LOCK(::cs_main,
        return chainman.InitializeChainstate(&mempool, *m_node.evodb, m_node.chain_helper, GetRandHash()));

    reload_all_block_indexes();

    // The fully validated chain only has candidates up to the start of the assumed-valid
    // blocks.
    BOOST_CHECK_EQUAL(cs1.setBlockIndexCandidates.count(validated_tip), 1);
    BOOST_CHECK_EQUAL(cs1.setBlockIndexCandidates.count(assumed_tip), 0);
    BOOST_CHECK_EQUAL(cs1.setBlockIndexCandidates.size(), assumed_valid_start_idx);

    // The assumed-valid tolerant chain has all blocks as candidates.
    BOOST_CHECK_EQUAL(cs2.setBlockIndexCandidates.count(validated_tip), 1);
    BOOST_CHECK_EQUAL(cs2.setBlockIndexCandidates.count(assumed_tip), 1);
    BOOST_CHECK_EQUAL(cs2.setBlockIndexCandidates.size(), num_indexes);
}

//! Ensure that snapshot chainstates initialize properly when found on disk.
BOOST_FIXTURE_TEST_CASE(chainstatemanager_snapshot_init, SnapshotTestSetup)
{
    ChainstateManager& chainman = *Assert(m_node.chainman);
    Chainstate& bg_chainstate = chainman.ActiveChainstate();

    this->SetupSnapshot();

    fs::path snapshot_chainstate_dir = *node::FindSnapshotChainstateDir(chainman.m_options.datadir);
    BOOST_CHECK(fs::exists(snapshot_chainstate_dir));
    BOOST_CHECK_EQUAL(snapshot_chainstate_dir, gArgs.GetDataDirNet() / "chainstate_snapshot");

    BOOST_CHECK(chainman.IsSnapshotActive());
    const uint256 snapshot_tip_hash = WITH_LOCK(chainman.GetMutex(),
        return chainman.ActiveTip()->GetBlockHash());

    auto all_chainstates = chainman.GetAll();
    BOOST_CHECK_EQUAL(all_chainstates.size(), 2);

    // "Rewind" the background chainstate so that its tip is not at the
    // base block of the snapshot - this is so after simulating a node restart,
    // it will initialize instead of attempting to complete validation.
    //
    // Note that this is not a realistic use of DisconnectTip().
    DisconnectedBlockTransactions unused_pool{MAX_DISCONNECTED_TX_POOL_BYTES};
    BlockValidationState unused_state;
    {
        LOCK2(::cs_main, bg_chainstate.MempoolMutex());
        BOOST_CHECK(bg_chainstate.DisconnectTip(unused_state, &unused_pool));
        unused_pool.clear();  // to avoid queuedTx assertion errors on teardown
    }
    BOOST_CHECK_EQUAL(bg_chainstate.m_chain.Height(), 109);

    // Test that simulating a shutdown (resetting ChainstateManager) and then performing
    // chainstate reinitializing successfully cleans up the background-validation
    // chainstate data, and we end up with a single chainstate that is at tip.
    ChainstateManager& chainman_restarted = this->SimulateNodeRestart();

    BOOST_TEST_MESSAGE("Performing Load/Verify/Activate of chainstate");

    // This call reinitializes the chainstates.
    this->LoadVerifyActivateChainstate();

    {
        LOCK(chainman_restarted.GetMutex());
        BOOST_CHECK_EQUAL(chainman_restarted.GetAll().size(), 2);
        BOOST_CHECK(chainman_restarted.IsSnapshotActive());
        BOOST_CHECK(!chainman_restarted.IsSnapshotValidated());

        BOOST_CHECK_EQUAL(chainman_restarted.ActiveTip()->GetBlockHash(), snapshot_tip_hash);
        BOOST_CHECK_EQUAL(chainman_restarted.ActiveHeight(), 210);
    }

    BOOST_TEST_MESSAGE(
        "Ensure we can mine blocks on top of the initialized snapshot chainstate");
    mineBlocks(10);
    {
        LOCK(chainman_restarted.GetMutex());
        BOOST_CHECK_EQUAL(chainman_restarted.ActiveHeight(), 220);

        // Background chainstate should be unaware of new blocks on the snapshot
        // chainstate.
        for (Chainstate* cs : chainman_restarted.GetAll()) {
            if (cs != &chainman_restarted.ActiveChainstate()) {
                BOOST_CHECK_EQUAL(cs->m_chain.Height(), 109);
            }
        }
    }
}

BOOST_FIXTURE_TEST_CASE(chainstatemanager_snapshot_completion, SnapshotTestSetup)
{
    this->SetupSnapshot();

    ChainstateManager& chainman = *Assert(m_node.chainman);
    Chainstate& active_cs = chainman.ActiveChainstate();
    auto tip_cache_before_complete = active_cs.m_coinstip_cache_size_bytes;
    auto db_cache_before_complete = active_cs.m_coinsdb_cache_size_bytes;

    SnapshotCompletionResult res;
    m_node.notifications->m_shutdown_on_fatal_error = false;

    fs::path snapshot_chainstate_dir = *node::FindSnapshotChainstateDir(chainman.m_options.datadir);
    BOOST_CHECK(fs::exists(snapshot_chainstate_dir));
    BOOST_CHECK_EQUAL(snapshot_chainstate_dir, gArgs.GetDataDirNet() / "chainstate_snapshot");

    BOOST_CHECK(chainman.IsSnapshotActive());
    const uint256 snapshot_tip_hash = WITH_LOCK(chainman.GetMutex(),
        return chainman.ActiveTip()->GetBlockHash());

    res = WITH_LOCK(::cs_main, return chainman.MaybeCompleteSnapshotValidation());
    BOOST_CHECK_EQUAL(res, SnapshotCompletionResult::SUCCESS);

    WITH_LOCK(::cs_main, BOOST_CHECK(chainman.IsSnapshotValidated()));
    BOOST_CHECK(chainman.IsSnapshotActive());

    // Cache should have been rebalanced and reallocated to the "only" remaining
    // chainstate.
    BOOST_CHECK(active_cs.m_coinstip_cache_size_bytes > tip_cache_before_complete);
    BOOST_CHECK(active_cs.m_coinsdb_cache_size_bytes > db_cache_before_complete);

    auto all_chainstates = chainman.GetAll();
    BOOST_CHECK_EQUAL(all_chainstates.size(), 1);
    BOOST_CHECK_EQUAL(all_chainstates[0], &active_cs);

    // Trying completion again should return false.
    res = WITH_LOCK(::cs_main, return chainman.MaybeCompleteSnapshotValidation());
    BOOST_CHECK_EQUAL(res, SnapshotCompletionResult::SKIPPED);

    // The invalid snapshot path should not have been used.
    fs::path snapshot_invalid_dir = gArgs.GetDataDirNet() / "chainstate_snapshot_INVALID";
    BOOST_CHECK(!fs::exists(snapshot_invalid_dir));
    // chainstate_snapshot should still exist.
    BOOST_CHECK(fs::exists(snapshot_chainstate_dir));

    // Test that simulating a shutdown (resetting ChainstateManager) and then performing
    // chainstate reinitializing successfully cleans up the background-validation
    // chainstate data, and we end up with a single chainstate that is at tip.
    ChainstateManager& chainman_restarted = this->SimulateNodeRestart();

    BOOST_TEST_MESSAGE("Performing Load/Verify/Activate of chainstate");

    // This call reinitializes the chainstates, and should clean up the now unnecessary
    // background-validation leveldb contents.
    this->LoadVerifyActivateChainstate();

    BOOST_CHECK(!fs::exists(snapshot_invalid_dir));
    // chainstate_snapshot should now *not* exist.
    BOOST_CHECK(!fs::exists(snapshot_chainstate_dir));

    const Chainstate& active_cs2 = chainman_restarted.ActiveChainstate();

    {
        LOCK(chainman_restarted.GetMutex());
        BOOST_CHECK_EQUAL(chainman_restarted.GetAll().size(), 1);
        BOOST_CHECK(!chainman_restarted.IsSnapshotActive());
        BOOST_CHECK(!chainman_restarted.IsSnapshotValidated());
        BOOST_CHECK(active_cs2.m_coinstip_cache_size_bytes > tip_cache_before_complete);
        BOOST_CHECK(active_cs2.m_coinsdb_cache_size_bytes > db_cache_before_complete);

        BOOST_CHECK_EQUAL(chainman_restarted.ActiveTip()->GetBlockHash(), snapshot_tip_hash);
        BOOST_CHECK_EQUAL(chainman_restarted.ActiveHeight(), 210);
    }

    BOOST_TEST_MESSAGE(
        "Ensure we can mine blocks on top of the \"new\" IBD chainstate");
    mineBlocks(10);
    {
        LOCK(chainman_restarted.GetMutex());
        BOOST_CHECK_EQUAL(chainman_restarted.ActiveHeight(), 220);
    }
}

BOOST_FIXTURE_TEST_CASE(chainstatemanager_snapshot_completion_hash_mismatch, SnapshotTestSetup)
{
    auto chainstates = this->SetupSnapshot();
    Chainstate& validation_chainstate = *std::get<0>(chainstates);
    ChainstateManager& chainman = *Assert(m_node.chainman);
    SnapshotCompletionResult res;
    m_node.notifications->m_shutdown_on_fatal_error = false;

    // Test tampering with the IBD UTXO set with an extra coin to ensure it causes
    // snapshot completion to fail.
    CCoinsViewCache& ibd_coins = WITH_LOCK(::cs_main,
        return validation_chainstate.CoinsTip());
    Coin badcoin;
    badcoin.out.nValue = InsecureRand32();
    badcoin.nHeight = 1;
    badcoin.out.scriptPubKey.assign(InsecureRandBits(6), 0);
    uint256 txid = InsecureRand256();
    ibd_coins.AddCoin(COutPoint(txid, 0), std::move(badcoin), false);

    fs::path snapshot_chainstate_dir = gArgs.GetDataDirNet() / "chainstate_snapshot";
    BOOST_CHECK(fs::exists(snapshot_chainstate_dir));

    {
        ASSERT_DEBUG_LOG("failed to validate the -assumeutxo snapshot state");
        res = WITH_LOCK(::cs_main, return chainman.MaybeCompleteSnapshotValidation());
        BOOST_CHECK_EQUAL(res, SnapshotCompletionResult::HASH_MISMATCH);
    }

    auto all_chainstates = chainman.GetAll();
    BOOST_CHECK_EQUAL(all_chainstates.size(), 1);
    BOOST_CHECK_EQUAL(all_chainstates[0], &validation_chainstate);
    BOOST_CHECK_EQUAL(&chainman.ActiveChainstate(), &validation_chainstate);

    fs::path snapshot_invalid_dir = gArgs.GetDataDirNet() / "chainstate_snapshot_INVALID";
    BOOST_CHECK(fs::exists(snapshot_invalid_dir));

    // Test that simulating a shutdown (resetting ChainstateManager) and then performing
    // chainstate reinitializing successfully loads only the fully-validated
    // chainstate data, and we end up with a single chainstate that is at tip.
    ChainstateManager& chainman_restarted = this->SimulateNodeRestart();

    BOOST_TEST_MESSAGE("Performing Load/Verify/Activate of chainstate");

    // This call reinitializes the chainstates, and should clean up the now unnecessary
    // background-validation leveldb contents.
    this->LoadVerifyActivateChainstate();

    BOOST_CHECK(fs::exists(snapshot_invalid_dir));
    BOOST_CHECK(!fs::exists(snapshot_chainstate_dir));

    {
        LOCK(::cs_main);
        BOOST_CHECK_EQUAL(chainman_restarted.GetAll().size(), 1);
        BOOST_CHECK(!chainman_restarted.IsSnapshotActive());
        BOOST_CHECK(!chainman_restarted.IsSnapshotValidated());
        BOOST_CHECK_EQUAL(chainman_restarted.ActiveHeight(), 210);
    }

    BOOST_TEST_MESSAGE(
        "Ensure we can mine blocks on top of the \"new\" IBD chainstate");
    mineBlocks(10);
    {
        LOCK(::cs_main);
        BOOST_CHECK_EQUAL(chainman_restarted.ActiveHeight(), 220);
    }
>>>>>>> 023418a140 (Merge bitcoin/bitcoin#28530: tests, bug fix: DisconnectedBlockTransactions rewrite followups)
}

BOOST_AUTO_TEST_SUITE_END()
