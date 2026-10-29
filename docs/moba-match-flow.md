# MOBA mode: before and after a match

How a player prepares a match and what happens when it ends. The server (`openmu-aram`) drives all of it; the client only
shows the windows and sends the answers.

## Picking the skills (loadout window)

The window opens by itself when your champion stands in its base before the match starts (and from the **Maestro de la Arena**
in town, or with `/misskills`). It shows every skill the character's class can take into a match (buffs and summons are not
offered; the class's buffs are always part of the champion).

- Click a skill to pick it; the picked ones get a green cell, a gold frame and a number. Pick **4**: clicking a fifth drops the
  oldest. Every time the pick is complete it is applied to the champion at once and saved (it stays after a server restart).
- **CERRAR** or `Esc` closes the window; when the countdown ends the window closes and the picks of that match are final.
- A character that never picked uses a default set of four attacks for its class.

## Private rooms

For playing with friends without the queue. Talk to the **Maestro de la Arena** and choose **Salas privadas**: the room browser
opens with the list of the open rooms (name, host, format, players, how long ago it was created) and a **UNIRME** button on each.

- **CREAR SALA** opens the form: the name, the players per team (1 to 5), the map (the three lanes with the jungle, or the
  middle lane only) and whether the free places are filled with bots. So any mix is possible: 2v2 without bots, 3v3 on the middle
  lane, 5v5 with bots...
- Inside a room a panel shows both teams with the portrait of each player's class, and the **chat of the room** on the right
  (press Enter or click the line to write, Enter sends, Esc leaves the line). Every player has **CAMBIAR EQUIPO** and **SALIR**.
- The host also has **BOTS**, **EMPEZAR** (there is no ready check) and **CERRAR SALA**, and three small buttons on every other
  player: move to the other team (`<>`), kick (`X`) and hand the room over (`★`). A kicked player cannot join that room again.
- A room closes by itself after 15 minutes without news (somebody joining, leaving, writing or changing something); the members
  are warned in the room chat three minutes before. `_` turns the panel into a small «MI SALA» tab and back.
- Chat commands still work: `/sala crear [NvN|NvNaram|aram]`, `/sala unir NUMBER`, `/sala equipo`, `/sala bots si|no`,
  `/sala iniciar`, `/sala salir` and `/sala`.

Everybody needs level 400 and a Master class (game masters skip the requirement), and cannot be in the queue or in a match.

## Idle players (anti-AFK)

A champion whose player gives no input (or is disconnected) for **15 s** is announced to its team as idle, and after **20 s**
it channels the recall and goes back to its base. Any input clears it. It does not run during the pre-match countdown or while
the champion is dead.

## Controlling a disconnected teammate

When a teammate disconnects, his champion stays in the match, and **any other player of his team can control it** (only while its
player is disconnected: an idle but connected champion is only sent back to its base). A card with the portrait, the name, the
life and the mana of each of these champions appears on the left of the screen.

- **Take / release:** a short tap of **Ctrl** (without clicking or pressing another key) takes the nearest one, or lets go of the
  controlled one; a click on a card takes that one. A player controls one champion at a time and a champion has one controller.
- **Orders:** like the creature of the Elf, the **right click** on the ground (or on the minimap) walks the champion there and on
  an enemy it attacks. **B** sends it back to its base.
- **Skills:** the **number keys 1-9** cast **its** skills (with its own ranks, mana and cooldowns) on the object or the point
  under the mouse. The skill bar of the controlled champion appears above the skill bar of the hero, with the rank and the
  cooldown of each skill. To change a key, click a skill of that bar and then press the number key: the setting lives only in your
  client, the owner's keys are never touched.
- **The owner comes back:** the control ends at once and he finds his champion as he left it, with his own key setup.

## Gear by class and by set

The pieces of the shop give a bonus that depends on the class that wears them (the tooltip of each piece lists it, in blue):
Blade Knight and Rage Fighter get life, life regeneration and durability; Soul Master and Summoner mana, mana regeneration and
skill power; the Elf healing power and critical chance; the Dark Lord and the Magic Gladiator a mix.

Where a class is offered **two sets of the same tier**, the first one is the *recovery* set (life or mana, and recovery) and the
second one the *attack* set (skill power and critical chance), so you can build against the style of your class. A class with a
single set in a tier gets a balanced one (half of each). The weapons of the third tier work the same way (the second one is the
attack weapon), and the three mixes of the first two tiers differ by their excellent options.

Every class is offered two sets in every tier. Where a class has a single set of its own, it wears a set of a related class: the
Magic Gladiator (no helm) can be a mage with the sets of the Soul Master or a tank with the sets of the Blade Knight, the Summoner
completes its third tier with a set of the Soul Master, and the Rage Fighter its second and third tiers with sets of the Blade Knight.

## Teleport of the Soul Master

The Soul Master and Grand Master have the **Teleport** skill (a jump of up to 6 tiles, cooldown 1 s). Each jump costs AG: a full AG
gauge pays **three jumps**, the gauge refills a jump every 4.5 s, and the rings and the pendant add AG regeneration (the tooltip says
how much) up to a jump every 2.5 s with three tier 3 pieces. The jump is refused before the match starts and onto the enemy base.

## Base aura

Around the blacksmith (the vendor) of each base there is a green aura: the living champions of the team inside it recover **20 %
of their life, mana and shield every second**, in or out of combat.

## End of the match: results table

When a nexus falls the banner shows **VICTORIA** or **DERROTA** and, a few seconds later, the results table: one line per
champion (class, level, kills/deaths/assists, minions, gold, damage dealt and taken; your own line is highlighted).

- **VOLVER A LORENCIA** sends you back to your real character right away; otherwise the server does it after 90 seconds.
  The client reconnects by itself, as it does when a match starts.
