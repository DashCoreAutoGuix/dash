#!/usr/bin/env python3
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the generation of UTXO snapshots using `dumptxoutset`.
"""

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

import hashlib
from pathlib import Path


class DumptxoutsetTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        """Test a trivial usage of the dumptxoutset RPC command."""
        node = self.nodes[0]
        mocktime = node.getblockheader(node.getblockhash(0))['time'] + 1
        node.setmocktime(mocktime)
        self.generate(node, COINBASE_MATURITY)

        FILENAME = 'txoutset.dat'
        out = node.dumptxoutset(FILENAME)
        expected_path = Path(node.datadir) / self.chain / FILENAME

        assert expected_path.is_file()

        assert_equal(out['coins_written'], 100)
        assert_equal(out['base_height'], 100)
        assert_equal(out['path'], str(expected_path))
        # Blockhash should be deterministic based on mocked time.
        assert_equal(
            out['base_hash'],
            '38619d68335f21ded08d6e1f4ef9572eaeb0bb04b9070b3e6be33c7c908564b4')

        with open(str(expected_path), 'rb') as f:
            digest = hashlib.sha256(f.read()).hexdigest()
            # UTXO snapshot hash should be deterministic based on mocked time.
            assert_equal(
                digest, '4a34cf865938252bf0bef702989955824d886f595afab596b1edac4dd31cd89f')

        assert_equal(
            out['txoutset_hash'], 'a0b7baa3bf5ccbd3279728f230d7ca0c44a76e9923fca8f32dbfd08d65ea496a')
        assert_equal(out['nchaintx'], 101)

        # Specifying a path to an existing file will fail.
        assert_raises_rpc_error(
            -8, '{} already exists'.format(FILENAME),  node.dumptxoutset, FILENAME)

if __name__ == '__main__':
    DumptxoutsetTest().main()
