# MOBA mode: before and after a match

How a player prepares a match and what happens when it ends. The server (`openmu-aram`) drives all of it; the client only
shows the windows and sends the answers.

## Picking the skills (loadout window)

Talk to the **Maestro de la Arena** in Lorencia and choose **Elegir mis skills**. A window opens with the icon of every skill
the character's class can take into a match (buffs and summons are not offered).

- Click a skill to pick it or drop it. Between 4 and 6 skills have to be picked; the counter shows how many.
- **GUARDAR** saves the pick (it stays after a server restart); **CANCELAR** or `Esc` closes the window without saving.
- A character that never picked uses a default set of six attacks for its class.
- The pick is used by every match started from the queue or from a private room. The champion enters the match with only
  those skills (the skill bar is built from them).

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

## End of the match: results table

When a nexus falls the banner shows **VICTORIA** or **DERROTA** and, a few seconds later, the results table: one line per
champion (class, level, kills/deaths/assists, minions, gold, damage dealt and taken; your own line is highlighted).

- **VOLVER A LORENCIA** sends you back to your real character right away; otherwise the server does it after 90 seconds.
  The client reconnects by itself, as it does when a match starts.
- The arena is taken down once the last player has left.
