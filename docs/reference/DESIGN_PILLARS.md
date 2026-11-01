# Design pillars

This page records what kind of combat game KatanaCombat is meant to be, and the defense decisions that follow from it. The
owner set the pillars on 4 October 2026 by answering a set of multiple-choice questions, then made the defense decisions on
the same day with cost and content facts in hand.

- Where the owner was unsure, this page says so. Those answers are working assumptions, not settled direction.
- Each decision, with its date, reasoning and status, is in the [decision log](DECISIONS.md).
- The facts these decisions rest on, with file and line references, are in the
  [October 2026 defense and feel findings](../audits/2026-10-defense-and-feel-findings.md).

## Why pillars first

The project began as a cinematic free-flow system in the style of Assassin's Creed 3 and 4 and Batman Arkham. Later
requests pulled toward duels: a lock-on, an engagement mode, and readable parries. The shape of the parry chain forced the
question. In the owner's words, "this is a critical decision as it decides the soul of the game". A perfect-parry action
game wants a parry followed by a contextual follow-up, as in Sekiro or Ghost of Tsushima. A counter-driven cinematic
brawler wants the parry to include the counter. Rather than prototype both or follow the original description, the owner
chose to write the pillars first and let the chain follow from them.

## The pillars

| Pillar | The owner's answer | What it means |
|---|---|---|
| Fight scale | Mixed: small crowds plus duels | Most fights are 3 to 6 enemies. Deliberate one-on-one duels against named opponents are the highlights, as in Ghost of Tsushima. |
| Camera | Free camera, optional lock | A free camera with soft targeting and magnetism, plus an optional hard lock for duels. |
| Player fantasy | Both, by context | "A whirlwind in crowds, a precise duelist in duels." The systems shift with the fight. |
| Skill | A mix of "easy in, deep mastery" and "cinematic power fantasy". **Marked uncertain by the owner** ("i am uncertain"). | Forgiving at first, with parry assists and generous windows as designer settings and a high ceiling for players who learn timing; and success that is mostly about rhythm and cues, where you rarely die and always look good. |
| Readability | A mix of "animations plus a subtle cue" and "a universal warning icon". **Marked uncertain by the owner** ("I am uncertain"). | Players read each enemy type's telegraphed animations, with a subtle flash or sound on the parryable moment; and every incoming attack can also show the same counter indicator. |
| Pacing | A mix of "fast basics, longer elites" and "flowing combo momentum" | Ordinary enemies fall in a few clean hits while elites take a real exchange; and keeping a continuous combo going across the crowd is part of the fun. |
| Finishers | A mix of "earned on openings" and "frequent and flowing", "if that is possible" | A finisher becomes available when you create an opening (a parry, a broken guard, a low-health elite); and most kills can end in a short finisher, so the flow rarely stops. |

The mixed answers are compatible in practice. Frequent short finishers on basic enemies and earned finishers on openings
against elites is a known pattern. Skill and readability will come back as sharper questions.

## Defense depends on the enemy type

The owner asked what each option would cost in animation and assets before choosing a role for defense. With those facts,
they chose to set defense's role per enemy type:

| Enemy type | How defense plays | Parry chain |
|---|---|---|
| Basic enemies | Counter-led, as in Arkham and Assassin's Creed: attack freely, counter incoming strikes, finish them. | **Instant parry-counter.** One press: the perfect parry plays a combined deflect-and-strike two-character animation. |
| Duelists and elites | A lighter parry-centric kit, as in Sekiro or Ghost of Tsushima: dangerous attacks you must parry, each with a reaction for the attacker being parried. About 6 such reactions need to be made. | **Parry, then a contextual follow-up.** The parry deflects and staggers the attacker. The next attack press gives a finisher if the enemy qualifies, or a single-character strike otherwise. |
| Brutes and guard-breakers | Later, as separate pieces. | Not designed yet. |

**The parry chain by enemy type is a leaning, not a final decision.** The owner leans toward it but added a condition: "i
dont want to cheapen the feel of the parry + counter included branch so i want to ensure we have animations for every
direction". The project owns 8 two-character counter animations, and an offline check found that each one starts with the
attacker in front. None covers an attack from the side or behind. Side and rear coverage is planned as described below.

The analysis behind these choices also suggested making evade a universal escape. That suggestion is not part of the
option the owner picked, and evade is not implemented yet.

## The counter window

This applies only to the parry-then-follow-up chain used against duelists and elites. The instant parry-counter has no
window, because the counter plays with the parry.

- After a perfect parry the attacker is **Staggered**, the name the owner chose for the state a parry causes.
- Once the parry animation ends, the player is free: they can move, guard and be hit. The staggered enemy stays held for
  the whole window.
- **The player's aim decides what a press does.** A press aimed at the staggered enemy, within a broad cone that prefers
  the staggered enemy, performs the follow-up. A press aimed elsewhere ends the window, releases the held enemy and becomes
  a normal attack, so crowd flow is kept.

The free-player behaviour is under review in PR #137. The aim rule is not built yet.

## Guard reach

- **A perfect parry works from any direction.** It plays a directional deflect (left, right or behind). Until those clips
  exist, a code-driven turn covers off-front parries.
- **A held block covers the front arc only.** Hits from outside it land, so positioning matters and flanking enemies are a
  real threat.
- **Block and parry data are keyed by direction from the start.** Upgrading later to full-circle directional blocks, in
  the Arkham style, is then new content plus one setting, not a data change.
