// Copyright (c) 2019-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
#include <test/util/setup_common.h>
#include <util/system.h>
#include <univalue.h>

#ifdef HAVE_BOOST_PROCESS
#include <boost/process.hpp>
#endif // HAVE_BOOST_PROCESS

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(system_tests, BasicTestingSetup)

// At least one test is required (in case HAVE_BOOST_PROCESS is not defined).
// Workaround for https://github.com/bitcoin/bitcoin/issues/19128
BOOST_AUTO_TEST_CASE(dummy)
{
    BOOST_CHECK(true);
}

#ifdef HAVE_BOOST_PROCESS

BOOST_AUTO_TEST_CASE(run_command)
{
    {
        const UniValue result = RunCommandParseJSON("");
        BOOST_CHECK(result.isNull());
    }
    {
        const UniValue result = RunCommandParseJSON("echo \"{\"success\": true}\"");
        BOOST_CHECK(result.isObject());
        const UniValue& success = result.find_value("success");
        BOOST_CHECK(!success.isNull());
        BOOST_CHECK_EQUAL(success.getBool(), true);
    }
    {
        // An invalid command is handled by Boost
<<<<<<< HEAD
=======
        const int expected_error{2};
>>>>>>> dfbad09c60 (Merge bitcoin/bitcoin#29489: test: Remove Windows-specific code from `system_tests/run_command`)
        BOOST_CHECK_EXCEPTION(RunCommandParseJSON("invalid_command"), boost::process::process_error, [&](const boost::process::process_error& e) {
            BOOST_CHECK(std::string(e.what()).find("RunCommandParseJSON error:") == std::string::npos);
            BOOST_CHECK_EQUAL(e.code().value(), 2);
            return true;
        });
    }
    {
        // Return non-zero exit code, no output to stderr
<<<<<<< HEAD
#ifdef WIN32
        const std::string command{"cmd.exe /c call"};
#else
=======
>>>>>>> dfbad09c60 (Merge bitcoin/bitcoin#29489: test: Remove Windows-specific code from `system_tests/run_command`)
        const std::string command{"false"};
        BOOST_CHECK_EXCEPTION(RunCommandParseJSON(command), std::runtime_error, [&](const std::runtime_error& e) {
            BOOST_CHECK(std::string(e.what()).find(strprintf("RunCommandParseJSON error: process(%s) returned 1: \n", command)) != std::string::npos);
            return true;
        });
    }
    {
        // Return non-zero exit code, with error message for stderr
<<<<<<< HEAD
#ifdef WIN32
        const std::string command{"cmd.exe /c dir nosuchfile"};
        const std::string expected{"File Not Found"};
#else
=======
>>>>>>> dfbad09c60 (Merge bitcoin/bitcoin#29489: test: Remove Windows-specific code from `system_tests/run_command`)
        const std::string command{"ls nosuchfile"};
        const std::string expected{"No such file or directory"};
        BOOST_CHECK_EXCEPTION(RunCommandParseJSON(command), std::runtime_error, [&](const std::runtime_error& e) {
            const std::string what(e.what());
            BOOST_CHECK(what.find(strprintf("RunCommandParseJSON error: process(%s) returned", command)) != std::string::npos);
            BOOST_CHECK(what.find(expected) != std::string::npos);
            return true;
        });
    }
    {
        // Unable to parse JSON
        const std::string command{"echo {"};
        BOOST_CHECK_EXCEPTION(RunCommandParseJSON(command), std::runtime_error, HasReason("Unable to parse JSON: {"));
    }
    // Test std::in
    {
        const UniValue result = RunCommandParseJSON("cat", "{\"success\": true}");
        BOOST_CHECK(result.isObject());
        const UniValue& success = result.find_value("success");
        BOOST_CHECK(!success.isNull());
        BOOST_CHECK_EQUAL(success.getBool(), true);
    }
}
#endif // HAVE_BOOST_PROCESS

BOOST_AUTO_TEST_SUITE_END()
