# Flow Game

Flow is a persistent planetary survival sandbox about an expedition establishing a lasting presence and optionally renewing the surrounding ecosystem.

## Language

**Base**:
A persistent, player-developed operational site on the planet. The expedition may establish multiple Bases.
_Avoid_: Camp, outpost, settlement

**Colony**:
The player-facing, fictional name for the primary Base built around the Landing Module. In the initial game scope it does not imply colonists, population simulation, or NPC workers.
_Avoid_: Using the term to imply residents

**Landing Module**:
The expedition's initial intact, immovable structure, serving as the player's arrival point, first storage, permanent Colony nucleus, and fallback respawn point even when other Bases exist.
_Avoid_: Escape pod, crash site

**Emergency Fabricator**:
The limited Fabricator and power supply built into the Landing Module that bootstrap the expedition's first independent machines.
_Avoid_: Full Fabricator, crafting table

**Freeform Construction**:
The construction language in which players shape Bases by placing individual Blocks without prescribed rooms, attachment points, recognised building layouts, or structural-support requirements.
_Avoid_: Prefab base, modular room system

**Functional Block**:
A placeable technological block that provides a specific Colony capability independently of the shape of the surrounding construction.
_Avoid_: Station, prefab module

**Power Network**:
A connected group of generators, cables, and consuming Functional Blocks. Operations start only when the network has enough free generation, pause without losing progress when supply disappears, and do not consume power while idle.
_Avoid_: Power radius, electrical simulation

**Solid-Fuel Generator**:
The expedition's first independent power source, consuming wood or the more efficient bio-resin to supply a Power Network.
_Avoid_: Free generator, reactor

**Sandbox Progression**:
Player-directed development without a required quest chain, checklist, or declared completion state. Industrial milestones remain development validation targets rather than player objectives.
_Avoid_: Main quest, guided progression

**Fabricator**:
A powered Functional Block that creates technological Items unavailable through Handcrafting.
_Avoid_: Crafting table, workbench

**Handcrafting**:
The small set of basic recipes available directly to the player without a Functional Block.
_Avoid_: Crafting grid

**Human Technology**:
The expedition's initial body of known recipes, representing established human knowledge available without planetary discoveries.
_Avoid_: Starter tier, primitive technology

**Recipe Discovery**:
The permanent addition of a recipe to the expedition's known technology through planetary material study or discoveries recovered from ruins.
_Avoid_: Experience unlock, level reward

**Material Analysis**:
The suit's study of a newly encountered planetary material, permanently adding its common applications to the player's known recipes.
_Avoid_: Research points, automatic technology tier

**Technology Schematic**:
A discovery recovered from ruins that permanently adds one specific advanced recipe to the player's known technology.
_Avoid_: Skill point, generic research item

**Suit Knowledge**:
The recipes and material discoveries stored by the player's suit and retained through death rather than placed in a Recovery Cache.
_Avoid_: Inventory knowledge, character experience

**Expedition Knowledge**:
Recipes and material discoveries shared at the expedition level. The initial data model distinguishes it from Suit Knowledge even though transferring knowledge between them is not yet a player-facing capability.
_Avoid_: Personal recipe list

**Item**:
An inventory entity identified by a stable `item_id`, including materials, components, equipment, tools, and objects that can place Blocks.
_Avoid_: Using `block_id` as inventory identity

**Block**:
A voxel state placed in the world and identified by `block_id`. A Block is not itself an inventory entity, although an Item may place one.
_Avoid_: Item, inventory block

**Suit Storage**:
The nine-slot inventory carried inside the expedition suit, separate from the nine-slot Hotbar and expandable through suit improvement.
_Avoid_: Backpack, main inventory

**Hotbar**:
The suit's nine immediately accessible Item slots. Together with Suit Storage, it forms the player's initial eighteen inventory slots.
_Avoid_: Equipment bar

**Carry Capacity**:
The total Item mass the suit can carry before overload, independent of available inventory slots. Overload slows movement and disables sprinting, while a lack of a compatible or empty slot prevents picking up a new Item.
_Avoid_: Slot count, stack limit

**Electric Furnace**:
A powered Functional Block that converts raw iron and copper ore into refined material through a single-job processing queue.
_Avoid_: Fabricator recipe, fuel furnace

**Material Progression**:
Progress expressed through acquired equipment, tools, portable capabilities, and Functional Blocks rather than character levels or skills.
_Avoid_: Experience level, skill tree

**Environmental Threat**:
A non-agent danger created by planetary conditions rather than an enemy. Oxygen deprivation, water, depth, and falling are the initial Environmental Threats.
_Avoid_: Enemy, combat encounter

**Recovery Cache**:
A retrievable container left at the player's place of death that holds the inventory carried at that moment. Only the most recent Recovery Cache exists.
_Avoid_: Corpse, grave

**Basic Tool**:
The player's always-available basic interaction and slow resource-gathering capability. It is not an Item, remains available after death, and prevents loss of inventory from making the world unrecoverable.
_Avoid_: Free replacement Item, starter loot

**Planetary Restoration**:
An optional progression path that renews local ecosystems and grants benefits without replacing base development as the player's main activity.
_Avoid_: Main quest, victory condition

**Restoration Beacon**:
A discoverable planetary structure whose activation can restore the surrounding Sector, periodically deposits renewable bio-resin and medicinal fiber in its internal inventory, and supplies energy to a physically connected local Power Network. Some Restoration Beacons also support Fast Travel.
_Avoid_: Objective marker, quest beacon

**Beacon Activation**:
The permanent transition caused by installing a Beacon Repair Core. In the initial scope it enables energy generation, Beacon Inventory production, and eligible Fast Travel without yet performing Planetary Restoration.
_Avoid_: Sector restoration, quest completion

**Beacon Inventory**:
The internal storage of an activated Restoration Beacon into which it periodically deposits renewable resources for later collection.
_Avoid_: Resource nodes, player inventory

**Beacon Repair Core**:
A Fabricator-produced Item made from refined iron, refined copper, and bio-resin that activates a dormant Restoration Beacon.
_Avoid_: Repair quest, direct resource payment

**Fast Travel**:
Travel initiated at the Landing Module or an eligible activated Restoration Beacon and ending at another such point without traversing the intervening terrain.
_Avoid_: Teleport command, respawn

**Contextual Feedback**:
Interface information that explains the currently targeted object, available interaction, or reason an action cannot proceed without prescribing a progression path.
_Avoid_: Quest marker, checklist, tutorial chain

**Test World**:
A world generated from a fixed seed for deterministic validation of content that remains fully procedural and non-guaranteed in ordinary worlds.
_Avoid_: Curated player start, guaranteed world content

**Sector**:
A local region associated with a Restoration Beacon and affected by its restoration.
_Avoid_: Level, map
