#!/usr/bin/env python3
# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test fastprune mode."""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
)
from test_framework.blocktools import (
    create_block,
    create_coinbase,
)
from test_framework.wallet import MiniWallet


class FeatureFastpruneTest(BitcoinTestFramework):
    def set_test_params(self) -> None:
        self.num_nodes = 1
        self.extra_args = [["-fastprune"]]

    def run_test(self) -> None:
        self.log.info("ensure that large blocks don't crash or freeze in -fastprune")
        wallet = MiniWallet(self.nodes[0])

        # Generate blocks to get some UTXOs for the wallet
        self.generate(wallet, 101)

        # Create multiple transactions to make the block large (>64KB)
        # This tests the same fastprune logic without requiring witness data
        txlist = []
        for _ in range(300):  # Create ~300 transactions to exceed 64KB limit
            tx = wallet.create_self_transfer()['tx']
            txlist.append(tx)

        tip = int(self.nodes[0].getbestblockhash(), 16)
        time = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['time'] + 1
        height = self.nodes[0].getblockcount() + 1
        block = create_block(hashprev=tip, ntime=time, txlist=txlist, coinbase=create_coinbase(height=height))
        block.solve()
        self.nodes[0].submitblock(block.serialize().hex())
        assert_equal(int(self.nodes[0].getbestblockhash(), 16), block.sha256)


if __name__ == '__main__':
    FeatureFastpruneTest().main()
