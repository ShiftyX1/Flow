# Break compatibility with prototype saves

The first gameplay MVP will introduce a new save schema without migrating prototype saves, because slot-based `item_id` inventories, weight, knowledge ownership, Functional Blocks, Power Networks, machines, Recovery Caches, and beacon state cannot be represented safely by the existing `block_id`-count metadata. Older worlds must be reported as incompatible rather than silently loaded, converted, reset, or deleted.
