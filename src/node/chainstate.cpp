// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/chainstate.h>

#include <chain.h>
#include <coins.h>
#include <chainparamsbase.h>
#include <consensus/params.h>
#include <deploymentstatus.h>
#include <node/blockstorage.h>
#include <node/caches.h>
#include <sync.h>
#include <threadsafety.h>
#include <tinyformat.h>
#include <txdb.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/translation.h>
#include <validation.h>

#include <bls/bls.h>
#include <evo/chainhelper.h>
#include <evo/creditpool.h>
#include <evo/deterministicmns.h>
#include <evo/evodb.h>
#include <evo/mnhftx.h>
#include <gsl/pointers.h>
#include <llmq/context.h>

#include <atomic>
#include <cassert>
#include <memory>
#include <vector>

namespace node {
ChainstateLoadResult LoadChainstate(ChainstateManager& chainman,
                                    const CacheSizes& cache_sizes,
                                    const ChainstateLoadOptions& options,
                                    CGovernanceManager& govman,
                                    CMasternodeMetaMan& mn_metaman,
                                    CMasternodeSync& mn_sync,
                                    CSporkManager& sporkman,
                                    std::unique_ptr<CActiveMasternodeManager>& mn_activeman,
                                    std::unique_ptr<CChainstateHelper>& chain_helper,
                                    std::unique_ptr<CCreditPoolManager>& cpoolman,
                                    std::unique_ptr<CDeterministicMNManager>& dmnman,
                                    std::unique_ptr<CEvoDB>& evodb,
                                    std::unique_ptr<CMNHFManager>& mnhf_manager,
                                    std::unique_ptr<LLMQContext>& llmq_ctx,
                                    const fs::path& data_dir,
                                    bool is_addrindex_enabled,
                                    bool is_governance_enabled,
                                    bool is_spentindex_enabled,
                                    bool is_timeindex_enabled,
                                    bool is_txindex_enabled,
                                    const Consensus::Params& consensus_params,
                                    const std::string& network_id,
                                    bool dash_dbs_in_memory)
{
    auto is_coinsview_empty = [&](CChainState* chainstate) EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return options.reindex || options.reindex_chainstate || chainstate->CoinsTip().GetBestBlock().IsNull();
    };

    LOCK(cs_main);

    evodb.reset();
    evodb = std::make_unique<CEvoDB>(util::DbWrapperParams{.path = data_dir, .memory = dash_dbs_in_memory, .wipe = options.reindex || options.reindex_chainstate});

    mnhf_manager.reset();
    mnhf_manager = std::make_unique<CMNHFManager>(*evodb);

    chainman.InitializeChainstate(options.mempool, *evodb, chain_helper);
    chainman.m_total_coinstip_cache = cache_sizes.coins;
    chainman.m_total_coinsdb_cache = cache_sizes.coins_db;

    auto& pblocktree{chainman.m_blockman.m_block_tree_db};
    // new CBlockTreeDB tries to delete the existing file, which
    // fails if it's still open from the previous loop. Close it first:
    pblocktree.reset();
    pblocktree.reset(new CBlockTreeDB(cache_sizes.block_tree_db, options.block_tree_db_in_memory, options.reindex));

    DashChainstateSetup(chainman, govman, mn_metaman, mn_sync, sporkman, mn_activeman, chain_helper, cpoolman,
                        dmnman, evodb, mnhf_manager, llmq_ctx, options.mempool, data_dir, dash_dbs_in_memory,
                        /*llmq_dbs_wipe=*/options.reindex || options.reindex_chainstate, consensus_params);

    if (options.reindex) {
        pblocktree->WriteReindexing(true);
        //If we're reindexing in prune mode, wipe away unusable block files and all undo data files
        if (options.prune) {
            CleanupBlockRevFiles();
        }
    }

    if (options.check_interrupt && options.check_interrupt()) return {ChainstateLoadStatus::INTERRUPTED, {}};

