#\!/usr/bin/env python3
# Copyright (c) 2017-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test various command line arguments and configuration file parameters."""

import os
import platform
import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework import util


class ConfArgsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.supports_cli = False
        self.wallet_names = []

    def test_config_file_parser(self):
        self.log.info('Test config file parser')
        self.stop_node(0)

        # Check that startup fails if conf= is set in bitcoin.conf or in an included conf file
        bad_conf_file_path = os.path.join(self.options.tmpdir, 'node0', 'bitcoin_bad.conf')
        util.write_config(bad_conf_file_path, n=0, chain='', extra_config=f'conf=some.conf\n')
        conf_in_config_file_err = 'Error: Error reading configuration file: conf cannot be set in the configuration file; use includeconf= if you want to include additional config files'
        self.nodes[0].assert_start_raises_init_error(
            extra_args=[f'-conf={bad_conf_file_path}'],
            expected_msg=conf_in_config_file_err,
        )
        inc_conf_file_path = os.path.join(self.nodes[0].datadir, 'include.conf')
        with open(os.path.join(self.nodes[0].datadir, 'dash.conf'), 'a', encoding='utf-8') as conf:
            conf.write(f'includeconf={inc_conf_file_path}\n')
        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('conf=some.conf\n')
        self.nodes[0].assert_start_raises_init_error(
            expected_msg=conf_in_config_file_err,
        )

        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error: Error parsing command line arguments: Invalid parameter -dash_cli=1',
            extra_args=['-dash_cli=1'],
        )

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('')  # clear

        # Check that invalid configuration setting alerts the user
        inc_conf_file2_path = os.path.join(self.nodes[0].datadir, 'include2.conf')
        with open(os.path.join(self.nodes[0].datadir, 'dash.conf'), 'a', encoding='utf-8') as conf:
            conf.write(f'includeconf={inc_conf_file2_path}\n')
        with open(inc_conf_file2_path, 'w', encoding='utf-8') as conf:
            conf.write('-dash=1\n')
        self.nodes[0].assert_start_raises_init_error(expected_msg='Error: Error reading configuration file: parse error on line 1: -dash=1, options in configuration file must be specified without leading -')

        if self.is_wallet_compiled():
            with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
                conf.write("wallet=foo\n")
            self.nodes[0].assert_start_raises_init_error(expected_msg=f'Error: Config setting for -wallet only applied on {self.chain} network when in [{self.chain}] section.')

        main_conf_file_path = os.path.join(self.options.tmpdir, 'node0', 'dash_main.conf')
        util.write_config(main_conf_file_path, n=0, chain='main', extra_config=f'includeconf={inc_conf_file_path}\n')
        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write("acceptnonstdtxn=1\n")
        self.nodes[0].assert_start_raises_init_error(extra_args=[f"-conf={main_conf_file_path}", "-mainnet"], expected_msg="Error: acceptnonstdtxn is not currently supported for main chain")

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('')  # clear
        with open(inc_conf_file2_path, 'w', encoding='utf-8') as conf:
            conf.write('')  # clear

    def test_networkactive(self):
        self.log.info('Test -networkactive option')
        self.stop_node(0)
        with self.nodes[0].assert_debug_log(expected_msgs=['SetNetworkActive: true\n']):
            self.start_node(0)
        self.stop_node(0)
        with self.nodes[0].assert_debug_log(expected_msgs=['SetNetworkActive: true\n']):
            self.start_node(0, extra_args=['-networkactive'])
        self.stop_node(0)
        with self.nodes[0].assert_debug_log(expected_msgs=['SetNetworkActive: true\n']):
            self.start_node(0, extra_args=['-networkactive=1'])
        self.stop_node(0)
        with self.nodes[0].assert_debug_log(expected_msgs=['SetNetworkActive: false\n']):
            self.start_node(0, extra_args=['-networkactive=0'])
        self.stop_node(0)
        with self.nodes[0].assert_debug_log(expected_msgs=['SetNetworkActive: false\n']):
            self.start_node(0, extra_args=['-nonetworkactive'])
        self.stop_node(0)
        with self.nodes[0].assert_debug_log(expected_msgs=['SetNetworkActive: false\n']):
            self.start_node(0, extra_args=['-nonetworkactive=1'])

    def test_seed_peers(self):
        self.log.info('Test seed peers')
        default_data_dir = self.nodes[0].datadir
        new_data_dir = os.path.join(default_data_dir, 'newdatadir')
        self.stop_node(0)

        # Test that setting -dnsseed=0 disables querying DNS seeds
        with self.nodes[0].assert_debug_log(expected_msgs=[], unexpected_msgs=["Loading addresses from DNS seed"]):
            self.start_node(0, extra_args=["-dnsseed=0", "-fixedseeds=0", f"-datadir={new_data_dir}"])
        self.stop_node(0)

        # Test that running dashd with -forcednsseed and -dnsseed=0/-nodnsseed
        # throws an error that cannot be set at the same time
        self.nodes[0].assert_start_raises_init_error(
            expected_msg="Error: Cannot set -forcednsseed to true when setting -dnsseed to false.",
            extra_args=["-forcednsseed=1", "-dnsseed=0"],
        )
        self.nodes[0].assert_start_raises_init_error(
            expected_msg="Error: Cannot set -forcednsseed to true when setting -dnsseed to false.",
            extra_args=["-forcednsseed=1", "-nodnsseed"],
        )

        # Test that running dashd with -forcednsseed and -fixedseeds=0/-nofixedseeds
        # throws an error that cannot be set at the same time
        self.nodes[0].assert_start_raises_init_error(
            expected_msg="Error: Cannot set -forcednsseed to true when setting -fixedseeds to false.",
            extra_args=["-forcednsseed=1", "-fixedseeds=0"],
        )
        self.nodes[0].assert_start_raises_init_error(
            expected_msg="Error: Cannot set -forcednsseed to true when setting -fixedseeds to false.",
            extra_args=["-forcednsseed=1", "-nofixedseeds"],
        )

        # Test that running dashd with -dnsseed=0 and -fixedseeds=0 and no
        # -addnode argument results in dashd agreeing that it will not connect
        # to any peers.
        with self.nodes[0].assert_debug_log(expected_msgs=[
            "dnsseed thread exit",
            "addcon thread exit",
        ]):
            self.start_node(0, extra_args=["-dnsseed=0", "-fixedseeds=0", "-connect=fakeaddress", f"-datadir={new_data_dir}"])
        self.stop_node(0)

        # No peers.dat exists and -dnsseed=1
        # We expect the node will query DNS seeds for peers
        with self.nodes[0].assert_debug_log(expected_msgs=[
            "Loading addresses from DNS seed",
        ]):
            self.start_node(0, extra_args=["-dnsseed=1", "-fixedseeds=0", f"-datadir={new_data_dir}"])
        self.stop_node(0)

        # No peers.dat exists and -dnsseed=0
        # We expect the node will use fixed seeds
        with self.nodes[0].assert_debug_log(expected_msgs=[
            "Added 1 fixed seeds from reachable networks.",
        ], timeout=10):
            self.start_node(0, extra_args=["-dnsseed=0", "-fixedseeds=1", f"-datadir={new_data_dir}"])
        self.stop_node(0)

        # No peers.dat exists and -dnsseed=0 and -fixedseeds=0
        # We expect the node will not add fixed seeds and dnsseed is skipped
        with self.nodes[0].assert_debug_log(expected_msgs=[
            "Fixed seeds are disabled",
            "DNS seeding disabled",
        ]):
            self.start_node(0, extra_args=["-dnsseed=0", "-fixedseeds=0", f"-datadir={new_data_dir}"])
        self.stop_node(0)

        # No peers.dat exists and -dnsseed=1 and -forcednsseed=1
        # We expect the node to log that it force queried DNS seeds with empty addrman
        with self.nodes[0].assert_debug_log(expected_msgs=[
            "Loading addresses from DNS seed",
            "Loaded 0 addresses from peers.dat",
            "Loaded 0 addresses from \"anchors.dat\"",
            "Forced DNS seed query on empty addrman",
        ]):
            self.start_node(0, extra_args=["-forcednsseed=1", "-dnsseed=1", "-fixedseeds=1", f"-datadir={new_data_dir}"])
        self.stop_node(0)

        # Restarting the node empties addrman
        with self.nodes[0].assert_debug_log(expected_msgs=[
            "Loaded 0 addresses from peers.dat",
            "Loaded 0 addresses from \"anchors.dat\"",
        ]):
            self.start_node(0, extra_args=["-dnsseed=0", "-fixedseeds=0", f"-datadir={new_data_dir}"])
        self.stop_node(0)

        # No peers.dat exists and -forcednsseed=1 and -fixedseeds=0
        # We expect the node to log that it force queried DNS seeds with empty addrman
        with self.nodes[0].assert_debug_log(expected_msgs=[
            "Loading addresses from DNS seed",
            "Fixed seeds are disabled",
            "Forced DNS seed query on empty addrman",
        ]):
            self.start_node(0, extra_args=["-forcednsseed=1", "-dnsseed=1", "-fixedseeds=0", f"-datadir={new_data_dir}"])
        self.stop_node(0)

        # an address via -seednode sources should be equivalent to seeds via dsnsseed
        # so should prevent a forcednseed from executing
        with self.nodes[0].assert_debug_log(expected_msgs=[
            "DNS seeding disabled",
            "Adding fixed seeds as -seednode(s) are specified",
            "Added 1 fixed seeds from reachable networks."
        ], unexpected_msgs=["Loading addresses from DNS seed"]):
            self.start_node(0, extra_args=["-seednode=127.0.0.1:1337", "-forcednsseed=1", "-dnsseed=0", "-fixedseeds=1", f"-datadir={new_data_dir}"])
        self.stop_node(0)

        os.rmdir(new_data_dir)

    def run_test(self):
        self.test_log_buffer()
        self.test_args_log()
        self.test_seed_peers()
        self.test_networkactive()

        self.test_config_file_parser()

        # Remove the -datadir argument so it doesn't override the config file
        self.nodes[0].args = [arg for arg in self.nodes[0].args if not arg.startswith("-datadir")]

        default_data_dir = self.nodes[0].datadir
        new_data_dir = os.path.join(default_data_dir, 'newdatadir')
        new_data_dir_2 = os.path.join(default_data_dir, 'newdatadir2')

        # Check that using -datadir argument on non-existent directory fails
        self.nodes[0].datadir = new_data_dir
        self.nodes[0].assert_start_raises_init_error([f'-datadir={new_data_dir}'], f'Error: Specified data directory "{new_data_dir}" does not exist.')

        # Check that using non-existent datadir in conf file fails
        conf_file = os.path.join(default_data_dir, "dash.conf")
        
        # datadir needs to be set before [chain] section
        with open(conf_file, encoding='utf8') as f:
            conf_file_contents = f.read()
        with open(conf_file, 'w', encoding='utf8') as f:
            f.write(f'datadir={new_data_dir}\n')
            f.write(conf_file_contents)

        self.nodes[0].assert_start_raises_init_error([f'-conf={conf_file}'], f'Error: Error reading configuration file: specified data directory "{new_data_dir}" does not exist.')

        # Check that an explicitly specified config file that cannot be opened fails
        none_existent_conf_file = os.path.join(default_data_dir, "none_existent_dash.conf")
        self.nodes[0].assert_start_raises_init_error(['-conf=' + none_existent_conf_file], 'Error: Error reading configuration file: specified config file "' + none_existent_conf_file + '" could not be opened.')

        # Create the directory and ensure the config file now works
        os.mkdir(new_data_dir)
        self.start_node(0, [f'-conf={conf_file}'])
        self.stop_node(0)
        assert os.path.exists(os.path.join(new_data_dir, self.chain, 'blocks'))

        # Create the directory and ensure the config file now works
        os.mkdir(new_data_dir_2)
        
        # datadir needs to be set before [chain] section
        with open(conf_file, encoding='utf8') as f:
            conf_file_contents = f.read()
        with open(conf_file, 'w', encoding='utf8') as f:
            f.write(f'datadir={new_data_dir_2}\n')
            f.write(conf_file_contents)
        
        self.start_node(0, [f'-conf={conf_file}'])
        assert os.path.exists(os.path.join(new_data_dir_2, self.chain, 'blocks'))

    def test_args_log(self):
        self.log.info('Test config args logging')
        self.stop_node(0)
        with self.nodes[0].assert_debug_log(
                expected_msgs=[
                    'Command-line arg: addnode="some.node"',
                    'Command-line arg: rpcauth=****',
                    'Command-line arg: rpcpassword=****',
                    'Command-line arg: rpcuser=****',
                    'Command-line arg: torpassword=****',
                ],
                unexpected_msgs=[
                    'alice:f7efda5c189b999524f151318c0c86$d5b51b3beffbc0',
                    'secret-rpcuser',
                    'secret-torpassword',
                ]
        ):
            self.start_node(0, extra_args=[
                '-addnode=some.node',
                '-rpcauth=alice:f7efda5c189b999524f151318c0c86$d5b51b3beffbc0',
                '-rpcpassword=',
                '-rpcuser=secret-rpcuser',
                '-torpassword=secret-torpassword',
            ])
        self.stop_node(0)

    def test_log_buffer(self):
        self.log.info('Test -logthreadnames')
        self.stop_node(0)
        with self.nodes[0].assert_debug_log(expected_msgs=['net thread start'], unexpected_msgs=['net thread start\n']):
            self.start_node(0, extra_args=["-logthreadnames=0"])
        self.stop_node(0)

        self.log.info(
            "Test -logfiletimestamps (disabled by default in regtest)")
        self.stop_node(0)
        with self.nodes[0].assert_debug_log(expected_msgs=['Flushed fee estimates to fee_estimates.dat.'],
                                            unexpected_msgs=['[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z Flushed fee estimates to fee_estimates.dat.']):
            with self.nodes[0].assert_debug_log(expected_msgs=['Flushed fee estimates to fee_estimates.dat.'],
                                                unexpected_msgs=['[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z Flushed fee estimates to fee_estimates.dat.']):
                self.start_node(0, extra_args=["-logfiletimestamps=0"])
                self.nodes[0].wait_until_stopped(expected_ret_code=1)
                # sanity check: -logfiletimestamps=0 should not have timestamp
                # in debug.log
        self.stop_node(0)

        self.log.info("Test -logfiletimestamps (enabled by default on mainnet)")
        with self.nodes[0].assert_debug_log(expected_msgs=['Flushed fee estimates to fee_estimates.dat.'],
                                            unexpected_msgs=['Flushed fee estimates to fee_estimates.dat.']):
            # sanity check: with -logfiletimestamps=1 we should have a timestamp with log message
            with self.nodes[0].assert_debug_log(expected_msgs=['[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z Flushed fee estimates to fee_estimates.dat.']):
                self.start_node(0, extra_args=["-logfiletimestamps=1", "-mainnet"])
                self.nodes[0].wait_until_stopped(expected_ret_code=1)
                # sanity check: with -logfiletimestamps=1 we should have a timestamp with log message
        self.stop_node(0)


if __name__ == '__main__':
    ConfArgsTest().main()
