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
from test_framework.messages import CTxOut, tx_from_hex
from test_framework.script import CScript, OP_RETURN
from test_framework.wallet import MiniWallet


class FeatureFastpruneTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [["-fastprune"]]

    def run_test(self):
        self.log.info("ensure that large blocks don't crash or freeze in -fastprune")
        wallet = MiniWallet(self.nodes[0])

        # Create a single transaction with large OP_RETURN to make block >64kb
        # We need to create a transaction that's large enough to exceed the fastprune limit
        tx = wallet.create_self_transfer()['tx']
        # Add a large OP_RETURN output (65kb of data to exceed 64kb fastprune limit)
        large_data = b'\x00' * 65536
        tx.vout.append(CTxOut(0, CScript([OP_RETURN, large_data])))
        tx.rehash()

        tip = int(self.nodes[0].getbestblockhash(), 16)
        time = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['time'] + 1
        height = self.nodes[0].getblockcount() + 1

        # Create proper CbTx for Dash (DIP4/v20 activated)
        cbb = create_coinbase(height, dip4_activated=True, v20_activated=True)
        gbt = self.nodes[0].getblocktemplate()
        cbb.vExtraPayload = bytes.fromhex(gbt["coinbase_payload"])
        cbb.rehash()

        block = create_block(hashprev=tip, ntime=time, txlist=[tx], coinbase=cbb, version=4)

        # Add quorum commitments from block template
        for tx_obj in gbt["transactions"]:
            tx = tx_from_hex(tx_obj["data"])
            if tx.nType == 6:  # TRANSACTION_QUORUM_COMMITMENT
                block.vtx.append(tx)

        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()
        result = self.nodes[0].submitblock(block.serialize().hex())
        # submitblock returns None on success, error string on failure
        if result is not None:
            raise AssertionError(f"submitblock failed: {result}")
        assert_equal(int(self.nodes[0].getbestblockhash(), 16), block.sha256)


if __name__ == '__main__':
    FeatureFastpruneTest().main()
