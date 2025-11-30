#!/usr/bin/env python3
# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test fastprune mode."""
from test_framework.test_framework import DashTestFramework
from test_framework.util import (
    assert_equal
)
from test_framework.blocktools import (
    create_block,
    create_coinbase,
)
from test_framework.messages import (
    CTransaction,
    CTxIn,
    CTxOut,
    COutPoint,
)
from test_framework.script import CScript


class FeatureFastpruneTest(DashTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [["-fastprune"]]

    def run_test(self):
        self.log.info("ensure that large blocks don't crash or freeze in -fastprune")

        # Create a large transaction by adding many outputs
        # This tests the same fix as Bitcoin's version but without witness data
        tx = CTransaction()
        tx.vin = [CTxIn(COutPoint(0, 0xffffffff), CScript([0, 0]))]

        # Add many outputs to make the transaction large (>64KB when serialized in a block)
        # Each output is ~34 bytes, so we need ~2000 outputs to exceed 64KB
        for i in range(2500):
            tx.vout.append(CTxOut(0, CScript([i.to_bytes(2, 'little')])))

        tx.rehash()

        tip = int(self.nodes[0].getbestblockhash(), 16)
        block_time = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['time'] + 1
        height = self.nodes[0].getblockcount() + 1

        block = create_block(hashprev=tip, coinbase=create_coinbase(height=height), ntime=block_time, txlist=[tx])
        block.solve()

        self.nodes[0].submitblock(block.serialize().hex())
        assert_equal(int(self.nodes[0].getbestblockhash(), 16), block.sha256)


if __name__ == '__main__':
    FeatureFastpruneTest().main()
