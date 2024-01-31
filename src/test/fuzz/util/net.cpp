// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <compat/compat.h>
#include <netaddress.h>
#include <random.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/util/net.h>
#include <util/strencodings.h>

#include <cstdint>
#include <vector>

class CNode;

CNetAddr ConsumeNetAddr(FuzzedDataProvider& fuzzed_data_provider, FastRandomContext* rand) noexcept
{
    struct NetAux {
        Network net;
        CNetAddr::BIP155Network bip155;
        size_t len;
    };

    static constexpr std::array<NetAux, 6> nets{
        NetAux{.net = Network::NET_IPV4, .bip155 = CNetAddr::BIP155Network::IPV4, .len = ADDR_IPV4_SIZE},
        NetAux{.net = Network::NET_IPV6, .bip155 = CNetAddr::BIP155Network::IPV6, .len = ADDR_IPV6_SIZE},
        NetAux{.net = Network::NET_ONION, .bip155 = CNetAddr::BIP155Network::TORV3, .len = ADDR_TORV3_SIZE},
        NetAux{.net = Network::NET_I2P, .bip155 = CNetAddr::BIP155Network::I2P, .len = ADDR_I2P_SIZE},
        NetAux{.net = Network::NET_CJDNS, .bip155 = CNetAddr::BIP155Network::CJDNS, .len = ADDR_CJDNS_SIZE},
        NetAux{.net = Network::NET_INTERNAL, .bip155 = CNetAddr::BIP155Network{0}, .len = 0},
    };

    const size_t nets_index{rand == nullptr
        ? fuzzed_data_provider.ConsumeIntegralInRange<size_t>(0, nets.size() - 1)
        : static_cast<size_t>(rand->randrange(nets.size()))};

    const auto& aux = nets[nets_index];

    CNetAddr addr;

    if (aux.net == Network::NET_INTERNAL) {
        if (rand == nullptr) {
            addr.SetInternal(fuzzed_data_provider.ConsumeBytesAsString(32));
        } else {
            const auto v = rand->randbytes(32);
            addr.SetInternal(std::string{v.begin(), v.end()});
        }
        return addr;
    }

    DataStream s;

    s << static_cast<uint8_t>(aux.bip155);

    std::vector<uint8_t> addr_bytes;
    if (rand == nullptr) {
        addr_bytes = fuzzed_data_provider.ConsumeBytes<uint8_t>(aux.len);
        addr_bytes.resize(aux.len);
    } else {
        addr_bytes = rand->randbytes(aux.len);
    }
    if (aux.net == NET_IPV6 && addr_bytes[0] == CJDNS_PREFIX) { // Avoid generating IPv6 addresses that look like CJDNS.
        addr_bytes[0] = 0x55; // Just an arbitrary number, anything != CJDNS_PREFIX would do.
    }
    if (aux.net == NET_CJDNS) { // Avoid generating CJDNS addresses that don't start with CJDNS_PREFIX because those are !IsValid().
        addr_bytes[0] = CJDNS_PREFIX;
    }
    s << addr_bytes;

    s >> CAddress::V2_NETWORK(addr);

    return addr;
}
