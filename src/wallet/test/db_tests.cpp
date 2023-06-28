// Copyright (c) 2018-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <fs.h>
#include <test/util/setup_common.h>
#include <wallet/bdb.h>

#include <fstream>
#include <memory>
#include <string>

namespace wallet {
BOOST_FIXTURE_TEST_SUITE(db_tests, BasicTestingSetup)

static std::shared_ptr<BerkeleyEnvironment> GetWalletEnv(const fs::path& path, fs::path& database_filename)
{
    fs::path data_file = BDBDataFile(path);
    database_filename = data_file.filename();
    return GetBerkeleyEnv(data_file.parent_path(), false);
}

BOOST_AUTO_TEST_CASE(getwalletenv_file)
{
    fs::path test_name = "test_name.dat";
    const fs::path datadir = m_args.GetDataDirNet();
    fs::path file_path = datadir / test_name;
    std::ofstream f{file_path};
    f.close();

    fs::path filename;
    std::shared_ptr<BerkeleyEnvironment> env = GetWalletEnv(file_path, filename);
    BOOST_CHECK_EQUAL(filename, test_name);
    BOOST_CHECK_EQUAL(env->Directory(), datadir);
}

BOOST_AUTO_TEST_CASE(getwalletenv_directory)
{
    fs::path expected_name = "wallet.dat";
    const fs::path datadir = m_args.GetDataDirNet();

    fs::path filename;
    std::shared_ptr<BerkeleyEnvironment> env = GetWalletEnv(datadir, filename);
    BOOST_CHECK_EQUAL(filename, expected_name);
    BOOST_CHECK_EQUAL(env->Directory(), datadir);
}

BOOST_AUTO_TEST_CASE(getwalletenv_g_dbenvs_multiple)
{
    fs::path datadir = m_args.GetDataDirNet() / "1";
    fs::path datadir_2 = m_args.GetDataDirNet() / "2";
    fs::path filename;

    std::shared_ptr<BerkeleyEnvironment> env_1 = GetWalletEnv(datadir, filename);
    std::shared_ptr<BerkeleyEnvironment> env_2 = GetWalletEnv(datadir, filename);
    std::shared_ptr<BerkeleyEnvironment> env_3 = GetWalletEnv(datadir_2, filename);

    BOOST_CHECK(env_1 == env_2);
    BOOST_CHECK(env_2 != env_3);
}

BOOST_AUTO_TEST_CASE(getwalletenv_g_dbenvs_free_instance)
{
    fs::path datadir = gArgs.GetDataDirNet() / "1";
    fs::path datadir_2 = gArgs.GetDataDirNet() / "2";
    fs::path filename;

    std::shared_ptr <BerkeleyEnvironment> env_1_a = GetWalletEnv(datadir, filename);
    std::shared_ptr <BerkeleyEnvironment> env_2_a = GetWalletEnv(datadir_2, filename);
    env_1_a.reset();

    std::shared_ptr<BerkeleyEnvironment> env_1_b = GetWalletEnv(datadir, filename);
    std::shared_ptr<BerkeleyEnvironment> env_2_b = GetWalletEnv(datadir_2, filename);

    BOOST_CHECK(env_1_a != env_1_b);
    BOOST_CHECK(env_2_a == env_2_b);
}

static std::vector<std::unique_ptr<WalletDatabase>> TestDatabases(const fs::path& path_root)
{
    std::vector<std::unique_ptr<WalletDatabase>> dbs;
    DatabaseOptions options;
    DatabaseStatus status;
    bilingual_str error;
#ifdef USE_BDB
    dbs.emplace_back(MakeBerkeleyDatabase(path_root / "bdb", options, status, error));
#endif
#ifdef USE_SQLITE
    dbs.emplace_back(MakeSQLiteDatabase(path_root / "sqlite", options, status, error));
#endif
    dbs.emplace_back(CreateMockableWalletDatabase());
    return dbs;
}

BOOST_AUTO_TEST_CASE(db_cursor_prefix_range_test)
{
    // Test each supported db
    for (const auto& database : TestDatabases(m_path_root)) {
        std::vector<std::string> prefixes = {"", "FIRST", "SECOND", "P\xfe\xff", "P\xff\x01", "\xff\xff"};

        // Write elements to it
        std::unique_ptr<DatabaseBatch> handler = Assert(database)->MakeBatch();
        for (unsigned int i = 0; i < 10; i++) {
            for (const auto& prefix : prefixes) {
                BOOST_CHECK(handler->Write(std::make_pair(prefix, i), i));
            }
        }

        // Now read all the items by prefix and verify that each element gets parsed correctly
        for (const auto& prefix : prefixes) {
            DataStream s_prefix;
            s_prefix << prefix;
            std::unique_ptr<DatabaseCursor> cursor = handler->GetNewPrefixCursor(s_prefix);
            DataStream key;
            DataStream value;
            for (int i = 0; i < 10; i++) {
                DatabaseCursor::Status status = cursor->Next(key, value);
                BOOST_CHECK_EQUAL(status, DatabaseCursor::Status::MORE);

                std::string key_back;
                unsigned int i_back;
                key >> key_back >> i_back;
                BOOST_CHECK_EQUAL(key_back, prefix);

                unsigned int value_back;
                value >> value_back;
                BOOST_CHECK_EQUAL(value_back, i_back);
            }

            // Let's now read it once more, it should return DONE
            BOOST_CHECK(cursor->Next(key, value) == DatabaseCursor::Status::DONE);
        }
    }
}

// Lower level DatabaseBase::GetNewPrefixCursor test, to cover cases that aren't
// covered in the higher level test above. The higher level test uses
// serialized strings which are prefixed with string length, so it doesn't test
// truly empty prefixes or prefixes that begin with \xff
BOOST_AUTO_TEST_CASE(db_cursor_prefix_byte_test)
{
    const MockableData::value_type
        e{StringData(""), StringData("e")},
        p{StringData("prefix"), StringData("p")},
        ps{StringData("prefixsuffix"), StringData("ps")},
        f{StringData("\xff"), StringData("f")},
        fs{StringData("\xffsuffix"), StringData("fs")},
        ff{StringData("\xff\xff"), StringData("ff")},
        ffs{StringData("\xff\xffsuffix"), StringData("ffs")};
    for (const auto& database : TestDatabases(m_path_root)) {
        std::unique_ptr<DatabaseBatch> batch = database->MakeBatch();
        for (const auto& [k, v] : {e, p, ps, f, fs, ff, ffs}) {
            batch->Write(Span{k}, Span{v});
        }
        CheckPrefix(*batch, StringBytes(""), {e, p, ps, f, fs, ff, ffs});
        CheckPrefix(*batch, StringBytes("prefix"), {p, ps});
        CheckPrefix(*batch, StringBytes("\xff"), {f, fs, ff, ffs});
        CheckPrefix(*batch, StringBytes("\xff\xff"), {ff, ffs});
    }
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet
