#!/usr/bin/env python3
# Copyright (c) 2020-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test generate* RPCs."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)
from test_framework.wallet import MiniWallet


class RPCGenerateTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        self.test_generatetoaddress()
        self.test_generate()
        self.test_generateblock()

    def test_generatetoaddress(self):
        self.generatetoaddress(self.nodes[0], 1, 'yjQ5gLvGRtmq1cwc4kePLCrzQ8GVCh9Gaz')
        assert_raises_rpc_error(-5, "Invalid address", self.generatetoaddress, self.nodes[0], 1, '3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy')

    def test_generate(self):
        message = (
            "generate\n\n"
            "has been replaced by the -generate "
            "cli option. Refer to -help for more information.\n"
        )

        self.log.info("Test rpc generate raises with message to use cli option")
        assert_raises_rpc_error(-32601, message, self.nodes[0].rpc.generate)

        self.log.info("Test rpc generate help prints message to use cli option")
        assert_equal(message, self.nodes[0].help("generate"))

        self.log.info("Test rpc generate is a hidden command not discoverable in general help")
        assert message not in self.nodes[0].help()

    def test_generateblock(self):
        node = self.nodes[0]
        miniwallet = MiniWallet(node)

        self.log.info('Mine an empty block to address and return the hex')
        address = miniwallet.get_address()
        generated_block = self.generateblock(node, output=address, transactions=[], submit=False)
        node.submitblock(hexdata=generated_block['hex'])
        assert_equal(generated_block['hash'], node.getbestblockhash())

        self.log.info('Generate an empty block to address')
        hash = self.generateblock(node, output=address, transactions=[])['hash']
        block = node.getblock(blockhash=hash, verbose=2)
        assert_equal(len(block['tx']), 1)
        assert_equal(block['tx'][0]['vout'][0]['scriptPubKey']['address'], address)

        self.log.info('Generate an empty block to a descriptor')
        hash = self.generateblock(node, 'addr(' + address + ')', [])['hash']
        block = node.getblock(blockhash=hash, verbosity=2)
        assert_equal(len(block['tx']), 1)
        assert_equal(block['tx'][0]['vout'][0]['scriptPubKey']['address'], address)

        self.log.info('Generate an empty block to a combo descriptor with compressed pubkey')
        combo_key = '0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798'
        combo_address = 'yWziQMcwmKjRdzi7eWjwiQX8EjWcd6dSg6'
        hash = self.generateblock(node, 'combo(' + combo_key + ')', [])['hash']
        block = node.getblock(hash, 2)
        assert_equal(len(block['tx']), 1)
        assert_equal(block['tx'][0]['vout'][0]['scriptPubKey']['address'], combo_address)

        self.log.info('Generate an empty block to a combo descriptor with uncompressed pubkey')
        combo_key = '0408ef68c46d20596cc3f6ddf7c8794f71913add807f1dc55949fa805d764d191c0b7ce6894c126fce0babc6663042f3dde9b0cf76467ea315514e5a6731149c67'
        combo_address = 'yjQ5gLvGRtmq1cwc4kePLCrzQ8GVCh9Gaz'
        hash = self.generateblock(node, 'combo(' + combo_key + ')', [])['hash']
        block = node.getblock(hash, 2)
        assert_equal(len(block['tx']), 1)
        assert_equal(block['tx'][0]['vout'][0]['scriptPubKey']['address'], combo_address)

        self.log.info('Generate an empty block with an invalid address/descriptor')
        assert_raises_rpc_error(-5, 'Invalid address/descriptor', self.generateblock, node, 'yInvalid', [])

        self.log.info('Generate an empty block with a valid but not-in-wallet address')
        address_not_in_wallet = 'yNLuVTU7jUv21dGv1wGNTBt9Ad5mmP9XCM'
        hash = self.generateblock(node, address_not_in_wallet, [])['hash']
        block = node.getblock(hash, 2)
        assert_equal(len(block['tx']), 1)
        assert_equal(block['tx'][0]['vout'][0]['scriptPubKey']['address'], address_not_in_wallet)

        self.log.info('Generate block with P2SH address')
        p2sh_addr = '87VumiUDNhD1j9hm3LrHRKqNCWB7AG1VQf'
        hash = self.generateblock(node, p2sh_addr, [])['hash']
        block = node.getblock(hash, 2)
        assert_equal(len(block['tx']), 1)
        assert_equal(block['tx'][0]['vout'][0]['scriptPubKey']['address'], p2sh_addr)


if __name__ == "__main__":
    RPCGenerateTest().main()
