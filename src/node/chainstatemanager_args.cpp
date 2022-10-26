// Copyright (c) 2022 The Bitcoin Core developers
// Copyright (c) 2025 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/chainstatemanager_args.h>

#include <arith_uint256.h>
#include <tinyformat.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/translation.h>
#include <validation.h>

#include <chrono>
#include <optional>
#include <string>

namespace node {
std::optional<bilingual_str> ApplyArgsManOptions(const ArgsManager& args, ChainstateManager::Options& opts)
{
    if (args.IsArgSet("-checkblockindex")) {
        opts.check_block_index = args.GetBoolArg("-checkblockindex", opts.chainparams.DefaultConsistencyChecks());
    }

    if (args.IsArgSet("-checkpoints")) {
        opts.checkpoints_enabled = args.GetBoolArg("-checkpoints", DEFAULT_CHECKPOINTS_ENABLED);
    }

    if (args.IsArgSet("-minimumchainwork")) {
        const std::string minChainWorkStr = args.GetArg("-minimumchainwork", "");
        if (!IsHexNumber(minChainWorkStr)) {
            return strprintf(Untranslated("Invalid non-hex (%s) minimum chain work value specified"), minChainWorkStr);
        }
        opts.minimum_chain_work = UintToArith256(uint256S(minChainWorkStr));
    }

    if (args.IsArgSet("-assumevalid")) {
        opts.assumed_valid_block = uint256S(args.GetArg("-assumevalid", opts.chainparams.GetConsensus().defaultAssumeValid.GetHex()));
    }

    if (args.IsArgSet("-maxtipage")) {
        opts.max_tip_age = std::chrono::seconds{args.GetIntArg("-maxtipage", DEFAULT_MAX_TIP_AGE)};
    }

    return std::nullopt;
}
} // namespace node
