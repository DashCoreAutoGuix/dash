#!/usr/bin/env python3
# Copyright (c) 2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RPCs that handle raw transaction packages."""

from decimal import Decimal
import random

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.messages import (
    tx_from_hex,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
)
from test_framework.wallet import (
    MiniWallet,
)


class RPCPackagesTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def assert_testres_equal(self, package_hex, testres_expected):
        """Shuffle package_hex and assert that the testmempoolaccept result matches testres_expected. This should only
        be used to test packages where the order does not matter. The ordering of transactions in package_hex and
        testres_expected must match.
        """
        shuffled_indeces = list(range(len(package_hex)))
        random.shuffle(shuffled_indeces)
        shuffled_package = [package_hex[i] for i in shuffled_indeces]
        shuffled_testres = [testres_expected[i] for i in shuffled_indeces]
        assert_equal(shuffled_testres, self.nodes[0].testmempoolaccept(shuffled_package))

    def run_test(self):
        node = self.nodes[0]

        # get an UTXO that requires signature to be spent
        deterministic_address = node.get_deterministic_priv_key().address
        blockhash = self.generatetoaddress(node, 1, deterministic_address)[0]
        coinbase = node.getblock(blockhash=blockhash, verbosity=2)["tx"][0]
        coin = {
                "txid": coinbase["txid"],
                "amount": coinbase["vout"][0]["value"],
                "scriptPubKey": coinbase["vout"][0]["scriptPubKey"],
                "vout": 0,
                "height": 0
            }

        self.wallet = MiniWallet(self.nodes[0])
        self.generate(self.wallet, COINBASE_MATURITY + 100)  # blocks generated for inputs

        self.log.info("Create some transactions")
        # Create some transactions that can be reused throughout the test. Never submit these to mempool.
        self.independent_txns_hex = []
        self.independent_txns_testres = []
        for _ in range(3):
            tx_hex = self.wallet.create_self_transfer(fee_rate=Decimal("0.0001"))["hex"]
            testres = self.nodes[0].testmempoolaccept([tx_hex])
            assert testres[0]["allowed"]
            self.independent_txns_hex.append(tx_hex)
            # testmempoolaccept returns a list of length one, avoid creating a 2D list
            self.independent_txns_testres.append(testres[0])
        self.independent_txns_testres_blank = [{
            "txid": res["txid"]} for res in self.independent_txns_testres]

        self.test_independent(coin)
        self.test_chain()
        self.test_multiple_children()
        self.test_multiple_parents()
        self.test_conflicting()


    def test_independent(self, coin):
        self.log.info("Test multiple independent transactions in a package")
        node = self.nodes[0]
        # For independent transactions, order doesn't matter.
        self.assert_testres_equal(self.independent_txns_hex, self.independent_txns_testres)

        self.log.info("Test an otherwise valid package with an extra garbage tx appended")
        address = node.get_deterministic_priv_key().address
        garbage_tx = node.createrawtransaction([{"txid": "00" * 32, "vout": 5}], {address: 1})
        tx = tx_from_hex(garbage_tx)
        # Only the txid is returned because validation is incomplete for the independent txns.
        # Package validation is atomic: if the node cannot find a UTXO for any single tx in the package,
        # it terminates immediately to avoid unnecessary, expensive signature verification.
        package_bad = self.independent_txns_hex + [garbage_tx]
        testres_bad = self.independent_txns_testres_blank + [{"txid": tx.rehash(), "allowed": False, "reject-reason": "missing-inputs"}]
        self.assert_testres_equal(package_bad, testres_bad)

        self.log.info("Check testmempoolaccept tells us when some transactions completed validation successfully")
        tx_bad_sig_hex = node.createrawtransaction([{"txid": coin["txid"], "vout": 0}],
                                           {address : coin["amount"] - Decimal("0.0001")})
        tx_bad_sig = tx_from_hex(tx_bad_sig_hex)
        tx_bad_sig_hex = tx_bad_sig.serialize().hex()
        testres_bad_sig = node.testmempoolaccept(self.independent_txns_hex + [tx_bad_sig_hex])
        # By the time the signature for the last transaction is checked, all the other transactions
        # have been fully validated, which is why the node returns full validation results for all
        # transactions here but empty results in other cases.
        assert_equal(testres_bad_sig, self.independent_txns_testres + [{
            "txid": tx_bad_sig.rehash(),
            "allowed": False,
            "reject-reason": "mandatory-script-verify-flag-failed (Operation not valid with the current stack size)"
        }])

        self.log.info("Check testmempoolaccept reports txns in packages that exceed max feerate")
        tx_high_fee = self.wallet.create_self_transfer(fee=Decimal("0.999"))
        testres_high_fee = node.testmempoolaccept([tx_high_fee["hex"]])
        assert_equal(testres_high_fee, [
            {"txid": tx_high_fee["txid"], "allowed": False, "reject-reason": "max-fee-exceeded"}
        ])
        package_high_fee = [tx_high_fee["hex"]] + self.independent_txns_hex
        testres_package_high_fee = node.testmempoolaccept(package_high_fee)
        assert_equal(testres_package_high_fee, testres_high_fee + self.independent_txns_testres_blank)

    def test_chain(self):
        node = self.nodes[0]

        chain = self.wallet.create_self_transfer_chain(chain_length=25)
        chain_hex = chain["chain_hex"]
        chain_txns = chain["chain_txns"]

        self.log.info("Check that testmempoolaccept requires packages to be sorted by dependency")
        assert_equal(node.testmempoolaccept(rawtxs=chain_hex[::-1]),
                [{"txid": tx.rehash(), "package-error": "package-not-sorted"} for tx in chain_txns[::-1]])

        self.log.info("Testmempoolaccept a chain of 25 transactions")
        testres_multiple = node.testmempoolaccept(rawtxs=chain_hex)

        testres_single = []
        # Test accept and then submit each one individually, which should be identical to package test accept
        for rawtx in chain_hex:
            testres = node.testmempoolaccept([rawtx])
            testres_single.append(testres[0])
            # Submit the transaction now so its child should have no problem validating
            node.sendrawtransaction(rawtx)
        assert_equal(testres_single, testres_multiple)

        # Clean up by clearing the mempool
        self.generate(node, 1)

    def test_multiple_children(self):
        node = self.nodes[0]
        self.log.info("Testmempoolaccept a package in which a transaction has two children within the package")

        parent_tx = self.wallet.create_self_transfer_multi(num_outputs=2)
        assert node.testmempoolaccept([parent_tx["hex"]])[0]["allowed"]

        # Child A
        child_a_tx = self.wallet.create_self_transfer(utxo_to_spend=parent_tx["new_utxos"][0])
        assert not node.testmempoolaccept([child_a_tx["hex"]])[0]["allowed"]

        # Child B
        child_b_tx = self.wallet.create_self_transfer(utxo_to_spend=parent_tx["new_utxos"][1])
        assert not node.testmempoolaccept([child_b_tx["hex"]])[0]["allowed"]

        self.log.info("Testmempoolaccept with entire package, should work with children in either order")
        testres_multiple_ab = node.testmempoolaccept(rawtxs=[parent_tx["hex"], child_a_tx["hex"], child_b_tx["hex"]])
        testres_multiple_ba = node.testmempoolaccept(rawtxs=[parent_tx["hex"], child_b_tx["hex"], child_a_tx["hex"]])
        assert all([testres["allowed"] for testres in testres_multiple_ab + testres_multiple_ba])

        testres_single = []
        # Test accept and then submit each one individually, which should be identical to package testaccept
        for rawtx in [parent_tx["hex"], child_a_tx["hex"], child_b_tx["hex"]]:
            testres = node.testmempoolaccept([rawtx])
            testres_single.append(testres[0])
            # Submit the transaction now so its child should have no problem validating
            node.sendrawtransaction(rawtx)
        assert_equal(testres_single, testres_multiple_ab)

    def test_multiple_parents(self):
        node = self.nodes[0]
        self.log.info("Testmempoolaccept a package in which a transaction has multiple parents within the package")

        for num_parents in [2, 10, 24]:
            # Test a package with num_parents parents and 1 child transaction.
            parent_coins = []
            package_hex = []

            for _ in range(num_parents):
                # Package accept should work with the parents in any order (as long as parents come before child)
                parent_tx = self.wallet.create_self_transfer()
                parent_coins.append(parent_tx["new_utxo"])
                package_hex.append(parent_tx["hex"])

            child_tx = self.wallet.create_self_transfer_multi(utxos_to_spend=parent_coins, fee_per_output=2000)
            for _ in range(10):
                random.shuffle(package_hex)
                testres_multiple = node.testmempoolaccept(rawtxs=package_hex + [child_tx['hex']])
                assert all([testres["allowed"] for testres in testres_multiple])

            testres_single = []
            # Test accept and then submit each one individually, which should be identical to package testaccept
            for rawtx in package_hex + [child_tx["hex"]]:
                testres_single.append(node.testmempoolaccept([rawtx])[0])
                # Submit the transaction now so its child should have no problem validating
                node.sendrawtransaction(rawtx)
            assert_equal(testres_single, testres_multiple)

    def test_conflicting(self):
        node = self.nodes[0]
        coin = self.wallet.get_utxo()

        # tx1 and tx2 share the same inputs
        tx1 = self.wallet.create_self_transfer(utxo_to_spend=coin)
        tx2 = self.wallet.create_self_transfer(utxo_to_spend=coin)

        # Ensure tx1 and tx2 are valid by themselves
        assert node.testmempoolaccept([tx1["hex"]])[0]["allowed"]
        assert node.testmempoolaccept([tx2["hex"]])[0]["allowed"]

        self.log.info("Test duplicate transactions in the same package")
        testres = node.testmempoolaccept([tx1["hex"], tx1["hex"]])
        assert_equal(testres, [
            {"txid": tx1["txid"], "package-error": "conflict-in-package"},
            {"txid": tx1["txid"], "package-error": "conflict-in-package"}
        ])

        self.log.info("Test conflicting transactions in the same package")
        testres = node.testmempoolaccept([tx1["hex"], tx2["hex"]])
        assert_equal(testres, [
            {"txid": tx1["txid"], "package-error": "conflict-in-package"},
            {"txid": tx2["txid"], "package-error": "conflict-in-package"}
        ])


if __name__ == "__main__":
    RPCPackagesTest().main()
