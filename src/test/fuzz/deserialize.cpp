// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <addrdb.h>
#include <addrman.h>
#include <addrman_impl.h>
#include <blockencodings.h>
#include <blockfilter.h>
#include <chain.h>
#include <coins.h>
#include <compressor.h>
#include <consensus/merkle.h>
#include <key.h>
#include <merkleblock.h>
#include <net.h>
#include <netbase.h>
#include <netgroup.h>
#include <node/utxo_snapshot.h>
#include <primitives/block.h>
#include <protocol.h>
#include <psbt.h>
#include <script/sign.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <undo.h>
#include <util/system.h>
#include <version.h>

#include <exception>
#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <unistd.h>

#include <test/fuzz/fuzz.h>

using node::SnapshotMetadata;

namespace {
const BasicTestingSetup* g_setup;
} // namespace

void initialize_deserialize()
{
    static const auto testing_setup = MakeNoLogFileContext<>();
    g_setup = testing_setup.get();
}

#define FUZZ_TARGET_DESERIALIZE(name, code)                \
    FUZZ_TARGET(name, .init = initialize_deserialize)         \
    {                                                      \
        try {                                              \
            code                                           \
        } catch (const invalid_fuzzing_input_exception&) { \
        }                                                  \
    }

namespace {

struct invalid_fuzzing_input_exception : public std::exception {
};

template <typename T>
CDataStream Serialize(const T& obj)
{
    CDataStream ds{SER_NETWORK, INIT_PROTO_VERSION};
    ds << obj;
    return ds;
}

template <typename T>
T Deserialize(CDataStream ds)
{
    T obj;
    ds >> obj;
    return obj;
}

template <typename T>
void DeserializeFromFuzzingInput(FuzzBufferType buffer, T& obj)
{
    CDataStream ds{buffer, SER_NETWORK, INIT_PROTO_VERSION};
    {
        try {
            int version;
            ds >> version;
            ds.SetVersion(version);
        } catch (const std::ios_base::failure&) {
            throw invalid_fuzzing_input_exception();
        }
    }
    try {
        ds >> obj;
    } catch (const std::ios_base::failure&) {
        throw invalid_fuzzing_input_exception();
    }
    assert(buffer.empty() || !Serialize(obj).empty());
}

template <typename T>
void AssertEqualAfterSerializeDeserialize(const T& obj)
{
    assert(Deserialize<T>(Serialize(obj)) == obj);
}

} // namespace

