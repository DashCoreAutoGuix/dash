// Copyright (c) 2018-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <fs.h>
#include <test/util/setup_common.h>
#include <wallet/bdb.h>

#ifdef USE_SQLITE
#include <wallet/sqlite.h>
#endif

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

#ifdef USE_SQLITE

// Test-only statement execution error
constexpr int TEST_SQLITE_ERROR = -999;

class DbExecBlocker : public SQliteExecHandler
{
private:
    SQliteExecHandler m_base_exec;
    std::set<std::string> m_blocked_statements;
public:
    DbExecBlocker(std::set<std::string> blocked_statements) : m_blocked_statements(blocked_statements) {}
    int Exec(SQLiteDatabase& database, const std::string& statement) override {
        if (m_blocked_statements.contains(statement)) return TEST_SQLITE_ERROR;
        return m_base_exec.Exec(database, statement);
    }
};

BOOST_AUTO_TEST_CASE(txn_close_failure_dangling_txn)
{
    // Verifies that there is no active dangling, to-be-reversed db txn
    // after the batch object that initiated it is destroyed.
    DatabaseOptions options;
    DatabaseStatus status;
    bilingual_str error;
    std::unique_ptr<SQLiteDatabase> database = MakeSQLiteDatabase(m_path_root / "sqlite", options, status, error);

    std::string key = "key";
    std::string value = "value";

    std::unique_ptr<SQLiteBatch> batch = std::make_unique<SQLiteBatch>(*database);
    BOOST_CHECK(batch->TxnBegin());
    BOOST_CHECK(batch->Write(key, value));
    // Set a handler to prevent txn abortion during destruction.
    // Mimicking a db statement execution failure.
    batch->SetExecHandler(std::make_unique<DbExecBlocker>(std::set<std::string>{"ROLLBACK TRANSACTION"}));
    // Destroy batch
    batch.reset();

    // Ensure there is no dangling, to-be-reversed db txn
    BOOST_CHECK(!database->HasActiveTxn());

    // And, just as a sanity check; verify that new batchs only write what they suppose to write
    // and nothing else.
    std::string key2 = "key2";
    std::unique_ptr<SQLiteBatch> batch2 = std::make_unique<SQLiteBatch>(*database);
    BOOST_CHECK(batch2->Write(key2, value));
    // The first key must not exist
    BOOST_CHECK(!batch2->Exists(key));
}

#endif // USE_SQLITE

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet