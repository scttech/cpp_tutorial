# Order Book Matching Engine

> **You're an engineer at an exchange.** Traders send orders: "buy 100 shares at no more than $50.10", "sell 40 at no less than $50.20". The matching engine pairs them according to strict rules and must be fast, fair and *exactly* reproducible, because every trade is audited. This is the heart of every stock, crypto and futures exchange.

## The rules (price-time priority)

An **order book** holds the *resting* orders that have not found a counterparty yet: **bids** (buyers) and **asks** (sellers). A new **limit order** first tries to trade:

- A **buy** trades against resting **asks** priced at or below its limit, **lowest price first**; a **sell** trades against **bids** at or above its limit, **highest price first**. Equal prices trade.
- At the same price, the order that arrived **first** trades first (time priority). A partly filled order keeps its place in the queue.
- Every execution is a `Trade` at the **resting order's price** (the taker may get a better price than it asked), for `min(remaining incoming, remaining resting)`.
- What is left of the incoming order **rests** at its own limit price, at the *back* of the queue for that price.
- After any submit, `best_bid < best_ask`: the book is never crossed.
- Rejected, changing nothing: quantity or price ≤ 0, or an id that is already resting. An id may be reused once its order is gone.

`cancel(id)` removes a resting order and **must not search the book**; levels that become empty must disappear (so `best_ask()` is right).

## The data structure

```
asks:  102 -> [#9:5]            std::map<Price, std::list<Order>>   (ordered by price, best level first)
       101 -> [#4:10, #7:3]     the list is the queue at that price
bids:  100 -> [#2:8]            asks sorted ascending, bids descending (std::greater)
        99 -> [#5:20, #6:1]
index: id -> (side, price, list iterator)    std::unordered_map: cancel in O(1) average
```

`std::list` iterators stay valid when other elements are inserted or erased, which is what makes the id index possible. `Price` and `OrderId` count their comparisons, so the Fast tests can tell a book that finds the best level at once from one that scans, and a cancel that uses the index from one that searches.

## The tiers

- **Works**: resting and crossing, price and time priority, the trade price, partial fills of both orders, sweeping levels, cancel, rejections.
- **Robust**: ids reused after fill or cancel, no empty levels or stale index entries, cancel in the middle of a queue, never crossed, and a differential test: 6000 random submits and cancels give exactly the same trades, statuses, depths and best prices as a slow scanning reference book. Quantities near `INT64_MAX`.
- **Fast**: at most 30 price comparisons per resting order over 1000 levels, matching looks only at the best level, a cancel costs at most 6 id comparisons, orders are moved not copied, 400000 operations on a 100000-order book in time.
- **Clean**: no raw memory or exceptions, an ordered container and no re-sorting, an id index.

## Tips

Write one `match` function as a template over the book type (`std::map<Price, Level>` for asks, `std::map<Price, Level, std::greater<Price>>` for bids): both sides then share the code, and `book.begin()` is always the best level. After a maker is filled completely remember to erase it from the **index** and the level, and to erase an emptied **level** from the map.