/*
FUZZ_TARGET_DESERIALIZE(block_filter_deserialize, {
    BlockFilter block_filter;
    DeserializeFromFuzzingInput(buffer, block_filter);
})
*/
FUZZ_TARGET_DESERIALIZE(addr_info_deserialize, {
    AddrInfo addr_info;
    DeserializeFromFuzzingInput(buffer, addr_info);
})
FUZZ_TARGET_DESERIALIZE(block_file_info_deserialize, {
    CBlockFileInfo block_file_info;
    DeserializeFromFuzzingInput(buffer, block_file_info);
})
FUZZ_TARGET_DESERIALIZE(block_header_and_short_txids_deserialize, {
    CBlockHeaderAndShortTxIDs block_header_and_short_txids;
    DeserializeFromFuzzingInput(buffer, block_header_and_short_txids);
})
FUZZ_TARGET_DESERIALIZE(fee_rate_deserialize, {
    CFeeRate fee_rate;
    DeserializeFromFuzzingInput(buffer, fee_rate);
    AssertEqualAfterSerializeDeserialize(fee_rate);
})
FUZZ_TARGET_DESERIALIZE(merkle_block_deserialize, {
    CMerkleBlock merkle_block;
    DeserializeFromFuzzingInput(buffer, merkle_block);
})
FUZZ_TARGET_DESERIALIZE(out_point_deserialize, {
    COutPoint out_point;
    DeserializeFromFuzzingInput(buffer, out_point);
    AssertEqualAfterSerializeDeserialize(out_point);
})
FUZZ_TARGET_DESERIALIZE(partial_merkle_tree_deserialize, {
    CPartialMerkleTree partial_merkle_tree;
    DeserializeFromFuzzingInput(buffer, partial_merkle_tree);
})
FUZZ_TARGET_DESERIALIZE(pub_key_deserialize, {
    CPubKey pub_key;
    DeserializeFromFuzzingInput(buffer, pub_key);
    AssertEqualAfterSerializeDeserialize(pub_key);
})
FUZZ_TARGET_DESERIALIZE(script_deserialize, {
    CScript script;
    DeserializeFromFuzzingInput(buffer, script);
})
FUZZ_TARGET_DESERIALIZE(tx_in_deserialize, {
    CTxIn tx_in;
    DeserializeFromFuzzingInput(buffer, tx_in);
    AssertEqualAfterSerializeDeserialize(tx_in);
})
/*
FUZZ_TARGET_DESERIALIZE(flat_file_pos_deserialize, {
    FlatFilePos flat_file_pos;
    DeserializeFromFuzzingInput(buffer, flat_file_pos);
    AssertEqualAfterSerializeDeserialize(flat_file_pos);
})
FUZZ_TARGET_DESERIALIZE(key_origin_info_deserialize, {
    KeyOriginInfo key_origin_info;
    DeserializeFromFuzzingInput(buffer, key_origin_info);
    AssertEqualAfterSerializeDeserialize(key_origin_info);
})
*/
FUZZ_TARGET_DESERIALIZE(partially_signed_transaction_deserialize, {
    PartiallySignedTransaction partially_signed_transaction;
    DeserializeFromFuzzingInput(buffer, partially_signed_transaction);
})
FUZZ_TARGET_DESERIALIZE(prefilled_transaction_deserialize, {
    PrefilledTransaction prefilled_transaction;
    DeserializeFromFuzzingInput(buffer, prefilled_transaction);
})
FUZZ_TARGET_DESERIALIZE(psbt_input_deserialize, {
    PSBTInput psbt_input;
    DeserializeFromFuzzingInput(buffer, psbt_input);
})
FUZZ_TARGET_DESERIALIZE(psbt_output_deserialize, {
    PSBTOutput psbt_output;
    DeserializeFromFuzzingInput(buffer, psbt_output);
})
FUZZ_TARGET_DESERIALIZE(block_deserialize, {
    CBlock block;
    DeserializeFromFuzzingInput(buffer, block);
})
FUZZ_TARGET_DESERIALIZE(blocklocator_deserialize, {
    CBlockLocator bl;
    DeserializeFromFuzzingInput(buffer, bl);
})
FUZZ_TARGET_DESERIALIZE(blockmerkleroot, {
    CBlock block;
    DeserializeFromFuzzingInput(buffer, block);
    bool mutated;
    BlockMerkleRoot(block, &mutated);
})
FUZZ_TARGET_DESERIALIZE(addrman_deserialize, {
    NetGroupManager netgroupman{std::vector<bool>()};
    AddrMan am(netgroupman,
               /*deterministic=*/false,
               g_setup->m_node.args->GetIntArg("-checkaddrman", 0));
    DeserializeFromFuzzingInput(buffer, am);
})
FUZZ_TARGET_DESERIALIZE(blockheader_deserialize, {
    CBlockHeader bh;
    DeserializeFromFuzzingInput(buffer, bh);
})
FUZZ_TARGET_DESERIALIZE(txundo_deserialize, {
    CTxUndo tu;
    DeserializeFromFuzzingInput(buffer, tu);
})
FUZZ_TARGET_DESERIALIZE(blockundo_deserialize, {
    CBlockUndo bu;
    DeserializeFromFuzzingInput(buffer, bu);
})
FUZZ_TARGET_DESERIALIZE(coins_deserialize, {
    Coin coin;
    DeserializeFromFuzzingInput(buffer, coin);
})
FUZZ_TARGET_DESERIALIZE(netaddr_deserialize, {
    CNetAddr na;
    DeserializeFromFuzzingInput(buffer, na);
    if (na.IsAddrV1Compatible()) {
        AssertEqualAfterSerializeDeserialize(na, CNetAddr::V1);
    }
    AssertEqualAfterSerializeDeserialize(na, INIT_PROTO_VERSION | ADDRV2_FORMAT);
})
FUZZ_TARGET_DESERIALIZE(service_deserialize, {
    CService s;
    DeserializeFromFuzzingInput(buffer, s);
    if (s.IsAddrV1Compatible()) {
        AssertEqualAfterSerializeDeserialize(s, CNetAddr::V1);
    }
    AssertEqualAfterSerializeDeserialize(s, INIT_PROTO_VERSION | ADDRV2_FORMAT);
    CService s1;
    DeserializeFromFuzzingInput(buffer, s1, INIT_PROTO_VERSION);
    AssertEqualAfterSerializeDeserialize(s1, INIT_PROTO_VERSION);
    assert(s1.IsAddrV1Compatible());
    CService s2;
    DeserializeFromFuzzingInput(buffer, s2, INIT_PROTO_VERSION | ADDRV2_FORMAT);
    AssertEqualAfterSerializeDeserialize(s2, INIT_PROTO_VERSION | ADDRV2_FORMAT);
})
FUZZ_TARGET_DESERIALIZE(messageheader_deserialize, {
    CMessageHeader mh;
    DeserializeFromFuzzingInput(buffer, mh);
    (void)mh.IsCommandValid();
})
FUZZ_TARGET(address_deserialize, .init = initialize_deserialize)
{
    FuzzedDataProvider fdp{buffer.data(), buffer.size()};
    const auto ser_enc{ConsumeDeserializationParams<CAddress::SerParams>(fdp)};
    const auto maybe_a{ConsumeDeserializable<CAddress>(fdp, ser_enc)};
    if (!maybe_a) return;
    const CAddress& a{*maybe_a};
    // A CAddress in V1 mode will roundtrip
    // in all 4 formats (v1/v2, network/disk)
    if (ser_enc.enc == CNetAddr::Encoding::V1) {
        AssertEqualAfterSerializeDeserialize(a, CAddress::V1_NETWORK);
        AssertEqualAfterSerializeDeserialize(a, CAddress::V1_DISK);
        AssertEqualAfterSerializeDeserialize(a, CAddress::V2_NETWORK);
        AssertEqualAfterSerializeDeserialize(a, CAddress::V2_DISK);
    } else {
        // A CAddress in V2 mode will roundtrip in both V2 formats, and also in the V1 formats
        // if it's V1 compatible.
        if (a.IsAddrV1Compatible()) {
            AssertEqualAfterSerializeDeserialize(a, CAddress::V1_DISK);
            AssertEqualAfterSerializeDeserialize(a, CAddress::V1_NETWORK);
        }
        AssertEqualAfterSerializeDeserialize(a, CAddress::V2_NETWORK);
        AssertEqualAfterSerializeDeserialize(a, CAddress::V2_DISK);
    }
}
FUZZ_TARGET_DESERIALIZE(inv_deserialize, {
    CInv i;
    DeserializeFromFuzzingInput(buffer, i);
})
FUZZ_TARGET_DESERIALIZE(bloomfilter_deserialize, {
    CBloomFilter bf;
    DeserializeFromFuzzingInput(buffer, bf);
})
FUZZ_TARGET_DESERIALIZE(diskblockindex_deserialize, {
    CDiskBlockIndex dbi;
    DeserializeFromFuzzingInput(buffer, dbi);
})
FUZZ_TARGET_DESERIALIZE(txoutcompressor_deserialize, {
    CTxOut to;
    auto toc = Using<TxOutCompression>(to);
    DeserializeFromFuzzingInput(buffer, toc);
})
FUZZ_TARGET_DESERIALIZE(blocktransactions_deserialize, {
    BlockTransactions bt;
    DeserializeFromFuzzingInput(buffer, bt);
})
FUZZ_TARGET_DESERIALIZE(blocktransactionsrequest_deserialize, {
    BlockTransactionsRequest btr;
    DeserializeFromFuzzingInput(buffer, btr);
})
FUZZ_TARGET_DESERIALIZE(snapshotmetadata_deserialize, {
    SnapshotMetadata snapshot_metadata;
    DeserializeFromFuzzingInput(buffer, snapshot_metadata);
})
FUZZ_TARGET_DESERIALIZE(uint160_deserialize, {
    uint160 u160;
    DeserializeFromFuzzingInput(buffer, u160);
    AssertEqualAfterSerializeDeserialize(u160);
})
FUZZ_TARGET_DESERIALIZE(uint256_deserialize, {
    uint256 u256;
    DeserializeFromFuzzingInput(buffer, u256);
    AssertEqualAfterSerializeDeserialize(u256);
})
// Classes intentionally not covered in this file since their deserialization code is
// fuzzed elsewhere:
// * Deserialization of CTxOut is fuzzed in test/fuzz/tx_out.cpp
// * Deserialization of CMutableTransaction is fuzzed in src/test/fuzz/transaction.cpp
