// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/fees.h>
<<<<<<< HEAD
=======
#include <policy/fees_args.h>
#include <streams.h>
>>>>>>> afd3e99856 (Merge bitcoin/bitcoin#28873: fuzz: AutoFile with XOR)
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>
#include <test/util/setup_common.h>

#include <memory>

void initialize_policy_estimator_io()
{
    static const auto testing_setup = MakeNoLogFileContext<>();
}

FUZZ_TARGET(policy_estimator_io, .init = initialize_policy_estimator_io)
{
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());
    FuzzedFileProvider fuzzed_file_provider{fuzzed_data_provider};
    CAutoFile fuzzed_auto_file{fuzzed_file_provider.open()};
    // Re-using block_policy_estimator across runs to avoid costly creation of CBlockPolicyEstimator object.
    static CBlockPolicyEstimator block_policy_estimator;
    if (block_policy_estimator.Read(fuzzed_auto_file)) {
        block_policy_estimator.Write(fuzzed_auto_file);
    }
}
