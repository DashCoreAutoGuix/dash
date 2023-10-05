#!/usr/bin/env python3
# Copyright (c) 2021-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RPCs that handle raw transaction packages."""

from decimal import Decimal
from io import BytesIO
import random

from test_framework.messages import (
    BIP125_SEQUENCE_NUMBER,
    MAX_BIP125_RBF_SEQUENCE,
    CTransaction,
    CTxInWitness,
    ser_compact_size,
)
from test_framework.p2p import P2PTxInvStore
from test_framework.test_framework import DashTestFramework
from test_framework.util import (
    assert_equal,
    assert_fee_amount,
    assert_raises_rpc_error,
    hex_str_to_bytes,
)
from test_framework.wallet import (
    DEFAULT_FEE,
    MiniWallet,
)

class RPCPackagesTest(DashTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        # TODO remove debug
        self.add_nodes(self.num_nodes, extra_args=[["-debug=fees"]])
        self.start_node(0)

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
        self.log.info("Generate blocks to create UTXOs")
        node = self.nodes[0]
        self.privkeys = [node.get_deterministic_priv_key().key]
        self.address = node.get_deterministic_priv_key().address
        self.coins = []
        # The last 100 coinbase transactions are premature
        for b in self.generatetoaddress(node, 200, self.address)[:100]:
            coinbase = node.getblock(blockhash=b, verbosity=2)["tx"][0]
            self.coins.append({
                "txid": coinbase["txid"],
                "amount": coinbase["vout"][0]["value"],
                "scriptPubKey": coinbase["vout"][0]["scriptPubKey"],
            })

        # Create transactions in order so that we can increment vouts in
        # transactions spent later. These transactions are prepared ahead of time
        # so we can test them in different orders. It doesn't matter what order
        # they're tested in because validity doesn't change (except in the case
        # of reorgs).
        self.independent_transactions_hex = []
        self.independent_transactions_testres = []
        for _ in range(3):
            coin = self.coins.pop(0)
            rawtx = node.createrawtransaction([{"txid" : coin["txid"], "vout" : 0}],
                                              {self.address : coin["amount"] - Decimal("0.01")})
            signedtx = node.signrawtransactionwithkey(hexstring=rawtx, privkeys=self.privkeys)
            assert_equal(signedtx["complete"], True)
            testres = node.testmempoolaccept([signedtx["hex"]])
            assert_equal(testres[0]["allowed"], True)
            self.independent_transactions_hex.append(signedtx["hex"])
            # testmempoolaccept returns a list of length one, avoid creating a 2D list
            self.independent_transactions_testres.append(testres[0])
        self.independent_transactions_testres_blank = [{
            "txid": res["txid"], "wtxid": res["wtxid"]} for res in self.independent_transactions_testres]

        self.wallet = MiniWallet(node)
        self.generate(self.wallet, 100)
        self.independent_txns_hex = []
        self.independent_txns_testres = []
        for _ in range(3):
            tx_hex = self.wallet.create_self_transfer()["hex"]
            testres = self.nodes[0].testmempoolaccept([tx_hex])
            assert testres[0]["allowed"]
            self.independent_txns_hex.append(tx_hex)
            # testmempoolaccept returns a list of length one, avoid creating a 2D list
            self.independent_txns_testres.append(testres[0])
        self.independent_txns_testres_blank = [{
            "txid": res["txid"], "wtxid": res["wtxid"]} for res in self.independent_txns_testres]

        self.test_independent()
        self.test_chain()
        self.test_multiple_children()
        self.test_multiple_parents()
        self.test_conflicting()
        self.test_rbf()
        self.test_submitpackage()

    def test_independent(self):
        self.log.info("Test multiple independent transactions in a package")
        node = self.nodes[0]
        # For independent transactions, order doesn't matter.
        self.assert_testres_equal(self.independent_txns_hex, self.independent_txns_testres)

        self.log.info("Test an empty package")
        assert_equal(node.testmempoolaccept(rawtxs=[]), [])

    def test_chain(self):
        node = self.nodes[0]
        (chain_hex, chain_txns) = self.wallet.create_self_transfer_chain(chain_length=25)
        self.log.info("Check that testmempoolaccept requires packages to be sorted by dependency")
        assert_equal(node.testmempoolaccept(rawtxs=chain_hex[::-1]),
                [{"txid": tx.rehash(), "wtxid": tx.getwtxid(), "package-error": "package-not-sorted"} for tx in chain_txns[::-1]])

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
        testres = node.testmempoolaccept(rawtxs=[parent_tx["hex"], child_a_tx["hex"], child_b_tx["hex"]])
        assert all([testres[i]["allowed"] for i in range(3)])

        testres_reversed = node.testmempoolaccept(rawtxs=[parent_tx["hex"], child_b_tx["hex"], child_a_tx["hex"]])
        assert all([testres_reversed[i]["allowed"] for i in range(3)])

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
                testres = node.testmempoolaccept(rawtxs=package_hex + [child_tx["hex"]])
                assert all([testres[i]["allowed"] for i in range(num_parents + 1)])

    def test_conflicting(self):
        node = self.nodes[0]
        prevtx = self.wallet.send_self_transfer(from_node=node)

        # tx1 spends prevtx
        tx1 = self.wallet.create_self_transfer(utxo_to_spend=prevtx["new_utxo"])
        # tx2 spends prevtx
        tx2 = self.wallet.create_self_transfer(utxo_to_spend=prevtx["new_utxo"])

        self.log.info("Testmempoolaccept with two conflicting transactions")
        testres = node.testmempoolaccept([tx1["hex"], tx2["hex"]])
        assert_equal(testres, [
            {"txid": tx1["txid"], "wtxid": tx1["wtxid"], "package-error": "conflict-in-package"},
            {"txid": tx2["txid"], "wtxid": tx2["wtxid"], "package-error": "conflict-in-package"}
        ])

        # Add tx1 to mempool, then try the same package again
        node.sendrawtransaction(tx1["hex"])
        testres = node.testmempoolaccept([tx1["hex"], tx2["hex"]])
        assert_equal(testres, [
            {"txid": tx1["txid"], "wtxid": tx1["wtxid"], "allowed": False, "reject-reason": "txn-already-in-mempool"},
            {"txid": tx2["txid"], "wtxid": tx2["wtxid"], "allowed": False, "reject-reason": "txn-mempool-conflict"}
        ])

    def test_conflicting_2(self):
        self.log.info("Test conflicting transactions in the same package")
        node = self.nodes[0]
        coin = self.coins.pop(0)

        # tx1 and tx2 share the same inputs
        rawtx1 = node.createrawtransaction([{"txid" : coin["txid"], "vout" : 0}],
            {self.address : coin["amount"] - Decimal("0.01")})
        signedtx1 = node.signrawtransactionwithkey(hexstring=rawtx1, privkeys=self.privkeys)

        rawtx2 = node.createrawtransaction([{"txid" : coin["txid"], "vout" : 0}],
            {self.address : coin["amount"] - Decimal("0.005")})
        signedtx2 = node.signrawtransactionwithkey(hexstring=rawtx2, privkeys=self.privkeys)

        assert signedtx1["complete"]
        assert signedtx2["complete"]
        tx1 = CTransaction()
        tx1.deserialize(BytesIO(hex_str_to_bytes(signedtx1["hex"])))
        tx2 = CTransaction()
        tx2.deserialize(BytesIO(hex_str_to_bytes(signedtx2["hex"])))
        assert_equal(tx1.vin[0].prevout.hash, tx2.vin[0].prevout.hash)

        # Ensure tx1 and tx2 are valid by themselves
        assert node.testmempoolaccept([signedtx1["hex"]])[0]["allowed"]
        assert node.testmempoolaccept([signedtx2["hex"]])[0]["allowed"]

        self.log.info("Test duplicate transactions in the same package")
        testres = node.testmempoolaccept([signedtx1["hex"], signedtx1["hex"]])
        assert_equal(testres, [
            {"txid": tx1.rehash(), "package-error": "conflict-in-package"},
            {"txid": tx1.rehash(), "package-error": "conflict-in-package"}
        ])

        self.log.info("Test conflicting transactions in the same package")
        testres = node.testmempoolaccept([signedtx1["hex"], signedtx2["hex"]])
        assert_equal(testres, [
            {"txid": tx1.rehash(), "package-error": "conflict-in-package"},
            {"txid": tx2.rehash(), "package-error": "conflict-in-package"}
        ])

    def test_rbf(self):
        node = self.nodes[0]

        coin = self.wallet.get_utxo()
        fee = Decimal("0.00125000")
        replaceable_tx = self.wallet.create_self_transfer(utxo_to_spend=coin, sequence=MAX_BIP125_RBF_SEQUENCE, fee = fee)
        testres_replaceable = node.testmempoolaccept([replaceable_tx["hex"]])[0]
        assert_equal(testres_replaceable["txid"], replaceable_tx["txid"])
        assert_equal(testres_replaceable["wtxid"], replaceable_tx["wtxid"])
        assert testres_replaceable["allowed"]
        assert_equal(testres_replaceable["vsize"], replaceable_tx["tx"].get_vsize())
        assert_equal(testres_replaceable["fees"]["base"], fee)
        assert_fee_amount(fee, replaceable_tx["tx"].get_vsize(), testres_replaceable["fees"]["effective-feerate"])
        assert_equal(testres_replaceable["fees"]["effective-includes"], [replaceable_tx["wtxid"]])

        # Replacement transaction is identical except has double the fee
        replacement_tx = self.wallet.create_self_transfer(utxo_to_spend=coin, sequence=MAX_BIP125_RBF_SEQUENCE, fee = 2 * fee)
        testres_rbf_conflicting = node.testmempoolaccept([replaceable_tx["hex"], replacement_tx["hex"]])
        assert_equal(testres_rbf_conflicting, [
            {"txid": replaceable_tx["txid"], "wtxid": replaceable_tx["wtxid"], "package-error": "conflict-in-package"},
            {"txid": replacement_tx["txid"], "wtxid": replacement_tx["wtxid"], "package-error": "conflict-in-package"}
        ])

        self.log.info("Test that packages cannot conflict with mempool transactions, even if a valid BIP125 RBF")
        # This transaction is a valid BIP125 replace-by-fee
        self.wallet.sendrawtransaction(from_node=node, tx_hex=replaceable_tx["hex"])
        testres_rbf_single = node.testmempoolaccept([replacement_tx["hex"]])
        assert testres_rbf_single[0]["allowed"]
        testres_rbf_package = self.independent_txns_testres_blank + [{
            "txid": replacement_tx["txid"], "wtxid": replacement_tx["wtxid"], "allowed": False,
            "reject-reason": "bip125-replacement-disallowed"
        }]
        self.assert_testres_equal(self.independent_txns_hex + [replacement_tx["hex"]], testres_rbf_package)

    def assert_equal_package_results(self, node, testmempoolaccept_result, submitpackage_result):
        """Assert that a successful submitpackage result is consistent with testmempoolaccept
        results and getmempoolentry info. Note that the result structs are different and, due to
        policy differences between testmempoolaccept and submitpackage (i.e. package feerate),
        some information may be different.
        """
        for testres_tx in testmempoolaccept_result:
            # Grab this result from the submitpackage_result
            submitres_tx = submitpackage_result["tx-results"][testres_tx["wtxid"]]
            assert_equal(submitres_tx["txid"], testres_tx["txid"])
            # No "allowed" if the tx was already in the mempool
            if "allowed" in testres_tx and testres_tx["allowed"]:
                assert_equal(submitres_tx["vsize"], testres_tx["vsize"])
                assert_equal(submitres_tx["fees"]["base"], testres_tx["fees"]["base"])
            entry_info = node.getmempoolentry(submitres_tx["txid"])
            assert_equal(submitres_tx["vsize"], entry_info["vsize"])
            assert_equal(submitres_tx["fees"]["base"], entry_info["fees"]["base"])

    def test_submit_child_with_parents(self, num_parents, partial_submit):
        node = self.nodes[0]
        peer = node.add_p2p_connection(P2PTxInvStore())

        package_txns = []
        presubmitted_wtxids = set()
        for _ in range(num_parents):
            parent_tx = self.wallet.create_self_transfer(fee=DEFAULT_FEE)
            package_txns.append(parent_tx)
            if partial_submit and random.choice([True, False]):
                node.sendrawtransaction(parent_tx["hex"])
                presubmitted_wtxids.add(parent_tx["wtxid"])
        child_tx = self.wallet.create_self_transfer_multi(utxos_to_spend=[tx["new_utxo"] for tx in package_txns], fee_per_output=10000) #DEFAULT_FEE
        package_txns.append(child_tx)

        testmempoolaccept_result = node.testmempoolaccept(rawtxs=[tx["hex"] for tx in package_txns])
        submitpackage_result = node.submitpackage(package=[tx["hex"] for tx in package_txns])

        # Check that each result is present, with the correct size and fees
        for package_txn in package_txns:
            tx = package_txn["tx"]
            assert tx.getwtxid() in submitpackage_result["tx-results"]
            wtxid = tx.getwtxid()
            assert wtxid in submitpackage_result["tx-results"]
            tx_result = submitpackage_result["tx-results"][wtxid]
            assert_equal(tx_result["txid"], tx.rehash())
            assert_equal(tx_result["vsize"], tx.get_vsize())
            assert_equal(tx_result["fees"]["base"], DEFAULT_FEE)
            if wtxid not in presubmitted_wtxids:
                assert_fee_amount(DEFAULT_FEE, tx.get_vsize(), tx_result["fees"]["effective-feerate"])
                assert_equal(tx_result["fees"]["effective-includes"], [wtxid])

        # submitpackage result should be consistent with testmempoolaccept and getmempoolentry
        self.assert_equal_package_results(node, testmempoolaccept_result, submitpackage_result)

        # The node should announce each transaction. No guarantees for propagation.
        peer.wait_for_broadcast([tx["tx"].getwtxid() for tx in package_txns])
        self.generate(node, 1)

    def test_submitpackage(self):
        node = self.nodes[0]

        self.log.info("Submitpackage valid packages with 1 child and some number of parents")
        for num_parents in [1, 2, 24]:
            self.test_submit_child_with_parents(num_parents, False)
            self.test_submit_child_with_parents(num_parents, True)

        self.log.info("Submitpackage only allows packages of 1 child with its parents")
        # Chain of 3 transactions has too many generations
        chain_hex = [t["hex"] for t in self.wallet.create_self_transfer_chain(chain_length=25)]
        assert_raises_rpc_error(-25, "package topology disallowed", node.submitpackage, chain_hex)


if __name__ == "__main__":
    RPCPackagesTest().main()