    // LoadBlockIndex will load m_have_pruned if we've ever removed a
    // block file from disk.
    // Note that it also sets fReindex global based on the disk flag!
    // From here on, fReindex and options.reindex values may be different!
    if (!chainman.LoadBlockIndex()) {
        if (options.check_interrupt && options.check_interrupt()) return {ChainstateLoadStatus::INTERRUPTED, {}};
        return {ChainstateLoadStatus::FAILURE, _("Error loading block database")};
    }

    // TODO: Remove this when pruning is fixed.
    // See https://github.com/dashpay/dash/pull/1817 and https://github.com/dashpay/dash/pull/1743
    if (is_governance_enabled && !is_txindex_enabled && network_id != CBaseChainParams::REGTEST) {
        return {ChainstateLoadStatus::FAILURE, _("Transaction index can't be disabled with governance validation enabled. Either start with -disablegovernance command line switch or enable transaction index.")};
    }

    if (!chainman.BlockIndex().empty() &&
            !chainman.m_blockman.LookupBlockIndex(consensus_params.hashGenesisBlock)) {
        // If the loaded chain has a wrong genesis, bail out immediately
        // (we're likely using a testnet datadir, or the other way around).
        return {ChainstateLoadStatus::FAILURE_INCOMPATIBLE_DB, _("Incorrect or no genesis block found. Wrong datadir for network?")};
    }

    if (!consensus_params.hashDevnetGenesisBlock.IsNull() && !chainman.BlockIndex().empty() &&
            !chainman.m_blockman.LookupBlockIndex(consensus_params.hashDevnetGenesisBlock)) {
        return {ChainstateLoadStatus::FAILURE_INCOMPATIBLE_DB, _("Incorrect or no devnet genesis block found. Wrong datadir for devnet specified?")};
    }

    if (!options.reindex && !options.reindex_chainstate) {
        // Check for changed -addressindex state
        if (!fAddressIndex && fAddressIndex != is_addrindex_enabled) {
            return {ChainstateLoadStatus::FAILURE, _("You need to rebuild the database using -reindex to enable -addressindex")};
        }

        // Check for changed -timestampindex state
        if (!fTimestampIndex && fTimestampIndex != is_timeindex_enabled) {
            return {ChainstateLoadStatus::FAILURE, _("You need to rebuild the database using -reindex to enable -timestampindex")};
        }

        // Check for changed -spentindex state
        if (!fSpentIndex && fSpentIndex != is_spentindex_enabled) {
            return {ChainstateLoadStatus::FAILURE, _("You need to rebuild the database using -reindex to enable -spentindex")};
        }
    }

    chainman.InitAdditionalIndexes();

    // Check for changed -prune state.  What we are concerned about is a user who has pruned blocks
    // in the past, but is now trying to run unpruned.
    if (chainman.m_blockman.m_have_pruned && !options.prune) {
        return {ChainstateLoadStatus::FAILURE, _("You need to rebuild the database using -reindex to go back to unpruned mode.  This will redownload the entire blockchain")};
    }

    // At this point blocktree args are consistent with what's on disk.
    // If we're not mid-reindex (based on disk + args), add a genesis block on disk
    // (otherwise we use the one already on disk).
    // This is called again in ThreadImport after the reindex completes.
    if (!fReindex && !chainman.ActiveChainstate().LoadGenesisBlock()) {
        return {ChainstateLoadStatus::FAILURE, _("Error initializing block database")};
    }

    // At this point we're either in reindex or we've loaded a useful
    // block tree into BlockIndex()!