- The arena is taken down once the last player has left.

## The announcer and the kill feed

The server tells the client the notable events of the match and the client turns them into a **voice line, a banner and a kill feed line**
(upper right corner: "X mató a Y", with "(Doble muerte)" and the like; the colour is the team that did it, 6 lines, 8 seconds each).

| Event | When |
|---|---|
| First Blood | the first champion kill of the match |
| Double / Triple / Quadra / Penta Kill | 2, 3, 4 or 5+ kills of the same champion within 10 seconds |
| Equipo eliminado / Todos han caído | the last living champion of a team (3 or more players) died: yours won / lost the fight |
| Shutdown | you ended the kill streak (3+) of an enemy |
| Killing Spree / Dominating / Unstoppable / Legendary | 3 / 5 / 7 / 9 kills in a row without dying |
| Torre caída | a turret fell (yours or the enemy's: the voice says whose) |
| Nexo bajo ataque | your nexus is losing life (at most every 30 seconds) |
| Start of the match, victory, defeat | the countdown ended / a nexus fell |
| Golden dragons, golden invasion, Great Golden Dragon slain | the golden invasion events |

One voice line is heard at a time: a more important one interrupts the current one, the others wait their turn (up to 4 seconds).
The voice lines are the WAV files of `Data\Sound\MobaAnn` (see `tools/anunciador` in the server repo to regenerate or replace them).

## Music

In the arena the music follows the match: **preparation** (countdown) → **early game** (from the start) → **battle** (3 minutes after the start)
→ **golden invasion** (from the dragons' fly-over until the end) → the Crywolf **victory / defeat** music and silence. Tracks fade out when they
change and loop on their own. The music volume and the music on / off option of the options window apply. The files are
`Data\Music\moba_prep.ogg`, `moba_early.ogg`, `moba_mid.ogg` and `moba_golden.ogg` (see `tools/musica` in the server repo).
A client that joins a match already running starts on the preparation track until the first event of play.

## Other HUD pieces

- **Active pets:** a row of small badges centered right over the skill bar, one per extra pet switched on from the inventory (hover for the name). The skill window (**K**) and the Elf's creature strip are drawn above that row.
- **Elf's creature skills:** with the command mode on (**X**) the creature's one or two skills show next to its strip; **1** and **2** (or a click on the button) cast them on the enemy under the cursor (the nearest one if there is none), with cooldown sweep. Without the command mode the creature uses them by itself.
- **Golden monsters:** while the invasion lasts they are gold dots on the minimap (bigger the stronger) and the TAB scoreboard ends with
  "Dorados vivos: N (grandes y dragones: M)".
- **Shop:** talking to the blacksmith opens the panel at once: seven tabs (Para ti, Consumibles y accesorios, Armas, Sets T1/T2/T3, Mascotas y escudos), the recommended build of your class on the left, the catalog in the middle (sets as columns, helm to boots from top to bottom, with a **SET** button) and the detail and the buy button of the item you click on the right. The **perfection jewel** (a Jewel of Bless dropped by golden goblins, 30 %) is dragged onto an item of your inventory and takes it to its maximum.

## Surrendering

After 15 minutes of match, any human of a team can type `/rendirse` to propose giving up; the rest of the team votes with the same command.
**75 % of the humans** of the team (5 players: 4) must agree within 60 seconds and then the nexus of that team falls and the match ends.
A vote that does not pass cannot be repeated for 2 minutes. Bots do not vote.

## Pings

Mark a spot for your team the way League of Legends does (the teammates see it, the enemy does not):

| You do | Result |
|---|---|
| hold **G** or **Alt** and click | a plain ping |
| hold **Ctrl** and click | a caution ping |
| hold one of those keys, keep the mouse button down and wait a moment | the **wheel of nine pings** opens around the click: drag toward one and let go (the right mouse button closes it without pinging) |

Wheel, clockwise from the top: **Retirada**, **Empujar**, **Voy en camino**, **Todo o nada**, **Ayúdame**, **Falta visión**, **Enemigo desaparecido**, **Visión enemiga**; the centre is **Peligro**.
It works on the ground and on the minimap (hold the key and click on the minimap).

The ping shows on the ground of your teammates as an icon with expanding rings (2.5 seconds, with the sender's name) and, when it is out of view, as an arrow at the
edge of the screen; on the minimap it shows a ring for 5 seconds. Each kind has its own sound. You can send at most **5 pings in 5 seconds**; past that you cannot ping for 6 seconds.
In the arena **G** belongs to the pings (it no longer opens the guild window); **V** and **I** open the inventory as usual.

## Damage direction

When your champion takes damage, a **red wedge** appears around it pointing at the attacker (blue when only the shield took it; the stronger the hit, the brighter it is).
Damage without a source (poison, burning) pulses a red ring instead.

## Inhibitors and super-minions

Each lane has a chain of structures for each team: **outer turret → middle turret → base turret → inhibitor**. The inhibitor (a small crystal, on the minimap in your team colour) stands on the road of the lane, behind the base turret toward the nexus; it cannot be hit until the base turret of its lane has fallen.

- **A fallen inhibitor** (a) makes the **nexus of its team attackable** for as long as it stays down (when it comes back, 5 minutes later, the nexus is protected again), and (b) sends **super-minions** down that lane with every wave of the enemy team: one per wave (two in every lane if all three inhibitors are down).
- The attackers get the EXP of a turret and **100 gold each**. The announcer tells when an inhibitor falls and when it is back; the owner team is warned 30 seconds before the respawn.
- A super-minion is a big (35 % larger) creep with 5 times the life, 2.5 times the damage and double the defense of a normal creep; it is worth three creeps.
- On the minimap a fallen inhibitor shows a dark crystal with a cross; the TAB scoreboard says how many are down and when yours comes back.
