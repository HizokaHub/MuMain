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

## Base aura

Around the blacksmith (the vendor) of each base there is a green aura: the living champions of the team inside it recover **20 %
of their life, mana and shield every second**, in or out of combat.

## End of the match: results table

When a nexus falls the banner shows **VICTORIA** or **DERROTA** and, a few seconds later, the results table: one line per
champion (class, level, kills/deaths/assists, minions, gold, damage dealt and taken; your own line is highlighted).

- **VOLVER A LORENCIA** sends you back to your real character right away; otherwise the server does it after 90 seconds.
  The client reconnects by itself, as it does when a match starts.
- The arena is taken down once the last player has left.