- **One rule will decide whether a guard blocks:** the defense resolver's tolerance (35° by default). It already decides
  every weapon contact. An older, separate 70° check still applies to damage dealt directly, such as counter and finisher
  damage, and to two Blueprint queries. PR #136 retires it; once that merges, the resolver's tolerance is the only rule.

## Parry-counters from the side or behind

The owned counter pairs all start with the attacker in front, and a marketplace search found no pack selling counter
pairs where the attack comes from behind. The plan is an authored turn-deflect that flows into a front counter pair:

1. Classify where the attacker is: front, left, right or behind.
2. **Front:** play a counter pair directly.
3. **Sides:** a 90° turn-deflect, mirrored for the other side, flowing into a front pair.
4. **Behind:** a 180° turn-deflect flowing into a front pair.
5. Motion warping hides the leftover angle within each direction, inside the deflect's own motion, for both characters and
   within a turn limit.

The owner's framing of the problem: "a lot of those animations refer to finishers from behind (ie attacker is behind the
victim) and not ones where the victim attacks the attacker and the attacker parries behind them". Public descriptions of
how Arkham and Sifu handle this are second-hand, so they informed the choice without deciding it.

**Timing.** Natural-looking turns take longer than a parry window. The owner's answer: "Why dont we make parry windows
longer as needed and also retime the turns as needed for the best feel?" The recorded plan is two designer settings for
each off-front direction:

- **a window lead:** the parry window opens earlier by the turn's duration and still closes at contact;
- **a turn play rate.**

The plan puts both in the reaction profile, with per-direction defaults that a character can override. The values will be
chosen from a rendered sweep of turn speeds (1.0, 1.5, 2.0 and 2.5 times), which is still running.

A throwaway rendered prototype with owned clips came first. Its measurements are in the findings document. In the
prototype's reading, the owned side turns look acceptable but would need about three times the speed, and a fast code spin
from behind fits the timing but reads as a turntable with skating feet.

## Facing while blocking and parrying

Block and parry facing is a code-driven, rate-limited turn owned by the combat code.

- It needs no animation authoring and works with any block clip.
- Today a block turns through motion warping, which only moves a character inside a warp window authored into the
  animation. A block clip without one doesn't turn.
- Motion warping stays for attacks and for the two-character parry, counter and finisher animations.

## How defense data is organised

The owner approved replacing the separate defense configuration asset, with one refinement: "perhaps these should be as
overrides per attack so we can have defaults and not have to configure everything per every attack if we want to have
only a few unique attacks or if we want to have a base profile to start with?"

- **Each attack** carries its hit, block and parry feedback and how long a parry staggers it. Values come from per-attack-
  type defaults and can be overridden per attack, the same layering knockback already uses.
- **Each character** has one reaction profile: its hit reactions; its block, parry, stagger and guard animations; its
  parry-assist settings; and its resistances. Profiles start from a base profile that characters inherit and override.
- **Global rules** live in the combat settings. The defense configuration asset is removed.
- **The parry fallback is explicit.** Each parry entry in the profile has an optional two-character animation, plus the
  solo animations to play when the paired one can't.
- **Parry assists are designer settings, not debug switches.** The owner wanted them to be "actual configurable settings".
- **Parry readability:** distinct contact feedback, a telegraph, an input buffer and a contact grace, and a parry animation
  that differs from the block-hit animation.
- **Knockback:** the combat settings asset holds the per-type defaults, each attack can override them, and the code values
  only seed a new asset. A per-weapon layer is noted for later.

## How hits and defense work

- **Stun length belongs to the attack.** Each attack type has a default stun length, overridable per attack. The stun field
  on hit-reaction entries is removed, so there is one place to tune how long a hit stuns.
- **Hit intensity.** The attack type decides whether a hit looks light or heavy. Whether the hit interrupts the victim is
  the attack's pressure against the victim's current resistance (for example bracing, attacking, or being an elite). Hits
  that don't interrupt play an additive flinch. This replaces today's rule, where damage divided by remaining health
  picks the reaction.
- **Pressure, resistance by attack phase, and lock duration** use per-type defaults in the combat settings, with
  per-field overrides on each attack.
- **The hit lock.** It applies to the current reaction behaviour now. Inputs pressed during it are buffered and fire when
  it releases.
- **Breaking defenders open.** Contextual stagger stays: being parried, a heavy into a guard, and finisher openings. A
  simple guard break is added for guarding enemy types, for example after a number of blocked hits or by a heavy attack.
  There is no posture meter. This revises the defense design's non-goal on guard-break gameplay, for guarding enemy types
  only.

## Targeting and engagement

- **Guard aim: direction, with magnetism.** The camera aims the guard, a bounded pull draws it toward the best threat, and
  a hard lock replaces the camera direction.
- **Planned features:** strafing while blocking, a target lock for duels, and an engagement mode. The player's combat mode
  comes first; enemy coordination comes after it.

## Still open

- **Exact rules.** The owner wants the rules for guard breaks, finishers, parries and counters written down first: what
  causes what, how, and why. They don't want to find out how something is calculated or arbitrated after the code exists.
- **Skill and readability** are marked uncertain.
- **The parry chain by enemy type** depends on directional counter content.
- **Turn timing values** wait on the rendered speed sweep.
- **Evade** is not implemented, and its role is not decided.
