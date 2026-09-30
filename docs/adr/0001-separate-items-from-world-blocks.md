# Separate inventory Items from world Blocks

Flow will identify inventory Items with stable `item_id` values and world Blocks with separate `block_id` values, because tools, equipment, components, and consumables must participate in the same inventory and crafting model without pretending to be placeable voxels. Placeable Items may reference the Block they create, but neither identifier substitutes for the other; this deliberately replaces the prototype's `block_id`-keyed inventory before more content depends on it.
