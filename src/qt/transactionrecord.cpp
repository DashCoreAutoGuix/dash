// Copyright (c) 2011-2021 The Bitcoin Core developers
// Copyright (c) 2014-2024 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactionrecord.h>

#include <chain.h>
#include <interfaces/wallet.h>
#include <interfaces/node.h>

#include <wallet/ismine.h>

#include <stdint.h>

#include <QDateTime>

using wallet::ISMINE_SPENDABLE;
using wallet::ISMINE_WATCH_ONLY;
using wallet::isminetype;

/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction()
{
    // There are currently no cases where we hide transactions, but
    // we may want to use this in the future for things like RBF.
    return true;
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TransactionRecord> TransactionRecord::decomposeTransaction(interfaces::Node& node, interfaces::Wallet& wallet, const interfaces::WalletTx& wtx)
{
    QList<TransactionRecord> parts;
    int64_t nTime = wtx.time;
    CAmount nCredit = wtx.credit;
    CAmount nDebit = wtx.debit;
    CAmount nNet = nCredit - nDebit;
    uint256 hash = wtx.tx->GetHash();
    std::map<std::string, std::string> mapValue = wtx.value_map;
    auto& coinJoinOptions = node.coinJoinOptions();

    bool involvesWatchAddress = false;
    isminetype fAllFromMe = ISMINE_SPENDABLE;
    bool any_from_me = false;
    if (wtx.is_coinbase || wtx.is_platform_transfer) {
        fAllFromMe = isminetype(0);
    } else {
        for (const isminetype mine : wtx.txin_is_mine)
        {
            if(mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
            if(fAllFromMe > mine) fAllFromMe = mine;
            if (mine) any_from_me = true;
        }
    }

    // Handle CoinJoin denominate transactions separately
    if(wtx.is_denominate) {
        parts.append(TransactionRecord(hash, nTime, TransactionRecord::CoinJoinMixing, "", -nDebit, nCredit));
        parts.last().involvesWatchAddress = false;   // maybe pass to TransactionRecord as constructor argument
        return parts;
    }

    if (fAllFromMe || !any_from_me) {
        for (const isminetype mine : wtx.txout_is_mine)
        {
            if(mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
        }

        CAmount nTxFee = nDebit - wtx.tx->GetValueOut();
        for(unsigned int i = 0; i < wtx.tx->vout.size(); i++)
        {
            const CTxOut& txout = wtx.tx->vout[i];

            if (fAllFromMe) {
                // Change is only really possible if we're the sender
                // Otherwise, someone just sent bitcoins to a change address, which should be shown
                if (wtx.txout_is_change[i]) {
                    continue;
                }

                //
                // Debit
                //

                TransactionRecord sub(hash, nTime);
                sub.idx = i;
                sub.involvesWatchAddress = involvesWatchAddress;

                if (!std::get_if<CNoDestination>(&wtx.txout_address[i]))
                {
                    // Sent to Dash Address
                    sub.type = TransactionRecord::SendToAddress;
                    sub.strAddress = EncodeDestination(wtx.txout_address[i]);
                    sub.txDest = wtx.txout_address[i];
                    sub.updateLabel(wallet);
                }
                else
                {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.type = TransactionRecord::SendToOther;
                    sub.strAddress = mapValue["to"];
                    sub.txDest = DecodeDestination(sub.strAddress);
                }

                if(mapValue["DS"] == "1")
                {
                    sub.type = TransactionRecord::CoinJoinSend;
                }

                CAmount nValue = txout.nValue;
                /* Add fee to first output */
                if (nTxFee > 0)
                {
                    nValue += nTxFee;
                    nTxFee = 0;
                }
                sub.debit = -nValue;

                parts.append(sub);
            }

            isminetype mine = wtx.txout_is_mine[i];
            if(mine)
            {
                //
                // Credit
                //

                TransactionRecord sub(hash, nTime);
                sub.idx = i; // vout index
                sub.credit = txout.nValue;
                sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                if (wtx.txout_address_is_mine[i])
                {
                    // Received by Dash Address
                    sub.type = TransactionRecord::RecvWithAddress;
                    sub.strAddress = EncodeDestination(wtx.txout_address[i]);
                    sub.txDest = wtx.txout_address[i];
                    sub.updateLabel(wallet);
                }
                else
                {
                    // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                    sub.type = TransactionRecord::RecvFromOther;
                    sub.strAddress = mapValue["from"];
                    sub.txDest = DecodeDestination(sub.strAddress);
                }
                if (wtx.is_coinbase)
                {
                    // Generated
                    sub.type = TransactionRecord::Generated;
                }
                if (wtx.is_platform_transfer)
                {
                    // Withdrawal from platform
                    sub.type = TransactionRecord::PlatformTransfer;
                }

                parts.append(sub);
            }
        }

        // Handle special Dash-specific CoinJoin transactions that are all from me
        if (fAllFromMe && nCredit > 0) {
            // Check for specific CoinJoin transaction types
            bool fHandled = false;
            
            // CoinJoin collateral payment
            if(wtx.tx->vin.size() == 1 && wtx.tx->vout.size() == 1
                && coinJoinOptions.isCollateralAmount(nDebit)
                && coinJoinOptions.isCollateralAmount(nCredit)
                && coinJoinOptions.isCollateralAmount(-nNet))
            {
                TransactionRecord sub(hash, nTime);
                sub.type = TransactionRecord::CoinJoinCollateralPayment;
                sub.debit = -(nDebit - wtx.change);
                sub.credit = nCredit - wtx.change;
                sub.involvesWatchAddress = involvesWatchAddress;
                parts.append(sub);
                fHandled = true;
            }
            // Check for CoinJoin make collaterals or create denominations
            else if (!fHandled) {
                bool fMakeCollateral = false;
                bool fCreateDenominations = false;
                
                if (wtx.tx->vout.size() == 2) {
                    CAmount nAmount0 = wtx.tx->vout[0].nValue;
                    CAmount nAmount1 = wtx.tx->vout[1].nValue;
                    // <case1>, see CCoinJoinClientSession::MakeCollateralAmounts
                    fMakeCollateral = (nAmount0 == coinJoinOptions.getMaxCollateralAmount() && !coinJoinOptions.isDenominated(nAmount1) && nAmount1 >= coinJoinOptions.getMinCollateralAmount()) ||
                                      (nAmount1 == coinJoinOptions.getMaxCollateralAmount() && !coinJoinOptions.isDenominated(nAmount0) && nAmount0 >= coinJoinOptions.getMinCollateralAmount()) ||
                    // <case2>, see CCoinJoinClientSession::MakeCollateralAmounts
                                      (nAmount0 == nAmount1 && coinJoinOptions.isCollateralAmount(nAmount0));
                } else if (wtx.tx->vout.size() == 1) {
                    // <case3>, see CCoinJoinClientSession::MakeCollateralAmounts
                    fMakeCollateral = coinJoinOptions.isCollateralAmount(wtx.tx->vout[0].nValue);
                }
                
                if (!fMakeCollateral) {
                    for (const auto& txout : wtx.tx->vout) {
                        if (coinJoinOptions.isDenominated(txout.nValue)) {
                            fCreateDenominations = true;
                            break;
                        }
                    }
                }
                
                if (fMakeCollateral || fCreateDenominations) {
                    TransactionRecord sub(hash, nTime);
                    sub.type = fMakeCollateral ? TransactionRecord::CoinJoinMakeCollaterals : TransactionRecord::CoinJoinCreateDenominations;
                    sub.debit = -(nDebit - wtx.change);
                    sub.credit = nCredit - wtx.change;
                    sub.involvesWatchAddress = involvesWatchAddress;
                    parts.append(sub);
                    fHandled = true;
                }
            }
            
            if (fHandled) {
                return parts;
            }
        }
    } else {
        //
        // Mixed debit transaction, can't break down payees
        //
        parts.append(TransactionRecord(hash, nTime, TransactionRecord::Other, "", nNet, 0));
        parts.last().involvesWatchAddress = involvesWatchAddress;
    }

    return parts;
}

void TransactionRecord::updateStatus(const interfaces::WalletTxStatus& wtx, const uint256& block_hash, int numBlocks, int chainLockHeight, int64_t block_time)
{
    // Determine transaction status

    // Sort order, unrecorded transactions sort to the top
    int typesort;
    switch (type) {
    case SendToAddress: case SendToOther:
        typesort = 2; break;
    case RecvWithAddress: case RecvFromOther:
        typesort = 3; break;
    default:
        typesort = 9;
    }
    status.sortKey = strprintf("%010d-%01d-%010u-%03d-%d",
        wtx.block_height,
        wtx.is_coinbase ? 1 : 0,
        wtx.time_received,
        idx,
        typesort);
    status.countsForBalance = wtx.is_trusted && !(wtx.blocks_to_maturity > 0);
    status.depth = wtx.depth_in_main_chain;
    status.m_cur_block_hash = block_hash;
    status.cachedChainLockHeight = chainLockHeight;
    status.lockedByChainLocks = wtx.is_chainlocked;
    status.lockedByInstantSend = wtx.is_islocked;

    // For generated transactions, determine maturity
    if (type == TransactionRecord::Generated) {
        if (wtx.blocks_to_maturity > 0)
        {
            status.status = TransactionStatus::Immature;

            if (wtx.is_in_main_chain)
            {
                status.matures_in = wtx.blocks_to_maturity;
            }
            else
            {
                status.status = TransactionStatus::NotAccepted;
            }
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    else
    {
        if (status.depth < 0)
        {
            status.status = TransactionStatus::Conflicted;
        }
        else if (status.depth == 0)
        {
            status.status = TransactionStatus::Unconfirmed;
            if (wtx.is_abandoned)
                status.status = TransactionStatus::Abandoned;
        }
        else if (status.depth < RecommendedNumConfirmations && !status.lockedByChainLocks)
        {
            status.status = TransactionStatus::Confirming;
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    status.needsUpdate = false;
}

bool TransactionRecord::statusUpdateNeeded(const uint256& block_hash, int chainLockHeight) const
{
    assert(!block_hash.IsNull());
    return status.m_cur_block_hash != block_hash || status.needsUpdate
        || (!status.lockedByChainLocks && status.cachedChainLockHeight != chainLockHeight);
}

void TransactionRecord::updateLabel(interfaces::Wallet& wallet)
{
    if (IsValidDestination(txDest)) {
        std::string name;
        if (wallet.getAddress(txDest, &name, /* is_mine= */ nullptr, /* purpose= */ nullptr)) {
            label = QString::fromStdString(name);
        } else {
            label = "";
        }
    }
}

QString TransactionRecord::getTxHash() const
{
    return QString::fromStdString(hash.ToString());
}

int TransactionRecord::getOutputIndex() const
{
    return idx;
}