    for (CChainState* chainstate : chainman.GetAll()) {
        chainstate->InitCoinsDB(
            /*cache_size_bytes=*/cache_sizes.coins_db,
            /*in_memory=*/options.coins_db_in_memory,
            /*should_wipe=*/options.reindex || options.reindex_chainstate);

        if (options.coins_error_cb) {
            chainstate->CoinsErrorCatcher().AddReadErrCallback(options.coins_error_cb);
        }

        // Refuse to load unsupported database format.
        // This is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
        if (chainstate->CoinsDB().NeedsUpgrade()) {
            return {ChainstateLoadStatus::FAILURE_INCOMPATIBLE_DB, _("Unsupported chainstate database format found. "
                                                                     "Please restart with -reindex-chainstate. This will "
                                                                     "rebuild the chainstate database.")};
        }

        // ReplayBlocks is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
        if (!chainstate->ReplayBlocks()) {
            return {ChainstateLoadStatus::FAILURE, _("Unable to replay blocks. You will need to rebuild the database using -reindex-chainstate.")};
        }

        // The on-disk coinsdb is now in a good state, create the cache
        chainstate->InitCoinsCache(cache_sizes.coins);
        assert(chainstate->CanFlushToDisk());

        // flush evodb
        // TODO: CEvoDB instance should probably be a part of CChainState
        // (for multiple chainstates to actually work in parallel)
        // and not a global
        if (&chainman.ActiveChainstate() == chainstate && !evodb->CommitRootTransaction()) {
            return {ChainstateLoadStatus::FAILURE, _("Failed to commit Evo database")};
        }

        if (!is_coinsview_empty(chainstate)) {
            // LoadChainTip initializes the chain based on CoinsTip()'s best block
            if (!chainstate->LoadChainTip()) {
                return {ChainstateLoadStatus::FAILURE, _("Error initializing block database")};
            }
            assert(chainstate->m_chain.Tip() != nullptr);
        }
    }

    if (!mnhf_manager->ForceSignalDBUpdate()) {
        return {ChainstateLoadStatus::FAILURE, _("Error upgrading evo database for EHF")};
    }

    // Check if nVersion-first migration is needed and perform it
    if (dmnman->IsMigrationRequired() && !dmnman->MigrateLegacyDiffs(chainman.ActiveChainstate().m_chain.Tip())) {
        return {ChainstateLoadStatus::FAILURE, _("Failed to upgrade Evo database")};
    }

    return {ChainstateLoadStatus::SUCCESS, {}};
}

void DashChainstateSetup(ChainstateManager& chainman,
                         CGovernanceManager& govman,
                         CMasternodeMetaMan& mn_metaman,
                         CMasternodeSync& mn_sync,
                         CSporkManager& sporkman,
                         std::unique_ptr<CActiveMasternodeManager>& mn_activeman,
                         std::unique_ptr<CChainstateHelper>& chain_helper,
                         std::unique_ptr<CCreditPoolManager>& cpoolman,
                         std::unique_ptr<CDeterministicMNManager>& dmnman,
                         std::unique_ptr<CEvoDB>& evodb,
                         std::unique_ptr<CMNHFManager>& mnhf_manager,
                         std::unique_ptr<LLMQContext>& llmq_ctx,
                         CTxMemPool* mempool,
                         const fs::path& data_dir,
                         bool llmq_dbs_in_memory,
                         bool llmq_dbs_wipe,
                         const Consensus::Params& consensus_params)
{
    // Same logic as pblocktree
    dmnman.reset();
    dmnman = std::make_unique<CDeterministicMNManager>(*evodb, mn_metaman);

    cpoolman.reset();
    cpoolman = std::make_unique<CCreditPoolManager>(*evodb);

    if (llmq_ctx) {
        llmq_ctx->Interrupt();
        llmq_ctx->Stop();
    }
    llmq_ctx.reset();
    llmq_ctx = std::make_unique<LLMQContext>(chainman, *dmnman, *evodb, mn_metaman, *mnhf_manager, sporkman,
                                             *mempool, mn_activeman.get(), mn_sync,
                                             util::DbWrapperParams{.path = data_dir, .memory = llmq_dbs_in_memory, .wipe = llmq_dbs_wipe});
    mempool->ConnectManagers(dmnman.get(), llmq_ctx->isman.get());
    // Enable CMNHFManager::{Process, Undo}Block
    mnhf_manager->ConnectManagers(&chainman, llmq_ctx->qman.get());

    chain_helper.reset();
    chain_helper = std::make_unique<CChainstateHelper>(*cpoolman, *dmnman, *mnhf_manager, govman, *(llmq_ctx->isman), *(llmq_ctx->quorum_block_processor),
                                                       *(llmq_ctx->qsnapman), chainman, consensus_params, mn_sync, sporkman, *(llmq_ctx->clhandler),
                                                       *(llmq_ctx->qman));
}

