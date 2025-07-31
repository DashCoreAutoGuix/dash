#!/usr/bin/env python3
import re

with open('src/validation.cpp', 'r') as f:
    content = f.read()

# Fix the first conflict (line 1913)
content = content.replace(
    """<<<<<<< ours
    if (!UndoReadFromDisk(blockUndo, pindex)) {
        error("DisconnectBlock(): failure reading undo data");
=======
    if (!m_blockman.UndoReadFromDisk(blockUndo, *pindex)) {
        LogError("DisconnectBlock(): failure reading undo data\\n");
>>>>>>> theirs""",
    """    if (!UndoReadFromDisk(blockUndo, pindex)) {
        LogError("DisconnectBlock(): failure reading undo data\\n");""")

# Fix the conflict around line 2441
content = content.replace(
    """<<<<<<< ours
                LogError("ConnectBlock(): CheckInputScripts on %s failed with %s\\n",
                    tx.GetHash().ToString(), state.ToString());
                return false;
            }
            control.Add(vChecks);
        }

        if (fAddressIndex) {
            int64_t nTime2_index2 = GetTimeMicros();
            for (unsigned int k = 0; k < tx.vout.size(); k++) {
                const CTxOut &out = tx.vout[k];

                AddressType address_type{AddressType::UNKNOWN};
                uint160 address_bytes;

                if (!AddressBytesFromScript(out.scriptPubKey, address_type, address_bytes)) {
                    continue;
                }

                // record receiving activity
                addressIndex.push_back(std::make_pair(CAddressIndexKey(address_type, address_bytes, pindex->nHeight, i, txhash, k, false), out.nValue));

                // record unspent output
                addressUnspentIndex.push_back(std::make_pair(CAddressUnspentKey(address_type, address_bytes, txhash, k), CAddressUnspentValue(out.nValue, out.scriptPubKey, pindex->nHeight)));
=======
                LogError("ConnectBlock(): CheckInputScripts on %s failed with %s\\n",
                    tx.GetHash().ToString(), state.ToString());
                return false;
>>>>>>> theirs""",
    """                LogError("ConnectBlock(): CheckInputScripts on %s failed with %s\\n",
                    tx.GetHash().ToString(), state.ToString());
                return false;
            }
            control.Add(vChecks);
        }

        if (fAddressIndex) {
            int64_t nTime2_index2 = GetTimeMicros();
            for (unsigned int k = 0; k < tx.vout.size(); k++) {
                const CTxOut &out = tx.vout[k];

                AddressType address_type{AddressType::UNKNOWN};
                uint160 address_bytes;

                if (!AddressBytesFromScript(out.scriptPubKey, address_type, address_bytes)) {
                    continue;
                }

                // record receiving activity
                addressIndex.push_back(std::make_pair(CAddressIndexKey(address_type, address_bytes, pindex->nHeight, i, txhash, k, false), out.nValue));

                // record unspent output
                addressUnspentIndex.push_back(std::make_pair(CAddressUnspentKey(address_type, address_bytes, txhash, k), CAddressUnspentValue(out.nValue, out.scriptPubKey, pindex->nHeight)));""")

# Fix the DisconnectTip conflict
content = content.replace(
    """<<<<<<< ours
    if (!ReadBlockFromDisk(block, pindexDelete, m_params.GetConsensus())) {
        return error("DisconnectTip(): Failed to read block");
=======
    if (!m_blockman.ReadBlockFromDisk(block, *pindexDelete)) {
        LogError("DisconnectTip(): Failed to read block\\n");
        return false;
>>>>>>> theirs""",
    """    if (!ReadBlockFromDisk(block, pindexDelete, m_params.GetConsensus())) {
        LogError("DisconnectTip(): Failed to read block\\n");
        return false;""")

# Replace error() calls with LogError
content = re.sub(r'return error\("([^"]+)"\)', r'LogError("\1\\n");\n        return false', content)
content = re.sub(r'        error\("([^"]+)"\);', r'        LogError("\1\\n");', content)
content = re.sub(r'return error\("([^"]+)", ([^)]+)\)', r'LogError("\1\\n", \2);\n        return false', content)

# Remove any remaining conflict markers  
content = re.sub(r'<<<<<<< ours.*?>>>>>>> theirs\n', '', content, flags=re.DOTALL)

with open('src/validation.cpp', 'w') as f:
    f.write(content)