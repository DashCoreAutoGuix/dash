#!/usr/bin/env python3
# Copyright (c) 2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that legacy txindex will be disabled on upgrade.

Previous releases are required by this test, see test/README.md.
"""

import os
import shutil

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal
from test_framework.wallet import MiniWallet


class TxindexCompatibilityTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.extra_args = [
            ["-reindex", "-txindex"],
            ["-txindex=0"],
            ["-txindex=0"],
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_previous_releases()

    def setup_network(self):
        self.add_nodes(
            self.num_nodes,
            self.extra_args,
            versions=[
                160300,  # Last release with legacy txindex
                None,  # For MiniWallet, without migration code
                220000,  # Last release with migration code (0.17.x - 22.x)
            ],
        )
        # Delete previous release cached datadir to avoid making a legacy version try to
        # make sense of our current database formats
        shutil.rmtree(os.path.join(self.nodes[2].datadir, self.chain))
        self.start_nodes()
        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)

    def run_test(self):
        mini_wallet = MiniWallet(self.nodes[1])

        spend_utxo = mini_wallet.get_utxo()
        mini_wallet.send_self_transfer(from_node=self.nodes[1], utxo_to_spend=spend_utxo)
        self.generate(self.nodes[1], 1, sync_fun=self.no_op)

        self.log.info("Check legacy txindex")
        assert_equal(self.nodes[0].getmempoolinfo()["loaded"], True)
        self.nodes[0].getrawtransaction(txid=spend_utxo["txid"])  # Requires -txindex

        self.stop_nodes()
        legacy_chain_dir = self.nodes[0].chain_path

        self.log.info("Drop legacy txindex")
        drop_index_chain_dir = self.nodes[1].chain_path
        shutil.rmtree(drop_index_chain_dir)
        shutil.copytree(legacy_chain_dir, drop_index_chain_dir)
        self.nodes[1].assert_start_raises_init_error(
            extra_args=["-txindex"],
            expected_msg="Error: The block index db contains a legacy 'txindex'. To clear the occupied disk space, run a full -reindex, otherwise ignore this error. This error message will not be displayed again.",
        )
        # Build txindex from scratch and check there is no error this time
        self.start_node(1, extra_args=["-txindex"])
        self.wait_until(lambda: self.nodes[1].getmempoolinfo()["loaded"])
        self.nodes[1].getrawtransaction(txid=spend_utxo["txid"])  # Requires -txindex

        self.stop_nodes()

        self.log.info("Check migrated txindex cannot be read by legacy node")
        err_msg = f": You need to rebuild the database using -reindex to change -txindex.{os.linesep}Please restart with -reindex or -reindex-chainstate to recover."
        shutil.rmtree(legacy_chain_dir)
        shutil.copytree(drop_index_chain_dir, legacy_chain_dir)
        self.nodes[0].assert_start_raises_init_error(extra_args=["-txindex=0"], expected_msg=err_msg)

        self.log.info("Check migrated txindex cannot be dropped")
        migrate_chain_dir = self.nodes[2].chain_path
        shutil.rmtree(migrate_chain_dir)
        shutil.copytree(legacy_chain_dir, migrate_chain_dir)
        self.nodes[0].assert_start_raises_init_error(extra_args=["-txindex"], expected_msg=err_msg)
        shutil.rmtree(legacy_chain_dir)
        shutil.copytree(drop_index_chain_dir, legacy_chain_dir)
        self.nodes[0].assert_start_raises_init_error(extra_args=["-txindex"], expected_msg=err_msg)


if __name__ == "__main__":
    TxindexCompatibilityTest().main()