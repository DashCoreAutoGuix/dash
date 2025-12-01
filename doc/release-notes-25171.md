Updated RPCs
------------

- The `deprecatedrpc=exclude_coinbase` configuration option has been removed.
  The `receivedby` RPCs (`listreceivedbyaddress`, `listreceivedbylabel`,
  `getreceivedbyaddress` and `getreceivedbylabel`) now always return results
  accounting for received coins from coinbase outputs, without an option to
  change that behaviour. Excluding coinbases was previously deprecated in 23.0.
