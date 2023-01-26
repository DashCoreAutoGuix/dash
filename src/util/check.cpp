// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/check.h>

#include <tinyformat.h>

#include <cstdio>
#include <cstdlib>
#include <string_view>

void assertion_fail(std::string_view file, int line, std::string_view func, std::string_view assertion)
{
    auto str = strprintf("%s:%s %s: Assertion `%s' failed.\n", file, line, func, assertion);
    fwrite(str.data(), 1, str.size(), stderr);
    std::abort();
}