void DashChainstateSetupClose(std::unique_ptr<CChainstateHelper>& chain_helper,
                              std::unique_ptr<CCreditPoolManager>& cpoolman,
                              std::unique_ptr<CDeterministicMNManager>& dmnman,
                              std::unique_ptr<CMNHFManager>& mnhf_manager,
                              std::unique_ptr<LLMQContext>& llmq_ctx,
                              CTxMemPool* mempool)

{
    chain_helper.reset();
    if (mnhf_manager) {
        mnhf_manager->DisconnectManagers();
    }
    llmq_ctx.reset();
    cpoolman.reset();
    mempool->DisconnectManagers();
    dmnman.reset();
}

ChainstateLoadResult VerifyLoadedChainstate(ChainstateManager& chainman,
                                            CEvoDB& evodb,
                                            const ChainstateLoadOptions& options,
                                            const Consensus::Params& consensus_params,
                                            std::function<int64_t()> get_unix_time_seconds,
                                            std::function<void(bool)> notify_bls_state)
{
    auto is_coinsview_empty = [&](CChainState* chainstate) EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return options.reindex || options.reindex_chainstate || chainstate->CoinsTip().GetBestBlock().IsNull();
    };

    LOCK(cs_main);

    for (CChainState* chainstate : chainman.GetAll()) {
        if (!is_coinsview_empty(chainstate)) {
            const CBlockIndex* tip = chainstate->m_chain.Tip();
            if (tip && tip->nTime > get_unix_time_seconds() + MAX_FUTURE_BLOCK_TIME) {
                return {ChainstateLoadStatus::FAILURE, _("The block database contains a block which appears to be from the future. "
                                                         "This may be due to your computer's date and time being set incorrectly. "
                                                         "Only rebuild the block database if you are sure that your computer's date and time are correct")};
            }
            const bool v19active{DeploymentActiveAfter(tip, consensus_params, Consensus::DEPLOYMENT_V19)};
            if (v19active) {
                bls::bls_legacy_scheme.store(false);
                if (notify_bls_state) notify_bls_state(bls::bls_legacy_scheme.load());
            }

            if (!CVerifyDB().VerifyDB(
                    *chainstate, consensus_params, chainstate->CoinsDB(),
                    evodb,
                    options.check_level,
                    options.check_blocks)) {
                return {ChainstateLoadStatus::FAILURE, _("Corrupted block database detected")};
            }

            // VerifyDB() disconnects blocks which might result in us switching back to legacy.
            // Make sure we use the right scheme.
            if (v19active && bls::bls_legacy_scheme.load()) {
                bls::bls_legacy_scheme.store(false);
                if (notify_bls_state) notify_bls_state(bls::bls_legacy_scheme.load());
            }

            if (options.check_level >= 3) {
                chainstate->ResetBlockFailureFlags(nullptr);
            }

        } else {
            // TODO: CEvoDB instance should probably be a part of CChainState
            // (for multiple chainstates to actually work in parallel)
            // and not a global
            if (&chainman.ActiveChainstate() == chainstate && !evodb.IsEmpty()) {
                // EvoDB processed some blocks earlier but we have no blocks anymore, something is wrong
                return {ChainstateLoadStatus::FAILURE, _("Error initializing block database")};
            }
        }
    }

    return {ChainstateLoadStatus::SUCCESS, {}};
}
} // namespace node
