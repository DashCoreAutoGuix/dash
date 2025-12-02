#!/usr/bin/env python3
# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test fastprune mode."""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal
)
from test_framework.blocktools import (
    create_block,
    create_coinbase,
)
from test_framework.messages import tx_from_hex
from test_framework.wallet import MiniWallet


class FeatureFastpruneTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [["-fastprune"]]

    def run_test(self):
        self.log.info("ensure that large blocks don't crash or freeze in -fastprune")
        wallet = MiniWallet(self.nodes[0])

        # Generate blocks to fund the wallet with UTXOs
        # We need at least 500 UTXOs for 500 transactions
        self.generate(wallet, 500)

        # Create many transactions to make a large block (>64kb)
        # Since Dash doesn't have witness data, we need to create many regular transactions
        txs = []
        for _ in range(500):  # Create enough transactions to exceed 64kb
            tx = wallet.create_self_transfer()['tx']
            txs.append(tx)

        tip = int(self.nodes[0].getbestblockhash(), 16)
        time = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['time'] + 1
        height = self.nodes[0].getblockcount() + 1

        # Create proper CbTx for Dash (DIP4/v20 activated)
        cbb = create_coinbase(height, dip4_activated=True, v20_activated=True)
        gbt = self.nodes[0].getblocktemplate()
        cbb.vExtraPayload = bytes.fromhex(gbt["coinbase_payload"])
        cbb.rehash()

        block = create_block(hashprev=tip, ntime=time, txlist=txs, coinbase=cbb, version=4)

        # Add quorum commitments from block template
        for tx_obj in gbt["transactions"]:
            tx = tx_from_hex(tx_obj["data"])
            if tx.nType == 6:  # TRANSACTION_QUORUM_COMMITMENT
                block.vtx.append(tx)

        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()
        self.nodes[0].submitblock(block.serialize().hex())
        assert_equal(int(self.nodes[0].getbestblockhash(), 16), block.sha256)


if __name__ == '__main__':
    FeatureFastpruneTest().main()
