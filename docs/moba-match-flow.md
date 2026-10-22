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

For playing with friends without the queue. Either use the **Partida privada (sala con código)** option of the Maestro or
the chat command `/sala`:

| Command | What it does |
|---|---|
| `/sala crear [5v5\|3v3\|1v1\|aram]` | Creates a room (you are the host, blue team) and shows its code. |
| `/sala unir CODIGO` | Joins a friend's room (the team with fewer players). |
| `/sala equipo` | Moves you to the other team, if it has room. |
| `/sala bots si\|no` | Host only: fill the free places with bots (default yes). Without bots both teams must have the same number of players. |
| `/sala iniciar` | Host only: starts the match for everybody in the room. There is no ready check. |
| `/sala salir` | Leaves the room (the host leaving hands the room to another member). |
| `/sala` | Shows the room. |

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
