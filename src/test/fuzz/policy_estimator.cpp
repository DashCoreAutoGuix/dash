// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/fees.h>
#include <primitives/transaction.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>
#include <test/util/setup_common.h>
#include <txmempool.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Simple implementation of ConsumeTxMemPoolEntry for Dash
static CTxMemPoolEntry ConsumeTxMemPoolEntry(FuzzedDataProvider& fuzzed_data_provider, const CTransaction& tx) noexcept
{
    const CAmount fee = ConsumeMoney(fuzzed_data_provider, /*max=*/std::numeric_limits<CAmount>::max() / CAmount{100'000});
    const int64_t time = fuzzed_data_provider.ConsumeIntegral<int64_t>();
    const unsigned int entry_height = fuzzed_data_provider.ConsumeIntegral<unsigned int>();
    const bool spends_coinbase = fuzzed_data_provider.ConsumeBool();
    const unsigned int sig_op_cost = fuzzed_data_provider.ConsumeIntegralInRange<unsigned int>(0, 80000); // MAX_BLOCK_SIGOPS_COST
    return CTxMemPoolEntry{MakeTransactionRef(tx), fee, time, entry_height, spends_coinbase, sig_op_cost, {}};
}

void initialize_policy_estimator()
{
    static const auto testing_setup = MakeNoLogFileContext<>();
}

FUZZ_TARGET(policy_estimator, .init = initialize_policy_estimator)
{
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());
    CBlockPolicyEstimator block_policy_estimator;
    LIMITED_WHILE(fuzzed_data_provider.ConsumeBool(), 10000) {
        CallOneOf(
            fuzzed_data_provider,
            [&] {
                const std::optional<CMutableTransaction> mtx = ConsumeDeserializable<CMutableTransaction>(fuzzed_data_provider);
                if (!mtx) {
                    return;
                }
                const CTransaction tx{*mtx};
                block_policy_estimator.processTransaction(ConsumeTxMemPoolEntry(fuzzed_data_provider, tx), fuzzed_data_provider.ConsumeBool());
                if (fuzzed_data_provider.ConsumeBool()) {
                    (void)block_policy_estimator.removeTx(tx.GetHash(), /*inBlock=*/fuzzed_data_provider.ConsumeBool());
                }
            },
            [&] {
                std::vector<CTxMemPoolEntry> mempool_entries;
                LIMITED_WHILE(fuzzed_data_provider.ConsumeBool(), 10000) {
                    const std::optional<CMutableTransaction> mtx = ConsumeDeserializable<CMutableTransaction>(fuzzed_data_provider);
                    if (!mtx) {
                        break;
                    }
                    const CTransaction tx{*mtx};
                    mempool_entries.push_back(ConsumeTxMemPoolEntry(fuzzed_data_provider, tx));
                }
                std::vector<const CTxMemPoolEntry*> ptrs;
                ptrs.reserve(mempool_entries.size());
                for (const CTxMemPoolEntry& mempool_entry : mempool_entries) {
                    ptrs.push_back(&mempool_entry);
                }
                block_policy_estimator.processBlock(fuzzed_data_provider.ConsumeIntegral<unsigned int>(), ptrs);
            },
            [&] {
                (void)block_policy_estimator.removeTx(ConsumeUInt256(fuzzed_data_provider), /*inBlock=*/fuzzed_data_provider.ConsumeBool());
            },
            [&] {
                block_policy_estimator.FlushUnconfirmed();
            });
        (void)block_policy_estimator.estimateFee(fuzzed_data_provider.ConsumeIntegral<int>());
        EstimationResult result;
        (void)block_policy_estimator.estimateRawFee(fuzzed_data_provider.ConsumeIntegral<int>(), fuzzed_data_provider.ConsumeFloatingPoint<double>(), fuzzed_data_provider.PickValueInArray(ALL_FEE_ESTIMATE_HORIZONS), fuzzed_data_provider.ConsumeBool() ? &result : nullptr);
        FeeCalculation fee_calculation;
        (void)block_policy_estimator.estimateSmartFee(fuzzed_data_provider.ConsumeIntegral<int>(), fuzzed_data_provider.ConsumeBool() ? &fee_calculation : nullptr, fuzzed_data_provider.ConsumeBool());
        (void)block_policy_estimator.HighestTargetTracked(fuzzed_data_provider.PickValueInArray(ALL_FEE_ESTIMATE_HORIZONS));
    }
    {
        FuzzedAutoFileProvider fuzzed_auto_file_provider = ConsumeAutoFile(fuzzed_data_provider);
        CAutoFile fuzzed_auto_file = fuzzed_auto_file_provider.open();
        block_policy_estimator.Write(fuzzed_auto_file);
        block_policy_estimator.Read(fuzzed_auto_file);
    }
}
