# Combat rules, data model and attack authoring: design

- **Date:** 2026-10-05.
- **Status:** draft for the owner's review, section by section. It authorizes no code change and no asset save.
- **Why this exists.** The owner's standing rule [D-20261004-45]: "i want to get more specific with the rules for guard
  breaks, finishers, parries, and counters, and what causes what and how and why and exact mechanics i dont want to find
  out how something is being calculated or arbitrated later once youve already generated code". The owner also chose one
  combined spec for the combo set, attack data, impact layering and the reaction profile, built in stages
  [D-20261005-04].
- **Baseline:** `origin/main` at `a7693bb4`; every file and line cited is from that commit. While this spec was being
  written, four pull requests merged into `main` (now `e47da15e`): #136, one block rule (the legacy 70° cone is gone);
  #139, attack timing read from the montage; #138, the October findings, design pillars and decision log; and #137, the
  rework of today's post-parry press window, which stays as the interim behaviour until the parry-counter stage (9.7)
  retires it. Their effect on the rules is noted where it matters. No pull request that touches these rules is open.
- **Sources.**
  - The decisions register, `docs/sessions/registers/decisions.md` (local and git-excluded), through D-20261007-18, plus the ruling R-20261005-01. Only
    active decisions are used, and "superseded by" is respected.
  - The owner's own answers, read from the session transcript, where the register condenses them.
  - The adopted combat glossary [D-20261005-40], reproduced as section 2 with the amendments of D-20261005-42,
    D-20261005-47 and D-20261005-48.
  - `.superpowers/design/combat-rules-as-is.md`, the current-rules reference. It is model-written. Every code fact this
    spec relies on was re-read at `a7693bb4`, unless it is marked **[reported]**.
  - The authoring comparison, the ComboCreator and Duet review, the defense-role feasibility report, the owned-deflect
    check, the directional counter prototype, and the defense data traces (all under `.superpowers/`).

## How to read this document

| Label | Meaning |
|---|---|
| **Decided** | The owner chose it. The decision ID is in brackets, for example [D-20261005-24]. |
| **Today** | What the code does at `a7693bb4`, with a file and line. |
| **Proposed** | Structure, names or editor behaviour this spec suggests. It is not a combat rule and sets no number, but it still needs the owner's approval. |
| **Open (Qn)** | Needed but not decided. Section 10 asks it as an A–D question with a recommendation. A tunable whose default reads "Qn" has **no** decided value; any number next to it is today's value, shown for reference only. |

- Behaviour that no decision touches keeps working as it does today. Section 3.11 lists that behaviour, so that nothing
  changes silently.
- Source paths are under `Source/KatanaCombat/Public/` or `Source/KatanaCombat/Private/`. The folder is left out where the
  file name is unique.
- Wherever a decision's wording leaves room for more than one reading, the reading used here is stated and the question is
  listed in section 10. Appendix B collects every such case.
- **Words.** This spec uses the adopted glossary (section 2). The retired words appear only in section 7, and only where it
  names today's code or assets.

---

## 1. Purpose and design pillars

**Status:** approved by the owner as written at `a34ecd69` [D-20261005-65]; amended since by D-20261006-04 (rules
before content), D-20261007-07 (how a lethal parry-counter ends) and D-20261007-11 (the words parry, counter and
parry-counter).

### 1.1 Purpose

This document holds three things in one place:
- the exact combat rules;
- the data that carries them;
- the attack-authoring overhaul.

It puts rules first and code later. It also puts rules before content: no combat rule is derived from what content
exists; content is created, bought or procedurally fitted to the rules, and an asset limitation is met with technical
or procedural means, not a weaker rule [D-20261006-04]. Implementation briefs carry the rule text from this document. Anything a rule needs
that this document does not state is an open question for the owner, not a choice for the implementer.

### 1.2 Design pillars

The pillars page is [`docs/reference/DESIGN_PILLARS.md`](../../reference/DESIGN_PILLARS.md), on `main` since PR #138
merged.

Parts of that page predate the decisions of 5 October and are superseded:
- the two parry modes by enemy type, now one model [D-20261005-24];
- the parry window lead, now an adaptive turn [D-20261005-05];
- resistance by attack phase, now armor [D-20261005-17];
- the separate press window after a parry-counter, now the Staggered time [D-20261005-22];
- block and parry reactions on the reaction profile, now the weapon's defense set [D-20261005-48].

The pillars themselves are the owner's answers of 4 October [D-20261004-26]:

| Pillar | The owner's answer |
|---|---|
| Fight scale | Mixed: small crowds plus duels. |
| Camera | Free camera, optional lock. |
| Player fantasy | Both, by context. |
| Skill | A mix of "easy in, deep mastery" and "cinematic power fantasy". **The owner marked this uncertain.** |
| Readability | A mix of "animations plus a subtle cue" and "a universal warning icon". **The owner marked this uncertain.** |
| Pacing | A mix of "fast basics, longer elites" and "flowing combo momentum". |
| Finishers | A mix of "earned on openings" and "frequent and flowing", "if that is possible". |

### 1.3 Principles that follow from the decisions

These restate decisions; each cites its source.

1. **Health decides kills.** A kill is decided at the press, from resolved damage after resistances. A lethal follow-up
   strike or ordinary attack plays a paired finisher; a lethal parry-counter kills at the counter's contact, and the
   entry's authored kill sequence plays instead when one exists [D-20261005-38, D-20261005-39, D-20261005-42,
   D-20261007-07].
2. **One Staggered state, one number.** A non-lethal parry-counter and a guard break lead to the same state, with one default
   duration. The follow-up strike opportunity lasts exactly that long [D-20261005-08, D-20261005-22, D-20261005-31].
3. **Defaults with overrides.** Each value resolves through four layers: the combat settings' type default, the combo
   set's type default (the per-weapon layer), the attack's override and the slot's override
   [D-20261004-20, D-20261004-38, D-20261005-01, D-20261005-61]. Each character's reaction profile inherits from a base
   profile and overrides it [D-20261004-20].
4. **One place per question.**
   - What does this attack do? The attack.
   - What follows what? The weapon's combo set.
   - How does this weapon block and parry, and which counter follows each parry? The weapon's defense set.
   - Which two-character sequences does this weapon play? The weapon's paired set.
   - How does this character's body react, resist and break? Its reaction profile.
   - What are the global rules? The combat settings.

   Sources: [D-20261004-20, D-20261005-01, D-20261005-48, D-20261005-57].
5. **Input is never lost.** Presses made during your own attack or during hit-stun are buffered
   [D-20261005-13, D-20261005-37, D-20261003-03].
6. **Feel is judged on rendered captures**, not on code or automation alone. This is project practice, recorded in the
   project memory and in [D-20261004-40].
7. **Rules before content.** No combat rule is derived from what content exists; content is created, bought or
   procedurally fitted to the rules, and an asset limitation is met with technical or procedural means, not a weaker
   rule [D-20261006-04].

---

## 2. Glossary

**Status:** approved by the owner as written at `a34ecd69` [D-20261005-65]; the reaction-profile entry was then narrowed by
D-20261005-68, and the decisions of 6 and 7 October amended the entries marked below.

### 2.1 The adopted glossary

The owner adopted this glossary as written [D-20261005-40]. It is reproduced from
`.superpowers/design/combat-glossary.md` with the heading levels adjusted. Later decisions changed it, and each
changed entry is marked **[amended]** with its decision: the paired finisher's scope [D-20261005-42], the reaction
profile's contents [D-20261005-47, D-20261005-48], and three new authoring entries, defense set, paired set and weapon
data [D-20261005-48, D-20261005-49, D-20261005-57]; then the words parry, counter and parry-counter [D-20261007-11],
the aim cone's reference [D-20261007-12], the counter's shape [D-20261007-07], the defense set's damage [D-20261006-03]
and the lenient lockout [D-20261007-18]. Every other line is verbatim.

One meaning per term. Required in code identifiers, tooltips, logs, specs and docs. Retired words are rejected in review.

#### States
- **Staggered** — the open state. Caused by a non-lethal parry-counter or a guard break. Lasts the Staggered time
  (default 1.5 s, overridable per attack). While Staggered, a strike lands without reply. The player's own default is
  0.6 s, and a dodge press ends the player's early (3.4) [D-20261005-88].
- **Hit-stun** — the short lock after an interrupting hit (defaults light 0.25 s, heavy 0.45 s, charged 0.6 s; floor
  0.12 s). Attacks and movement wait; guard can still be raised; input is buffered and fires after.
- **Flinch** — the additive reaction to a hit that does not interrupt.
- **Guard** — block held. Covers ±75°, turns the defender toward the hit, negates all damage (no chip).

#### Causes
- **Pressure** — an attack's weight: light 1, heavy 2, charged 3 (overridable per attack). Used for interruption and for
  guard break.
- **Armor** — the victim's resistance to interruption: a per-character base (default 0) plus authored armor windows on
  animations. A hit interrupts if pressure > armor.
- **Guard break** — 4 pressure points blocked within 3 s (per-character tunables), or a blocked guard-breaker attack.
  Result: Staggered. The player's guard breaks only to guard breakers.
- **Guard breaker** — an attack flagged to break a guard instantly. Charged heavies and directional lights by default.
- **Unparryable** — an attack flagged so a parry press cannot answer it. All attacks are parryable unless flagged.

#### Actions
- **Parry** — the deflect: the defender's motion that meets the incoming blade, after a block press inside the parry
  window. Today every parry is part of a parry-counter; the word alone is reserved for a possible later perfect parry
  that does not counter automatically. **[amended by D-20261007-11]**
- **Counter** — the strike back that follows the parry inside a parry-counter. Never a separate press.
  **[amended by D-20261007-11]**
- **Parry-counter** — the one-press sequence: the turn, the parry and the counter. A block press inside the parry window
  always plays it today. **[added by D-20261007-11]**
- **Follow-up strike** — the ×1.5 strike from an aimed attack press while the target is Staggered.
- **Lethal** — resolved damage (after resistances) ≥ the target's remaining health, decided at the press.
- **Paired finisher** — the synced two-character kill clip. Plays whenever any aimed strike is lethal: a parry-counter,
  a follow-up strike, or an ordinary attack. Marked enemies can do the same to the player. Never plays otherwise. For a
  parry-counter it is the entry's authored kill sequence when one exists; otherwise the counter itself kills at its
  contact (3.3). **[amended by D-20261005-42, D-20261007-07]**
- **Parry window** — the input timing on the attacker's animation, never shorter than 0.35 s before contact, scaled by
  the parry-assist setting. Turning is never limited to it.
- **Parry lockout** — 0.25 s after a missed parry press during which no parry can trigger. Blocking still works. A press
  made during the lockout is remembered and counts as a parry when the lockout ends with the window still open; the
  length is a tunable to be felt first and may go to zero. **[amended by D-20261007-18]**
- **Aim cone** — 60° either side of the stick direction, or of the character's facing when no direction is held;
  Staggered or eligible enemies win ties, nearest otherwise; the camera never decides. Used for every attack press:
  follow-up strikes, paired finishers and ordinary attacks (3.13). **[amended by D-20261007-12]**

#### Authoring
- **Combo set** — the asset holding slots, links and entry rules for one moveset, plus set-level type defaults.
- **Slot** — one placement of an attack in a combo set, with optional per-field overrides.
- **Attack** (`UAttackData`) — the move itself: montage, timing, damage, pressure, flags, impact feedback.
- **Reaction profile** — a character's reactions to hits only: hit reactions, armor base, guard-break threshold, window
  and counting toggle, the Staggered reaction and recoil, hit-stun responses, the "can finish the player" marker, and
  parry assists. It never arbitrates movement or camera; those get their own profiles when needed.
  **[amended by D-20261005-46, D-20261005-47, D-20261005-48, D-20261005-55, D-20261005-68]**
- **Defense set** — the weapon's block and parry responses: block reactions, the guard enter and exit clips, and for
  each attacker direction and swing the deflect, the fallback strike, the counter's damage, and a reference into the
  paired set for the counter. Referenced by the weapon data. **[added by D-20261005-48; amended by D-20261005-55,
  D-20261005-57, D-20261006-03]**
- **Paired set** — the weapon's two-character sequences, keyed by direction and role: counters (each able to kill; an
  authored kill sequence is optional) and finishers. The defense set references its counters; the combo set references
  its finishers. **[added by D-20261005-57; amended by D-20261007-07]**
- **Weapon data** — the weapon's kit: it references one combo set (attacks), one defense set (defenses) and one paired set
  (two-character sequences). A character with no weapon data uses the combat settings' default weapon data.
  **[added by D-20261005-48, D-20261005-49; amended by D-20261005-57]**
- **Back-to-opener link** — the explicit link type that restarts a chain; the only sanctioned loop.

#### Retired words (do not use)
| Retired | Use instead |
|---|---|
| guard broken | Staggered |
| stunned | hit-stun |
| counter window | Staggered time |
| chain, bridge | parry-counter / paired finisher |
| ending (alone) | paired finisher, or non-lethal counter |
| posture | — (no meter exists) |

**Requirement.** Code identifiers, tooltips and log text use these terms, and review rejects the retired words
[D-20261005-40].

**One glossary line read through a later decision.** The **Armor** entry says "a per-character base … plus authored
armor windows". D-20261005-44 fixed the arithmetic: current armor is the higher of the base and any active window, not
their sum. The line is kept verbatim and read that way.

### 2.2 Further terms used in this spec

These terms are not in the adopted glossary. They name parts of the design that the glossary does not cover. **Proposed
additions**, so that the product uses one word for each.

| Term | Meaning |
|---|---|
| **Attack type** | The attack's `EAttackType`: Light, Heavy or Special (`CombatTypes.h:48-54`). It decides whether a hit looks light or heavy, and it selects the per-type defaults. "Charged" is not an `EAttackType`; it has its own column in the default tables, and its exact definition is open (Q1). |
| **Directional light** | A light attack started by releasing a held Light with a direction: today's hold-release follow-up (`CombatComponent.cpp:5165`). The five shipped ones are Light-type assets (read from the assets' bytes: no saved `AttackType`, so the code default Light applies). |
| **Resolved damage** | The damage a strike would deal to a given target: base damage × weapon multiplier × any multiplier this spec adds (×1.5 for a follow-up strike) × the target's damage resistance. Today the first two factors are applied at `WeaponComponent.cpp:927` and the resistance at `HitReactionComponent.cpp:215`. A character dies when its health reaches 0 or below (`BaseCombatCharacter.cpp:277`). |
| **Contact** (predicted arrival) | The moment the attacking blade is predicted to reach the defender, computed at runtime from the swing's trajectory [D-20261005-73]. The parry window is measured back from it, the turn finishes at it, and "lands first" compares it. |
| **Effective parry window** | The authored lead time before predicted arrival, after the 0.35 s minimum and the parry-assist scale are applied (section 3.2). |
| **Missed parry** | A Block press made while a parryable attack aimed at the defender is in progress, outside that attack's window. It starts the parry lockout. A press with nothing incoming is just a guard [D-20261005-71]. |
| **Deflect** | The parry as content: the defender's clip that meets the incoming blade. "Deflect" names the clip, "parry" the action [D-20261007-11]. |
| **Turn-deflect** | An authored 90° turn (for the sides) or 180° turn (for behind) that flows into a front counter pair. Motion warping hides the leftover angle [D-20261004-30]. |
| **Swing** | The parried attack's swing, one of four: slash left, slash right, overhead, thrust [D-20261005-45]. It is a field on the attack, filled by the "Auto-detect swing" editor action or by hand [D-20261005-84]. |
| **Counter pool** | The defense set's directional counter entries, keyed by the attacker's direction and the parried attack's swing [D-20261005-24, D-20261005-45, D-20261005-48]. Each refers to a counter in the paired set [D-20261005-57]. |
| **Counter entry** | One item in the counter pool: the deflect, the fallback strike, the counter's damage, and a reference to a paired-set counter that shares that deflect. The counter can kill at its own contact; an authored kill sequence on it is optional and plays instead when the press is lethal, never as a separate clip after it [D-20261005-38, D-20261005-48, D-20261005-57, D-20261006-03, D-20261007-07]. |
| **Kill sequence** | An optional authored two-character ending on a paired-set counter, played instead of the counter's own contact when its damage is lethal [D-20261007-07]. |
| **Non-lethal counter** | What a parry-counter plays when the counter's damage is not lethal: a paired counter, or the single-character strike when none fits. It leaves the enemy Staggered [D-20261005-23, D-20261005-24, D-20261007-07]. |
| **Cancel notify** | The one "free from here" notify on any non-attack montage (3.0) [D-20261006-02]. |
| **Single-character strike** | A strong cinematic strike played by the defender alone, together with the enemy's stagger reaction. It is the last fallback when no paired animation fits [D-20261005-23, D-20261005-30]. |
| **Finisher pool** | The paired set's finishers, keyed by the attacker's direction only [D-20261005-47], as the combo set references them [D-20261005-57]. |
| **Block shove** | The short push a blocked hit gives the blocker, with a per-type default [D-20261005-59]. |
| **Finisher range** | 350 cm: the maximum distance at which a paired finisher can start [D-20261005-28]. |
| **Attack token** | An enemy's permission to attack the player. At most 2 are held at once (configurable), and each enemy has a short cooldown [D-20261005-35]. |
| **Free mode** | Targeting with no lock: every attack press aims by the aim cone around the stick direction, or the character's facing when no direction is held (3.13) [D-20261007-12]. |
| **Lock mode** | Targeting after a lock press: every press aims at the locked target, the player strafes facing it, and a flick of the look stick (or a scroll) switches targets (3.13) [D-20261007-13]. |
| **Stick direction** | The direction the movement stick is held, read in the frame movement uses (a reading, Appendix B). |
| **Others hold** | While the player performs a parry-counter or a paired finisher, other enemies start no new attack. Swings already in progress finish, and the enemies resume when it ends [D-20261005-21]. Always written as "others hold", to keep it apart from holding a button. |
| **Lane** | A group of slots shown together in the moveset grid: light, heavy and directional [D-20261005-02]. |
| **Link** | What a slot does next for one input: Light, Heavy, or hold-and-release Light toward Forward, Back, Left or Right. A link points at a slot, back to the opener, or nowhere. |
| **Opener** | The slot a combo starts from when an input is pressed and no combo is running. Today: `DefaultLightAttack` and `DefaultHeavyAttack` on the attack configuration. |
| **Type default** | A value the combat settings hold for each attack type [D-20261004-20, D-20261004-38]. |
| **Set-level type default** | A per-type default held by a combo set: the per-weapon layer, second of the four in section 4.2 [D-20261005-40, D-20261005-61]. |
| **Attack override** | A field the attack asset sets for itself, replacing the default [D-20261004-20]. |
| **Slot override** | A field a slot sets, replacing the attack's value for that slot only [D-20261005-01]. |
| **Edit-through** | Editing the referenced attack's own fields inline from the combo set [D-20261005-01]. |
| **Make unique** | Copying an attack asset so that one slot can differ in fields a slot cannot override [D-20261005-01]. |
| **Base profile** | The reaction profile that character profiles inherit from and override [D-20261004-20]. |
| **Combat settings** | `UCombatSettings`: the global rules and the per-type defaults [D-20261003-08, D-20261004-20]. |
| **Resolved attack spec** | An attack's effective values after every layer, captured when the attack starts and carried on each of its hits (section 4.11). |
| **Moveset grid** | The combo editor. Rows are slots grouped by lane, and columns are inputs [D-20261005-02]. |
| **Node map** | A read-only diagram of a combo set's shape [D-20261005-02]. |

---

## 3. Combat rules

### 3.0 Conventions

Each mechanic below gives, in this order:
- the decisions it rests on;
- its trigger conditions, in evaluation order;
- its formula, thresholds and durations;
- the state changes it causes;
- its arbitration;
- its failure paths;
- its tunables, with each default and where it lives;
- what changes from today;
- its open questions.

**Clocks.** No decision says which clock each duration uses. Today:
- Staggered and hit-stun count game time, so slow motion and hitstop stretch them (`HitReactionComponent.cpp:83-119`).
- The post-parry press windows count real time **[reported]**.

Q8 asks for one rule. Until it is answered, the rules give durations without naming a clock.

**Rules before content.** No rule in this section is derived from what content exists; content is created, bought or
procedurally fitted to the rules, and an asset limitation is met with technical or procedural means, not a weaker rule
[D-20261006-04]. Content findings appear only as notes on the stages (section 9) and in the shopping lists.

**Shared words.** These keep today's meanings:
- "Hostile" uses the team interface.
- "Alive" means not dead and not dying.
- "Aimed at" means the attack's intended target is this character. Enemy AI sets it when it attacks; player attacks set it
  through attack warping **[reported: `CombatComponent.cpp:4200-4204, 6060-6080`]**.

**The cancel notify** [D-20261006-02]. One general "free from here" notify, placed once on any non-attack montage, marks
the end of a sequence. It is one notify, with one meaning per context:
- on the defender's counter half of a parry-counter: the player is free from here (3.3);
- on the victim's half: the enemy becomes Staggered from here, and the rest of its clip is its Staggered reaction
  (3.3, 3.4);
- on a hit reaction: the character can act again from here, once hit-stun has also ended (3.6, 3.7);
- on the fallback strike: the strike carries its own, with the defender's meaning;
- attacks never carry it: their cancel point stays the phase transition from Active to Recovery;
- a montage with no cancel notify is read as ending at montage end, and validation warns (4.13).

Inside a parry-counter, neither character can be touched until its own cancel notify fires [D-20261007-08]. The
paired-content interface (4.12) exposes each role's notify. The notify's class and its validation are in 4.13.

### 3.1 Guard and block

**Status:** approved by the owner as written at `a34ecd69` [D-20261005-65], with the buffered press (D-20261005-66), the
Staggered player (D-20261005-67, since superseded by D-20261005-88), the block turn (D-20261005-69) and the arc's home (D-20261005-70) answered since.

**Decisions:**
- D-20261005-11: ±75°, turning toward the blocked attack;
- D-20261005-12: no chip damage;
- D-20261005-13: starting an attack drops guard, and a press during an attack is buffered;
- D-20261005-14: the player's guard breaks only to guard breakers;
- D-20261005-37: guard can be raised during hit-stun;
- D-20261005-66: a buffered Block press raises guard only if Block is still held, and never parries;
- D-20261005-88: a Staggered player cannot attack, guard or move by the stick, and a dodge ends it after a minimum; it
  supersedes D-20261005-67;
- D-20261005-69: the block turn faces the attacker fully, at a rate on the defense set, 360°/s to start;
- D-20261005-70: the guard arc's default is in the combat settings and the defense set may override it;
- D-20261005-40: the glossary;
- D-20261004-21: a code-driven turn;
- D-20261004-23: block covers the front arc, and data is keyed by direction;
- D-20261004-24: one block rule, merged in PR #136.

**Raising guard.** Evaluated on each Block press, in order:

| # | Condition | If it fails |
|---|---|---|
| Guard-1 | The character is alive, is not a paired victim, and is not playing a parry-counter or a paired finisher [D-20261004-24, merged in #136]. | The press is refused. |
| Guard-2 | The character is not Staggered. | Refused for everyone: a Staggered enemy is held (3.4), and a Staggered player cannot guard until the state ends or a dodge ends it (3.4) [D-20261005-88]. The player's press is buffered and fires afterwards; that carries over from D-20261005-67 and is a reading (Appendix B). |
| Guard-3 | The character is not in its own attack. | The press is buffered. It takes effect when the attack reaches Recovery, or when the attack ends, and only if Block is still held then; a buffered press never counts as a parry [D-20261005-13, D-20261005-66]. |
| Guard-4 | Hit-stun does not refuse the press: guard can be raised during hit-stun [D-20261005-37]. | That press can parry [D-20261005-77]. |
| Guard-5 | Guard rises. The same press is offered to the parry rule (3.2). | — |

**Guard ends** on any of these:
- Block is released or cancelled;
- death;
- becoming a paired victim;
- **starting an attack** [D-20261005-13];
- a guard break (3.5).

The counter of a parry-counter is a strike. This spec reads it as an attack for this rule, so guard ends when the counter
starts (Appendix B). Guard comes back only through a new press, or through the buffer rule Guard-3.

**Block at contact.** The defense resolver evaluates each weapon contact in today's order, with the arc changed
(`DefenseResolver.cpp:289-375`):

| # | Condition | Outcome |
|---|---|---|
| Contact-1 | The contact is invalid, friendly, already consumed or stale. | Ignored (today). |
| Contact-2 | The defender is invulnerable or in i-frames. | Ignored (today). |
| Contact-3 | The defender started the paired animation now playing. | Ignored (today). |
| Contact-4 | The attack carries `Attack.Property.Unblockable`. | A hit at full damage through guard; guard stays up (today, `:337-341`). This holds even when the attack is also a guard breaker: unblockable wins, the guard does not break, and validation warns on the pair of flags (4.4) [D-20261007-16]. |
| Contact-5 | The defender is not guarding. | A hit (3.6). |
| Contact-6 | \|yaw\| > 75°. | A hit. |
| Contact-7 | Otherwise. | **Block.** |

**How yaw is measured (Contact-6).** As today: the angle between the defender's facing and the line from the defender to the
attacker's location, at contact. It is compared with a strict greater-than (`DefenseResolver.cpp:354-361`;
`BaseCombatCharacter.cpp:568-570, 704-706`). That reference stays [D-20261005-69].

**What a block does:**
- **No damage on any attack**, and so no hit reaction, hit-stun or knockback [D-20261005-12]. Today the same holds
  (`BaseCombatCharacter.cpp:816-820`; knockback starts only from a played reaction, `HitReactionComponent.cpp:491-500`).
- **Guard break is evaluated** (3.5).
- **The defender turns to face the attacker fully**, so that the block reads as coming from in front. The turn is
  code-driven and rate-limited [D-20261005-11, D-20261004-21]; its rate is a defense tunable on the weapon's defense set
  with a combat-settings default of **360°/s**, tuned from captures [D-20261005-69].
- **Presentation.** The block reaction for the incoming direction, from the defender's weapon's defense set
  [D-20261005-48]. While the arc is ±75° only the front entry is used. The data is keyed by direction now, so that full-circle blocks later need only
  content plus one setting, the guard arc [D-20261004-23].
- **The attacker** recoils if the attack carries `Attack.Defense.BlockInterruptible`. Otherwise its swing continues but
  cannot touch this defender again (today, `DefenseResolver.cpp:366-374`).
- **The block uses one of the swing's hit slots** (today).

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Guard arc (half-angle) | 75° | combat settings default; the defense set may override it | D-20261005-11, D-20261005-70 | 35°, `NormalBlockFinalTolerance` on the defense configuration |
| Block turn: amount | the full angle to the attacker | — | D-20261005-69 | at most min(70°, measured yaw), through motion warping |
| Block turn: rate | 360°/s, tuned from captures | combat settings default; the defense set may override it | D-20261005-69 | 180°/s |

**Changes from today:**
- The arc widens from 35° to 75°.
- Starting an attack drops guard. Today guard stays up through your own attack, and contacts within 35° are blocked
  mid-swing **[reported]**.
- A Block press during your own attack is buffered. Today it is refused and dropped (`CombatComponent.cpp:3317-3348`).
- The turn is a code-driven rotation, not a motion-warp window that each block clip must author.
- The legacy 70° rule is gone (merged in #136).
- A guard can now break (3.5).

### 3.2 Parry

**Decisions:**
- D-20261005-05: the window is input timing only and forgiving, the turn adapts, and there is no earlier press for the
  rear;
- D-20261005-06, D-20261005-32 and D-20261005-71: a 0.25 s lockout after a missed press, and what a miss is;
- D-20261005-72: no lockout after a successful parry;
- D-20261005-07: every attack is parryable unless flagged;
- D-20261005-09 and D-20261005-74: a 0.35 s minimum plus a scale, applied at runtime when no window is authored;
- D-20261005-73: contact is the predicted blade arrival;
- D-20261005-10, D-20261005-33 and D-20261005-76: the attacker is slowed to no less than 50% for at most 0.3 s, easing
  in and out;
- D-20261005-50 and D-20261005-75: the turn finishes exactly at arrival, under a per-weapon cap starting at 720°/s;
- D-20261005-36: lands first;
- D-20261005-77: a press during hit-stun can parry;
- D-20261005-78: assists default to neutral; feedback values come from a rendered sweep;
- D-20261005-79: the window and the turn are independent;
- D-20261005-40: the glossary;
- D-20261004-06 and D-20261004-07: readability features and parry assists;
- D-20261004-23: a parry from any direction;
- D-20261004-30: turn-deflects;
- D-20261005-64: timing alone decides the parry;
- D-20261007-18: the lenient lockout: a press made during the lockout is remembered and counts when the window is
  still open at its end; the lockout may go to zero;
- D-20261007-11: the words: this section covers the parry, the deflect; the counter and the whole parry-counter are
  3.3;
- R-20261005-01 (a ruling, **proposed**, the owner may veto): the window is authored as a lead time.

**The window and the turn are independent** [D-20261005-79]. The window decides only whether the press counted. The turn
and the attacker slowdown then behave as needed to communicate the action, and never feed back into the window. Both are
measured against the same moment, the predicted blade arrival [D-20261005-73]. The owner: "Can't the window just be for
whether it happened and the turn rate and behavior behave as needed to smoothly communicate the action?"

**Input.** The Block press that raises guard (Guard-5). A Block already held when a window opens does nothing; only a press
counts (today).

**Evaluation at a Block press, in order:**

| # | Condition | If it fails |
|---|---|---|
| Parry-1 | The press passed Guard-1 to Guard-4 and is a live press: a press buffered through the defender's own attack never parries [D-20261005-66]. A press during hit-stun is live and can parry [D-20261005-77]. | No parry. |
| Parry-2 | The defender is not in the parry lockout. | No parry now. Guard still rises: "blocking still works" [D-20261005-06]. **The press is remembered:** if a candidate's window (Parry-3) is still open when the lockout ends, the remembered press counts as a parry then [D-20261007-18; the lenient reading, Appendix B]. The press counts as a miss only if it meets Parry-3's definition of one, so a press with nothing incoming never extends the lockout [D-20261005-71]. |
| Parry-3 | At least one **candidate** exists. A candidate is an attack that: (a) belongs to a hostile, living character; (b) is aimed at this defender; (c) is in progress and not consumed; (d) is not flagged unparryable [D-20261005-07]; (e) has its effective parry window open now, including the input buffer [D-20261004-06]; and (f) has not already touched this defender (today, `CombatComponent.cpp:3078-3086`). **The attacker's direction does not matter** [D-20261004-23]. | Guard rises. **It is a missed parry**, starting the 0.25 s lockout, when some parryable attack aimed at this defender is in progress but outside its window [D-20261005-71]. With nothing incoming it is just a guard. |
| Parry-4 | **Arbitration: lands first.** Of several candidates, the one whose predicted arrival comes first is parried [D-20261005-36]. Equal arrivals fall back to today's lower tie-breaks: smaller angle, then shorter distance, then stable id (`DefenseResolver.cpp:119-146`). | — |
| Parry-5 | **The parry succeeds.** No lockout follows a success: a second incoming attack can be parried at once with a fresh press [D-20261005-72]. | — |

**Gates removed from today.** The parry succeeds on timing alone [D-20261005-64]. These are removed because the window is
input timing only and turning is never limited [D-20261005-05], because any direction can be parried [D-20261004-23], and
by D-20261005-64:
- the 70° hard cone;
- the reachability formula (tolerance plus turn budget × time left);
- angle allowances that widen with an early press;
- the prediction-confidence gate and the clear-path gate.

Obstacles matter only for paired content: when a paired counter or finisher has no room, by the existing distance and
room check, the single-character deflect-and-strike plays instead (3.3) [D-20261005-64].

**Contact: the predicted blade arrival** [D-20261005-73]. Contact is the moment the attacking blade is predicted to reach
the defender, computed at runtime from the swing trajectory. It is the window's end, the turn's finish and the moment
"lands first" compares. **How it is computed is proposed:**
- The attack's montage gives the swing's timeline: the Active phase start (the phase notify) and, when the attack
  carries one, an authored contact frame. Today the parry window's end stands in for that frame; the proposal is a
  single authored contact marker on the montage, with the Active start as the fallback.
- The swing trajectory is the weapon's trace path from its sockets (`WeaponStart`/`WeaponEnd`), advanced along the
  montage at its current play rate, against the defender's capsule. Today's prediction already publishes a source socket,
  a target bone and a predicted contact time every animation tick (**[reported]** `CombatComponent.cpp:2260-2374`); the
  predicted arrival replaces its "contact time equals window end".
- **What the runtime needs:** the attacker's montage position and effective play rate (including hitstop and the
  attacker slowdown, so arrival moves when the attacker is slowed); the weapon sockets and the defender's capsule; a
  re-evaluation each animation tick while the attack is in progress, with the latest value used at the press; and the
  attack's warp, since an attack that is still closing distance changes its arrival.
- The prediction is a timing reference, not a gate: it never decides whether the parry succeeds beyond fixing when the
  window closes [D-20261005-64, D-20261005-79].

**The parry window.**
- **Authoring (R-20261005-01, proposed; the owner may veto).** A window is authored as a **lead time** L_a before
  predicted arrival: a length only, never a start and an end, so an inverted window cannot be authored. The window is
  still a notify on the attacker's animation [D-20261005-09]; its notify carries the lead time.
- **Minimum:** never shorter than **0.35 s** before arrival [D-20261005-09].
- **Scale:** k, the defender's parry-assist window scale, default **1.0** [D-20261005-09, D-20261005-78].
- **Effective length L = max(0.35 s, L_a × k).** Scaling first and then applying the floor keeps both "never shorter
  than 0.35 s" and "scales every window" true (a reading, Appendix B).
- **Effective window:** [C − L, C], where C is the predicted arrival. The input buffer extends it before the opening,
  and the contact grace after C [D-20261004-06]; both default to 0 [D-20261005-78].
- **No authored window:** the attack gets the 0.35 s minimum at runtime, and the editor warns [D-20261005-74].
- **A window cannot open before its attack starts.** An attack whose arrival comes less than L after its start is
  parryable from its first frame. This follows from the arithmetic; it is not a decision (Appendix B).
- **Worked example.** The only shipped window is on `LightAttack_1`, authored today as 0.20–0.30 s, with Active starting
  at 0.30 s **[reported]**. As a lead time that is 0.10 s before an arrival predicted at about 0.30 s. With k = 1 the
  floor raises it to L = 0.35 s, so the window covers the attack from its first frame to its arrival.

**On success, in order:**
1. **The parried attack is consumed** for every target, as today **[reported: `CombatComponent.cpp:1997-2069`]**.
2. **The counter is chosen and its lethality judged now** (3.3): the content first, then its damage, then the outcome
   [D-20261005-38, D-20261005-40, D-20261006-03].
3. **Others hold** (3.9) [D-20261005-21].
4. **The defender turns to face the attacker** [D-20261005-05], independently of the window [D-20261005-79]:
   - θ is the angle from the defender's facing to the attacker at the press. T is the time from the press to the
     predicted arrival.
   - Rate: ω = θ ÷ T, so that the turn finishes exactly at arrival [D-20261005-50]. This is "the rotation rate adapts to
     the direction and the time left" [D-20261005-05].
   - ω is capped at the fastest natural turn, ω_max: a per-weapon cap on the defense set with a combat-settings default,
     **720°/s** to start, chosen from a rendered sweep [D-20261005-75].
   - With turn-deflect clips [D-20261004-30], the clip's play rate adapts the same way, under the same cap. Until those
     clips exist, the code-driven turn covers off-front parries [D-20261004-23].
   - **When θ ÷ ω_max > T**, so that even the fastest natural turn cannot face the attacker in time, the attacker's
     animation is slowed [D-20261005-10], as the safety valve [D-20261005-50]:
     - the slowdown **eases in and out**: a curve from 100% down to the needed speed and back [D-20261005-76];
     - its lowest speed s is no less than 0.5, and its whole duration d is no more than 0.3 s [D-20261005-33];
     - the time it buys is the area under the curve: ∫(1 − s(t)) dt over d. With the eases it is less than the
       0.3 × (1 − 0.5) = **0.15 s** a flat slowdown would buy; how much less depends on the ease shape;
     - the ease shape (the ease-in and ease-out lengths within d) is a combat-settings tunable, tuned on captures
       (proposed; D-20261005-76 fixes the shape's kind, not its numbers).
   - **If the slowdown is still not enough**, the turn plays at ω_max and the parry still succeeds [D-20261005-33].
     Motion warping inside the deflect hides what it can [D-20261004-30]; the rest is visible.
   - **The turn is never limited to the window** [D-20261005-05]. It may continue after the window closes, until
     arrival.
5. **Readability** [D-20261004-06]:
   - the parry has its own contact feedback (hitstop, camera kick, sound and effect), distinct from a block;
   - its animation differs from the block reaction;
   - the values are chosen from a rendered sweep before the parry stage ships [D-20261005-78].

   Today a parry uses the block's halved hitstop (**[reported]** `CombatComponent.cpp:184-190`), and on shipped data the
   same sound and effect as a block **[reported]**.
6. **Parry assists** are designer settings on the reaction profile, not debug switches [D-20261004-07]. Their defaults
   are neutral [D-20261005-78]:

   | Assist | Default |
   |---|---|
   | Window scale | 1.0 |
   | Input buffer | 0 |
   | Contact grace | 0 |
   | Slow motion while a window is open | off |
   | Telegraph | on |
   | Always perfect parry | off (not named in D-20261005-78; a reading, Appendix B) |

**Failure paths:**
- **No candidate, an attack incoming:** guard rises, and the press is a missed parry that starts the 0.25 s lockout
  [D-20261005-06, D-20261005-32, D-20261005-71].
- **No candidate, nothing incoming:** guard rises, with no lockout [D-20261005-71].
- **Locked out:** guard rises, with no parry now; the lockout is not extended unless the press is itself a miss
  [D-20261005-71]. The press is remembered, and it counts as a parry if the window is still open when the lockout ends
  [D-20261007-18].
- **An unparryable attack inside its window** is not a candidate [D-20261005-07]. The press is a guard, and it is a miss
  only if some other parryable attack aimed at the defender is in progress [D-20261005-71].
- **The attack touched first:** once an attack has touched the defender, blocked or hit, a later press cannot parry it
  (today).
- **Two attacks:** lands-first picks one. The other can be parried at once with a fresh press [D-20261005-72], or blocked
  [D-20261005-36].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Minimum parry window | 0.35 s before arrival | combat settings | D-20261005-09 | none; the one shipped window is 0.10 s |
| Window lead time | per attack, on the window notify (proposed authoring) | the attack's montage | D-20261005-09, R-20261005-01 | a start and end on the montage |
| Window scale (parry assist) | 1.0 | reaction profile, parry assists | D-20261005-09, D-20261005-78 | none |
| Input buffer, contact grace | 0, 0 | reaction profile, parry assists | D-20261004-06, D-20261005-78 | none |
| Slow motion on window, telegraph, always perfect parry | off, on, off | reaction profile, parry assists | D-20261004-07, D-20261005-78 | none |
| Parry lockout | 0.25 s, after a miss only; lenient: a press during it is honoured if the window is still open when it ends; to be felt first, and it may go to zero | combat settings | D-20261005-06, D-20261005-32, D-20261005-72, D-20261007-18 | none: a failed press is a free guard |
| Attacker slowdown, lowest speed | 50% | combat settings | D-20261005-33 | none |
| Attacker slowdown, longest time | 0.3 s | combat settings | D-20261005-33 | none |
| Attacker slowdown, ease shape | tuned on captures (proposed) | combat settings | D-20261005-76 | none |
| Fastest natural turn ω_max | 720°/s, from a sweep | combat settings default; defense set override | D-20261005-75, D-20261005-68 | guard facing turns at 180°/s |
| Parry contact feedback | from a rendered sweep | the attack's impact feedback for the parry outcome (4.5) | D-20261004-06, D-20261005-78 | the block's feedback |
| Unparryable flag | off: every attack is parryable | the attack | D-20261005-07, D-20261005-40 | opt-in: only `LightAttack_1` is tagged parryable |

**Changes from today:**
- **Windows:** at least 0.35 s, where today the only one is 0.10 s, measured back from a predicted arrival instead of the
  window's own end.
- **Direction:** any direction can be parried, with no angle gate. Pressing early no longer widens the angle.
- **Lockout:** a missed press locks out parries for 0.25 s, a success never does, and a press made during the lockout
  is honoured if the window is still open when it ends.
- **Unparryable** is an opt-out flag. Today parryable is an opt-in tag.
- **Lands first** replaces today's ranking by credible intent, then deadline.
- **Turning:** an adaptive turn that finishes at arrival, plus a bounded, eased attacker slowdown.

### 3.3 The parry-counter

**Decisions:**
- D-20261005-24: the parry is the counter, with one fluid sequence;
- D-20261007-11: the words: parry (the deflect), counter (the strike back), parry-counter (the one-press sequence);
- D-20261005-38: a lethal parry-counter ends in a kill, decided at the press, with no exemptions;
- D-20261007-07: every counter can kill; an authored kill sequence is optional and plays instead when one exists;
- D-20261006-03: counter damage on the defense-set entry, settings default 50, a clip override optional; content first,
  then damage, then the outcome;
- D-20261006-05: damage at the counter's contact, never clamped;
- D-20261006-02: the cancel notify (3.0);
- D-20261007-08: each character untouchable until its own cancel notify;
- D-20261007-09: a press made inside the sequence is buffered and fires at the player's cancel notify;
- D-20261007-10: a press aimed elsewhere does not end the opening; it supersedes that half of D-20261005-62;
- D-20261007-17: a light press with a Staggered target is the follow-up with the light opener; the hold does not open;
- D-20261007-12 and D-20261007-13: the aim cone and the locked target (3.13);
- D-20261005-39 and D-20261005-43: the follow-up strike, paired when a clip matches;
- D-20261005-42: any lethal aimed strike plays a paired finisher;
- D-20261005-45: counters keyed by direction and swing;
- D-20261005-47: paired finishers keyed by direction only, and no body types;
- D-20261005-48 and D-20261005-57: the defense set holds the counter entries, and the paired set holds the
  two-character counters and finishers;
- D-20261005-51: one follow-up per parry-counter, and Staggered ends at once after it;
- D-20261005-54: the lethal-counter fallback, as amended by D-20261007-07;
- D-20261005-80: counter damage with a settings default of 50; its home amended by D-20261006-03;
- D-20261005-81: Staggered starts when the counter sequence ends, read as the enemy's cancel notify;
- D-20261005-82 and D-20261005-85: the follow-up strike's ×1.5 and its 350 cm reach;
- D-20261005-84: the attack's swing field and the auto-detect action;
- D-20261005-22: the follow-up opportunity equals the Staggered time;
- D-20261005-23 and D-20261005-30: the single-character strike and the fallback order;
- D-20261005-08: one Staggered default;
- D-20261005-21: others hold;
- D-20261006-04: rules before content;
- D-20261005-40: the glossary;
- D-20261004-30: turn-deflects;
- D-20261004-03 and D-20261005-62: the player is fully free after a non-lethal counter, and the attacker is held;
- D-20261004-35: aim decides.

**One press, one sequence.** A Block press inside the parry window plays the parry-counter: the turn, the parry (the
deflect) and the counter, in one fluid sequence. The counter is never a separate press [D-20261005-24, D-20261007-11].
The word "parry" alone leaves room for a later perfect parry that does not counter; nothing in this spec plays one.
The owner asked for this to be "clear and airtight" (the 2026-10-07 log, E8); the sequence below is the one confirmed
there and in E9.

**What each press does:**

| Press | Not lethal | Lethal |
|---|---|---|
| Parry-counter (press 1) | The turn, the parry and the counter; the counter's damage lands at its contact; the enemy is Staggered from its own cancel notify. | The same sequence, and the enemy dies at the counter's contact; an authored kill sequence plays instead when the entry's counter has one. |
| Follow-up strike (optional press 2, while the enemy is Staggered) | The ×1.5 strike: a paired counter clip when one matches the direction, otherwise the fallback strike; the enemy recovers. | A paired finisher from the finisher pool; the strike plays unpaired with the normal death when no clip matches. |

**The sequence, in order:**

| # | Step | Rule |
|---|---|---|
| Seq-1 | **The press.** A Block press passes 3.2, and the parried attack is consumed for every target. | D-20261005-24; consumption as today **[reported: `CombatComponent.cpp:1997-2069`]** |
| Seq-2 | **Content.** The attacker's direction (front, left, right or behind: today's four-way classifier, `CombatMath.h:53-56`, the dominant axis winning, so each direction spans 90°; keeping those boundaries is proposed), the parried attack's swing (its `Swing` field, 4.5), the variety rule and the room check pick one counter entry and the content it will play (below). | D-20261005-45, D-20261005-84, D-20261005-30, D-20261005-64, D-20261006-03 |
| Seq-3 | **Damage.** The counter's damage is the chosen clip's override when it has one, else the entry's value, else the combat settings' default of **50** (the heavy attack's damage). Resolved damage is that value times the weapon multiplier and the attacker's resistance (2.2). | D-20261006-03, D-20261005-80 |
| Seq-4 | **Outcome.** Lethal if the resolved damage is ≥ the attacker's current health. Decided now, at the press, so the right content starts at once. | D-20261005-38, D-20261005-40, D-20261006-03 |
| Seq-5 | **The turn and the parry.** The defender turns (3.2, "On success", item 4) and the deflect meets the blade at the predicted contact. | D-20261005-05, D-20261005-73 |
| Seq-6 | **The counter's contact.** The damage applies here, in full. It is never clamped: a counter is not chosen on a lethal press and then held back. | D-20261006-05 |
| Seq-7a | **Lethal.** The enemy dies at that contact; its death outcome takes over from its victim half of the clip (3.11), with today's death-handling flag (**[reported]** `HitReactionComponent.cpp:1518-1533`). When the entry's counter carries an authored kill sequence, that plays instead, from the press. No lethal variant is required. | D-20261007-07 |
| Seq-7b | **Not lethal.** The enemy becomes Staggered at its own cancel notify, for the parried attack's Staggered time (3.4); the rest of its clip is its Staggered reaction. | D-20261005-81, D-20261006-02, D-20261007-07 |
| Seq-8 | **Protection.** Each character is untouchable until its own cancel notify (3.0): the player is free and hittable from theirs; the enemy is open from its own. | D-20261007-08 |
| Seq-9 | **The player's cancel notify.** From it the player is fully free: move, guard, attack anywhere, be hit. Attack presses made inside the sequence are buffered and fire now, last input wins. | D-20261005-62, D-20261004-03, D-20261007-09 |

Others hold throughout the sequence [D-20261005-21].

**Choosing the content (Seq-2).** From the defender's weapon's defense set, its counter pool [D-20261005-24,
D-20261005-48]; each entry refers to a counter in the weapon's paired set [D-20261005-57]. The fallback order is
D-20261005-30's, which D-20261005-45 confirms:
1. an entry for this direction and this swing. An entry that also names the parried attack's lane beats one that does
   not (the optional refinement of D-20261005-45; the ordering is proposed);
2. an entry for this direction and any swing;
3. an entry for any direction, warped to fit;
4. the fallback strike of the entry chosen by steps 1 to 3, with the enemy's Staggered reaction [D-20261005-23]. Each
   entry carries its own fallback strike, per direction and swing [D-20261005-48]; an empty counter pool fails
   validation (4.6).

- The editor warns about missing coverage [D-20261005-30].
- **No room.** A paired counter whose distance and room check fails plays as the entry's fallback strike, the
  single-character deflect-and-strike [D-20261005-64]. The check is part of choosing the content, so the damage read in
  Seq-3 is the one the fallback strike deals (a reading, Appendix B).
- **Variety.** When several entries match at one step, the proposal reuses today's variety rule for reaction variants:
  one entry always plays; two alternate; three or more are random, excluding the last two
  (**[reported]** `HitReactionComponent.cpp:1292-1358`).
- **A lethal press** plays the chosen counter's authored kill sequence when it has one; otherwise the counter itself,
  and the enemy dies at its contact [D-20261007-07]. The fallback strike kills the same way. The finisher pool is not
  consulted for a parry-counter: D-20261005-54's middle step is read as superseded by D-20261007-07 (Appendix B).

**Damage (Seq-3)** [D-20261006-03, D-20261005-80]. Three homes, in order of precedence:
1. a paired clip's own override, when it has one (4.7);
2. the defense-set entry's value, per direction and swing (4.6);
3. the combat settings' default, **50**.

The fallback strike always deals the entry's value; a clip override never reaches it [D-20261006-03]. Content is
resolved before damage so that the clip's override can be read, and damage before the outcome so that lethality is known
at the press [D-20261006-03]. The damage applies at the counter's contact, in full [D-20261006-05].

**How many presses.** "Weak enemies take one press, tough ones two" [D-20261005-24] is narrowed by D-20261005-39. A
tough enemy takes several parry-counter-and-strike exchanges, because health always decides.

**After the sequence: the opening.** The Staggered enemy is an ordinary opening, and nothing more (the 2026-10-07 log,
E8–E9):
- it is held for its Staggered time (3.4);
- there is **no latch** on it and **no end-position rule**: the player is not bound to the parried enemy, and where the
  clips end changes no rule;
- a press aimed at someone else attacks that someone. It does **not** end the opening: Staggered runs its time
  [D-20261007-10]. The aimed-elsewhere rule of D-20261005-62 is retired;
- the follow-up strike is optional.

**The follow-up strike.** Evaluated when an attack press executes, in order:

| # | Condition | If it fails |
|---|---|---|
| FollowUp-1 | The press is Light or Heavy. A reading: Special and Evade resolve to no attack today (Appendix B). | Not a follow-up. |
| FollowUp-2 | The target, by 3.13: in free mode the aim cone around the stick direction, or the facing when no direction is held, with Staggered or eligible enemies winning ties and the nearest otherwise; in lock mode the locked target [D-20261007-12, D-20261007-13]. The follow-up strike reaches **350 cm**, the same as a paired finisher [D-20261005-85]. | An ordinary attack. |
| FollowUp-3 | The chosen target is Staggered. | An ordinary attack, and the paired-finisher rule (3.8) applies. |
| FollowUp-4 | **The strike.** A light press plays the light opener as the follow-up, from the initial press: the hold window does not open, and a directional light never follows up [D-20261007-17]. A Heavy press is Q19. | — |
| FollowUp-5 | **Damage:** ×1.5 the resolved damage of the attack the press resolves to [D-20261005-39, D-20261005-82]. Lethal is decided at the press [D-20261005-38, D-20261005-39, D-20261005-40]. | — |
| FollowUp-6 | **Lethal:** the paired finisher from the finisher pool for the attacker's direction; with no matching clip the strike plays unpaired and the enemy dies with its normal death reaction [D-20261005-47]. **Not lethal:** a paired-set counter for the target's direction (any swing) when one exists, otherwise the fallback strike of the defense set's entry for that direction, at ×1.5 [D-20261005-43, D-20261005-48, D-20261005-57]. Staggered ends immediately, and there is one follow-up per parry-counter [D-20261005-51]. | — |

- **A press inside the counter sequence** is buffered and fires as the follow-up the moment the player's cancel notify
  fires; last input wins [D-20261007-09].
- **No press:** the enemy recovers when its Staggered time ends.
- **One follow-up per parry-counter** [D-20261005-51]. After a non-lethal follow-up strike the enemy is no longer
  Staggered, so a second press is an ordinary attack.
- **Which enemies.** The follow-up strike applies to any Staggered enemy, including one Staggered by a guard break: it
  is "the ×1.5 strike from an aimed attack press while the target is Staggered" [D-20261005-40, D-20261005-39].

**Hand-off timing.** Two things are measured in the rendered parry-counter stage and reported with captures, not decided
here:
- when the attacker leaves its own attack animation for its half of the counter entry;
- how the deflect lines up with contact.

**Content this needs** (the shopping list; no rule above depends on it [D-20261006-04]):
- **Counter entries.** For each direction (front, left, right, behind) and swing (slash left, slash right, overhead,
  thrust), one defense-set entry and the paired-set counter it refers to: one deflect flowing into a counter that can
  kill. An authored kill sequence is optional, chosen at the press when the counter has one; it is never a separate
  finisher clip played after a non-lethal counter [D-20261005-38, D-20261005-45, D-20261005-48, D-20261005-57,
  D-20261007-07].
- **Turn-deflects.** 90° left and right, and 180° behind, each flowing into a front pair [D-20261004-30]. The decision
  says the 90° turns are mirrorable, but mirroring is not possible in the project today: there is no mirror data table
  and no Mirror node **[reported, from the turn speed sweep]**. Under rules before content, that is met with tooling,
  not a narrower rule [D-20261006-04].
- **The single-character strike**, and the enemy's Staggered reaction [D-20261005-23].
- **The finisher pool**, one paired finisher per attacker direction in the paired set [D-20261005-47, D-20261005-57].
  There are no body types [D-20261005-47].
- **A cancel notify** on every clip above (3.0, 4.13) [D-20261006-02].

What is owned today is a content finding and sits with the stage that builds this (9.7).

**Changes from today:**
- **Presses.** Today a parry plays a two-character deflect animation, then waits up to 2.0 s for a press that plays a
  counter, then up to 2.0 s more for a press that plays the kill. Now one press plays the whole parry-counter, plus an
  optional follow-up.
- **Counter choice.** Today the counter is the defender's own default attack's counter data
  (`PairedAnimationComponent.cpp:4288-4321`), so a Heavy press can never counter **[reported]**. Now the counter comes
  from the parried direction and swing.
- **Lethality.** Today a counter never kills: it is capped at health − 1 unless a component flag is set **[reported]**.
  Now a lethal counter kills at its contact, never clamped, and an authored kill sequence plays instead when one exists.
- **The opening.** Today's post-parry windows bind the player to the parried enemy. Now the Staggered enemy is an
  ordinary opening: no latch, no end-position rule, and a press elsewhere leaves it running.
- **No-content fallback.** Today a parry without counter content does nothing more **[reported]**. Now the
  single-character strike always plays.
- **Other enemies.** Today every other enemy aborts its swing **[reported]**. Now swings in progress finish (3.9).

### 3.4 Staggered

**Decisions:**
- D-20261004-35: the name;
- D-20261005-08: one default, 1.5 s, overridable per attack;
- D-20261005-14: the player's guard break;
- D-20261005-15: counted guard break;
- D-20261005-22: the follow-up opportunity equals the Staggered time;
- D-20261005-31: a guard break uses the same default;
- D-20261005-39: Staggered alone grants no kill;
- D-20261005-51: Staggered ends at once after a non-lethal follow-up strike;
- D-20261005-88: the player's Staggered state: the reaction and the push, no attack, guard or stick movement, a dodge
  ends it after a minimum, 0.6 s by default; it supersedes D-20261005-67;
- D-20261005-81: Staggered starts when the non-lethal counter sequence ends;
- D-20261006-01: hit-stun and Staggered both run, each with its own timer and rules;
- D-20261006-06: the player's own Staggered time always applies to the player;
- D-20261006-02 and D-20261007-07: the enemy's Staggered starts at its own cancel notify;
- D-20261007-10: a press aimed elsewhere does not end it;
- D-20261007-11: the words;
- D-20261005-40: the glossary.

**Causes.** Only these two [D-20261005-40]:
1. a non-lethal parry-counter (3.3);
2. a guard break (3.5).

**Duration.** The Staggered time of the attack that caused it:
- one default for every attack, **1.5 s**, in the combat settings;
- overridable per attack, and per slot (4.2) [D-20261005-08, D-20261005-31].

**The player's length is the player's own:** **0.6 s** by default, on the player's reaction profile (4.8)
[D-20261005-88]. It always applies to the player: an attack's or a slot's Staggered-time override never changes it
[D-20261006-06]. Enemies keep the 1.5 s default and take the causing attack's override.

For a parry-counter, the causing attack is the parried attack. For a guard break, it is the blocked attack that broke the
guard; that is a reading (Appendix B).

**Start.** For a parry-counter, at the enemy's own cancel notify on its half of the counter: that is where "the counter
sequence ends" [D-20261005-81, D-20261006-02, D-20261007-07]. For a guard break, at the breaking contact; that is a
reading.

**Clock.** Q8.

**While an enemy is Staggered:**
- it starts no attack and does not move: it is held [D-20261004-03]. Today the AI cannot start an attack while staggered
  (`EnemyCombatAIComponent.cpp:1140-1152`);
- a strike lands without reply [D-20261005-40];
- an aimed attack press on it is a follow-up strike (3.3) [D-20261005-39].

**Staggered ends:**
- when its time runs out;
- immediately when a non-lethal follow-up strike lands; there is one follow-up per parry-counter [D-20261005-39,
  D-20261005-51];
- on death.

A press aimed at someone else does **not** end it: Staggered runs its time [D-20261007-10]. The release by an
aimed-elsewhere press [D-20261004-35, D-20261005-62] is retired.

**While the player is Staggered** [D-20261005-88, which supersedes D-20261005-67]:
- the guard-break reaction plays, and the breaking hit's full push carries the player (3.5, 3.10) [D-20261005-59];
- there is no attack, no guard and no stick movement until it ends;
- **a dodge press ends it early**, by the same rule as the hit-stun escape (3.7): never before the 0.12 s floor and never
  within the first half, so the escape opens at max(0.12 s, S ÷ 2) after the breaking contact, where S is the player's
  Staggered length; that is 0.3 s at the default. The escape dodge is itself committed. A dodge press made before the
  escape opens is buffered and fires when it opens;
- **dodging out ends the Staggered state itself**, so the enemy's opening closes with it: the dodge reads as ending the
  stagger, and nothing of the state outlives it;
- otherwise it ends when the player's Staggered length runs out;
- attack and Block presses made meanwhile are buffered and fire afterwards. That carries over from D-20261005-67 and is a
  reading (Appendix B).

D-20261005-88 names the guard break. No rule in this spec has an enemy play a parry-counter against the player, so the
guard break is the only
way the player becomes Staggered here. Enemy attacks on a Staggered player are ordinary attacks, because the follow-up
strike is the player's press [D-20261005-39]. Enemy paired finishers are covered in 3.8.

**Beside hit-stun** [D-20261006-01]. Hit-stun and Staggered on one character both run: each keeps its own timer and its
own rules (no guard while Staggered, whatever the hit-stun allows: Guard-2 takes precedence over Guard-4, a reading,
Appendix B); the character is free when both have ended; a dodge-out needs both minimums to have passed; a hit never
shortens a lock and never restores guard early.

**Being Staggered again while Staggered:** the new time replaces the remaining time, as today
(`HitReactionComponent.cpp:1439-1466`; section 3.11).

**Staggered alone grants no paired finisher** [D-20261005-39].

**One state, one number.** Today there are two separate staggers: the hit-reaction stagger and the AI component's
Staggered state, each 1.5 s on its own timer **[reported]**. They become one state with one duration
[D-20261005-22: "one number"].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Staggered time | 1.5 s, the same for every attack | combat settings; per-attack and per-slot overrides | D-20261005-08, D-20261005-31 | 1.5 s on the **attacker's** defense configuration, plus 1.5 s on the AI component, plus a 1.5 s constant |
| The player's Staggered time | 0.6 s; always the player's own, never an attack's or a slot's override | the player's reaction profile | D-20261005-88, D-20261006-06 | none: nothing Staggers the player today |
| Staggered dodge escape (player) | opens at max(0.12 s, half the player's Staggered time) | rule, no tunable | D-20261005-87, D-20261005-88 | none |

### 3.5 Guard break

**Decisions:**
- D-20261004-43: keep the contextual Staggered state and add a simple guard break;
- D-20261005-14: the player's guard breaks only to guard breakers;
- D-20261005-15: count blocked hits, per character;
- D-20261005-25: count by pressure;
- D-20261005-26: 4 within 3 s;
- D-20261005-29: guard breakers;
- D-20261005-31: the Staggered time;
- D-20261005-12: no chip damage;
- D-20261005-20: no enemy paired finisher because of a guard break;
- D-20261005-52: the breaking hit deals no damage;
- D-20261005-56: counting is on in the base profile and off for the player;
- D-20261005-59: the breaking hit delivers the attack's full push;
- D-20261005-83: a sliding window, cleared on a break;
- D-20261005-86: a guard break grants an enemy nothing;
- D-20261007-15: the counted guard break is built now, tested on fixtures, and dormant until a shipped enemy guards;
- D-20261007-16: unblockable wins over guard breaker, and validation warns on the pair;
- D-20261005-40: the glossary.

There is no meter and no damage-derived stagger [D-20261005-15].

An unblockable attack never reaches the table below: Contact-4 lands it as a hit through guard, so an attack flagged
both unblockable and guard breaker breaks nothing. Validation warns on the pair of flags (4.4) [D-20261007-16].

**Evaluated after a contact is decided as a block (Contact-7), in order:**

| # | Condition | Result |
|---|---|---|
| Break-1 | The blocked attack's resolved guard-breaker flag is on. | **Guard break, immediately**, whoever the defender is [D-20261005-29]. |
| Break-2 | The defender is the player. | No guard break. The player's guard breaks only to guard breakers [D-20261005-14, D-20261005-40]. |
| Break-3 | The defender's reaction profile has guard-break counting switched on [D-20261005-46]. The attack's resolved pressure is added to the guard-break count at the contact time t [D-20261005-25]. The window is sliding: at each blocked hit the count is the sum of the pressure of blocked hits in the last W = 3 s [D-20261005-83]. | If the count ≥ N: **guard break** [D-20261005-26]. |

- **N** is 4 pressure points and **W** is 3 s by default. Both are set per character, through the base profile and
  per-character overrides [D-20261005-15, D-20261005-26].
- **Counting is a per-character toggle** in the reaction profile: on in the base profile, off in the player's, so the
  player's guard breaks only to guard breakers [D-20261005-14, D-20261005-46, D-20261005-56].
- The option the owner picked describes these breaks within 3 s (session transcript, answered 03:57 EDT): four lights;
  two heavies; a heavy and two lights.

**Guard-breaker defaults** [D-20261005-29], overridable per attack and per slot:

| Attack | Guard breaker by default |
|---|---|
| Charged heavy | yes |
| Directional light | yes |
| Everything else | no |

"Directional light" is not an attack type. The proposal applies this default to attacks reached through a hold-and-release
link (Appendix B).

**On a guard break:**
- **The breaking hit is still a blocked hit**, so it deals no damage [D-20261005-52].
- **Guard ends.**
- **The defender becomes Staggered** for the breaking attack's Staggered time [D-20261005-31, D-20261005-52]. A player
  whose guard breaks is Staggered for the player's own time, and a dodge ends it early (3.4) [D-20261005-88].
- **The breaking hit delivers the attack's full push**, not the block shove (3.10) [D-20261005-59].
- **The guard-break count clears** [D-20261005-83].
- **No enemy paired finisher** comes from it by itself; if a marked enemy's next hit on the Staggered player would be
  lethal, the normal lethal rule still applies [D-20261005-20, D-20261005-86] (3.8).

**What does not count:**
- unblockable hits, hits outside the arc and hits while not guarding, none of which is a block;
- parries.

**Who it applies to.** Any character whose guard is up. Enemies do not guard today **[reported]**. Enemy guarding is not
designed, and guard-breaker enemy types come later [D-20261004-27].

**Built dormant.** The counted guard break (Break-3) is built with the guard-break stage (9.5) and proven on fixtures
whose enemy guards; it stays dormant in shipped play until a shipped enemy guards [D-20261007-15]. The guard-breaker
break (Break-1) is live for the player from that stage.

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Guard-break threshold N | 4 pressure points | reaction profile: base, with per-character overrides | D-20261005-26 | nothing breaks a guard |
| Guard-break window W | 3 s | reaction profile: base, with per-character overrides | D-20261005-26 | — |
| Guard-break counting toggle | on in the base profile; off for the player | reaction profile | D-20261005-46, D-20261005-56 | — |
| Guard breaker | see the defaults table above | combat settings type defaults; attack and slot overrides | D-20261005-29 | — |
| Counted guard break | built and tested on fixtures; dormant until a shipped enemy guards | rule, no tunable | D-20261007-15 | — |

### 3.6 Interruption and hit look

**Decisions:**
- D-20261004-42: the attack type decides the look, and pressure decides interruption;
- D-20261005-16 and D-20261005-17: pressure against armor;
- D-20261005-44: current armor is the higher of the base and any active window;
- D-20261006-02: the reaction's cancel notify;
- D-20261005-40: the glossary.

**Scope.** Hits that land. Not blocks, not parries, and not damage applied by a paired animation.

**Look.** Decided by the attack type [D-20261004-42]:
- Light: a light reaction;
- Heavy, charged or not: a heavy reaction;
- Special: Q2.

The direction is classified as today: front, back, left or right of the victim
(**[reported]** `HitReactionComponent.cpp:645-655`).

**Interrupt rule** [D-20261005-17, D-20261005-40]:

```
interrupts  ⇔  P > A
P = the hit's resolved pressure:
    type default (light 1, heavy 2, charged 3) → set-level type default → attack override → slot override
A = the victim's current armor:
    max(base armor from the reaction profile (default 0), the level of any active armor window on the victim's animation)
    [D-20261005-44]
```

**Examples:**
- Base 0 and no window: every hit interrupts.
- A swing whose armor window is at 2, the brute example in the option the owner picked (session transcript, answered
  03:38 EDT): light (1) and heavy (2) flinch, and charged (3) interrupts.
- Base 1 with a window at 2: armor is 2, not 3. Windows set a level; they do not add [D-20261005-44].

**An interrupting hit:**
- stops the victim's current action. An interrupted attack ends, as today (**[reported]**
  `CombatComponent.cpp:5810-5831`);
- plays the full directional reaction, with the look above. The reaction's cancel notify (3.0) lets the character act
  again from there, once hit-stun has also ended; a reaction with no notify ends at montage end [D-20261006-02];
- applies hit-stun (3.7);
- applies knockback (3.10).

**A hit that does not interrupt:**
- plays the additive flinch over the current action, which continues [D-20261004-42];
- applies its damage;
- applies **no** hit-stun, which comes only "when a hit interrupts" [D-20261005-34];
- applies **no** knockback [D-20261005-58].

**Removed:**
- the damage ÷ health rule, `HeavyDamageHealthPercent`. Today, at shipped values, every weapon hit plays a heavy reaction
  **[reported]** [D-20261004-42].
- per-phase resistance from D-20261003-02, which armor windows replace (Appendix B).

**Armor windows** are authored on a character's animations [D-20261005-17]. The proposal is a notify state that carries
an armor level.

**Unchanged:** the super-armor and invulnerability flags (3.11).

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Pressure | light 1, heavy 2, charged 3 | combat settings type defaults; attack and slot overrides | D-20261005-17 | none: every hit that plays a reaction can interrupt |
| Base armor | 0 | reaction profile | D-20261005-17 | none |
| Armor window level | authored on each window | the animation | D-20261005-17 | none |

### 3.7 Hit-stun

**Decisions:**
- D-20261005-34: the defaults and the floor;
- D-20261005-37: what hit-stun blocks;
- D-20261004-41: the attack owns hit-stun;
- D-20261005-40: the glossary;
- D-20261003-01: it applies in the current reaction mode;
- D-20261003-03: buffered input;
- D-20261005-18: hit-stun is no longer a finisher route;
- D-20261005-53: it applies to enemies as to the player;
- D-20261005-87: a dodge press ends the player's hit-stun early, after a minimum;
- D-20261006-01: hit-stun and Staggered both run;
- D-20261006-02: the hit reaction's cancel notify.

**When.** An interrupting hit (3.6).

**Duration:**

```
H = max(0.12 s, resolved hit-stun)
resolved hit-stun = type default (light 0.25 s, heavy 0.45 s, charged 0.6 s)
                    → set-level type default → attack override → slot override
```

The floor is 0.12 s: never below it [D-20261005-34].

**During hit-stun** [D-20261005-37]:
- attacks and movement wait;
- guard can be raised (3.1), and that press can parry [D-20261005-77];
- input is buffered and fires afterwards [D-20261005-37, D-20261003-03]. The buffer is today's last-input-wins queue;
- **a dodge press ends the player's hit-stun early** [D-20261005-87]: never before the 0.12 s floor, and never within the
  first half of the stun, so the escape opens at max(0.12 s, H ÷ 2) after the hit. The escape dodge is itself committed:
  it plays through. A dodge press made before the escape opens is buffered and fires when it opens. The dodge itself is
  not designed here; today the Evade input does nothing (`CombatComponent.cpp:4147-4149` **[reported]**).

**Beside Staggered** [D-20261006-01]. When a character is both in hit-stun and Staggered, both run: each keeps its own
timer and rules; the character is free when both have ended; a dodge-out needs both minimums to have passed; a hit
never shortens a lock or restores guard early. The hit reaction's cancel notify (3.0) frees the character only once
hit-stun has also ended [D-20261006-02].

**Who it applies to.** Every character, the same way: no new attack and no movement until it ends, and the enemy AI
resumes afterwards [D-20261005-53]. Today hit-stun stops nothing: neither the player's input nor the AI
(`EnemyCombatAIComponent.cpp:1140-1152`).

**A second interrupting hit during hit-stun:** Q3. Today the new duration overwrites the old one, even when it is shorter
(`HitReactionComponent.cpp:540-558`).

**Other rules:**
- hit-stun no longer makes a character finishable [D-20261005-18];
- the hit-reaction entries' stun field is removed, and the attack owns hit-stun [D-20261004-41];
- hit-stun applies in the current reaction mode, with no mode switch [D-20261003-01].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Hit-stun | light 0.25 s, heavy 0.45 s, charged 0.6 s | combat settings type defaults; attack and slot overrides | D-20261005-34, D-20261004-41 | 0 on every shipped reaction entry, so nothing stuns **[reported]** |
| Hit-stun floor | 0.12 s | combat settings | D-20261005-34 | none |
| Hit-stun dodge escape | opens at max(0.12 s, half the stun) | rule, no tunable | D-20261005-87 | none |

### 3.8 Paired finishers

**Decisions:**
- D-20261005-18, narrowed by D-20261005-39: eligibility;
- D-20261005-19: the trigger;
- D-20261005-20: enemy paired finishers;
- D-20261005-21: others hold;
- D-20261005-27, as replaced by D-20261007-12 and D-20261007-13: the aim cone and the locked target (3.13);
- D-20261005-28: 350 cm;
- D-20261005-38 and D-20261005-39: lethal only;
- D-20261007-07: a lethal parry-counter kills at the counter's contact, with an authored kill sequence when one exists;
- D-20261005-42: any lethal aimed strike;
- D-20261005-47: keyed by direction only, no body types, the normal death when no clip matches;
- D-20261005-57: the finishers live in the weapon's paired set, referenced by the combo set;
- D-20261005-40: the glossary.

**Rule.** A paired finisher plays whenever an aimed strike's resolved damage against its target is lethal: a
parry-counter, a follow-up strike on a Staggered enemy, or an ordinary attack. That is decided at the press
[D-20261005-38, D-20261005-39, D-20261005-42]. Staggered on its own does not grant one [D-20261005-39]. No health
percentage is involved [D-20261005-18].

**Which strikes can play one** [D-20261005-42]:

| Strike | Decision |
|---|---|
| The counter of a parry-counter (3.3): it kills at its own contact, and the entry's authored kill sequence plays instead when one exists | D-20261005-38, D-20261007-07 |
| A follow-up strike on a Staggered enemy (3.3) | D-20261005-39 |
| Any other attack press aimed at an enemy, when its resolved damage is lethal | D-20261005-18, D-20261005-19, D-20261005-42 |
| A marked enemy's attack on the player, when its resolved damage is lethal | D-20261005-20, D-20261005-42 |

**The player's trigger.** Evaluated when an attack press executes, in order. When a buffered press is decided is Q5.

| # | Condition | If it fails |
|---|---|---|
| Trigger-1 | The press is Light or Heavy. A reading: Special and Evade resolve to no attack today (Appendix B). | — |
| Trigger-2 | **Target, by 3.13.** In free mode, candidates are hostile, alive, not dying and not in a paired animation, within **60° either side of the stick direction**, or of the character's facing when no direction is held [D-20261005-27, D-20261007-12]; a Staggered or eligible (lethal) enemy wins ties, and the nearest wins otherwise [D-20261007-12, D-20261005-40]. In lock mode the target is the locked one [D-20261007-13]. A paired finisher needs its target within **350 cm** [D-20261005-28] (measured between the two characters' locations, as today); a non-lethal follow-up strike reaches the same 350 cm [D-20261005-85]. | No paired finisher; the attack plays normally. |
| Trigger-3 | **The strike.** If the target is Staggered, the follow-up strike at ×1.5 (3.3). Otherwise, the attack the press resolves to [D-20261005-42]. | — |
| Trigger-4 | **Lethal check:** resolved damage ≥ the target's current health. For a Heavy press, Q4. | The strike plays normally. |
| Trigger-5 | **Content and start checks.** The paired finisher comes from the attacker's weapon's paired set, through the combo set's finisher references, keyed by the attacker's direction relative to the victim (front, left, right or behind) and nothing else [D-20261005-47, D-20261005-57]. With no clip for that direction, the strike plays unpaired and the victim dies with its normal death reaction; the editor warns on the missing direction [D-20261005-47]. Today's start checks stay: a clear path (a 30 cm sweep), and neither character already in a paired animation (**[reported]** `PairedAnimationComponent.cpp:3863-3998`) [D-20261005-64]. | When a start check fails, the strike plays unpaired and the victim dies its normal death (3.11). |

**During a paired finisher:**
- others hold [D-20261005-21];
- the victim's death is in the clip.

**Enemy paired finishers on the player** [D-20261005-20]. All of these must hold:
- the enemy's reaction profile carries the "can finish the player" marker;
- the enemy's attack would be lethal to the player [D-20261005-42];
- a guard break grants nothing by itself; a marked enemy's lethal hit on the Staggered player still finishes
  [D-20261005-20, D-20261005-86];
- the enemy is within 350 cm [D-20261005-28].

**There is no "can be finished" marker.** Health alone decides, with no per-character exemption, and a victim with no
matching clip simply dies its normal death [D-20261005-38, D-20261005-47]. There are no body types [D-20261005-47].

**Removed from today:**
- the 25% low-health threshold (`HitReactionComponent.cpp:1423`);
- hit-stun and Staggered as routes on their own;
- choosing the one best soft-aim target in the character's facing direction, at 500 cm and 120°
  (**[reported]** `PairedAnimationComponent.cpp:2777-2799`). This becomes the aim cone of 3.13, around the stick
  direction or the facing, at 350 cm for a paired finisher;
- finisher data on every attack, replaced by the finisher pool;
- every shipped enemy attack carrying lethal finisher data **[reported]**, replaced by marked enemy types only.

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Aim cone (half-angle) | 60°, around the stick direction or the facing (3.13) | combat settings | D-20261005-27, D-20261007-12 | soft aim from facing: 120° rejection, scored on 45° |
| Finisher range | 350 cm | combat settings | D-20261005-28 | 500 cm, `SoftAimRange` |
| Can finish the player | off | reaction profile | D-20261005-20, D-20261005-40 | every enemy attack can |

### 3.9 Several attackers

**Decisions:**
- D-20261005-35: up to 2 attackers;
- D-20261005-21: others hold;
- D-20261005-36: lands first;
- D-20261007-08: untouchable until your own cancel notify;
- D-20261004-01: enemy coordination comes after the player's combat mode.

**Attack tokens:**
- At most **N = 2** enemies attack the player at once by default. N is configurable, in the combat settings
  [D-20261005-35]. Today N is 1, global and C++ only (`CombatTokenSubsystem.h:41`).
- Each enemy has **a short cooldown** between attacks [D-20261005-35]. Its value, and when it starts, are Q6. Today it is
  1.0 s, counted from the grant (`CombatTokenSubsystem.h:45`).
- Waiting enemies queue first in, first out, as today.

**Others hold** [D-20261005-21]:
- While the player performs a parry-counter or a paired finisher, no other enemy starts a new attack.
- Swings already in progress finish.
- The enemies resume when it ends.
- Today every enemy targeting the player aborts its swing instead **[reported]**.
- An in-progress swing cannot touch the player during a parry-counter until the player's own cancel notify
  [D-20261007-08]; during a paired finisher, today's rule holds: the character that started it cannot be touched (3.11).
  The swing's contact on an untouchable character is ignored and the swing continues; that is a reading (Appendix B).
  D-20261005-36's fresh press for the other attack therefore applies before the sequence starts, or after the player's
  cancel notify.
- Whether other enemies hold during a non-lethal follow-up strike is Q7.

**Two parryable attacks** [D-20261005-36]. A parry press answers whichever lands first (3.2, Parry-4). The other can be parried
at once with a fresh press, since a success starts no lockout [D-20261005-72], or blocked.

**Not in this spec.** Wider enemy coordination, such as attack slots and orbits, comes after the player's combat mode
[D-20261004-01].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Most enemies attacking at once | 2 | combat settings | D-20261005-35 | 1, C++ only |
| Per-enemy cooldown | Q6 | combat settings | D-20261005-35 | 1.0 s from the grant |

### 3.10 Knockback

**Decisions:**
- D-20261004-38, D-20261001-07, D-20260930-02 and D-20260930-01: the layering, the code seeds, per-attack distance and
  duration, and the victim's scale;
- D-20261005-58: no push on a hit that does not interrupt;
- D-20261005-59: a block shove on a blocked hit, and the full push on the hit that breaks a guard;
- D-20261005-60: normal pushes on victims that are Staggered or in hit-stun, and none during paired sequences.

**Push by outcome:**

| Outcome | Push | Decision |
|---|---|---|
| Interrupting hit | The attack's resolved knockback: distance, duration, direction mode, speed profile and blend (3.6). | D-20261004-38 |
| Flinch (a hit that does not interrupt) | None. | D-20261005-58 |
| Blocked hit | The **block shove**: a short push with a per-type default, in the attack's push direction. | D-20261005-59 |
| The hit that breaks a guard | The attack's full knockback, together with Staggered (3.5). | D-20261005-59 |
| A hit on a victim that is Staggered or in hit-stun | As the hit's outcome above: the full push if it interrupts. Being Staggered or in hit-stun changes nothing. | D-20261005-60 |
| Any contact during a paired sequence (a counter or a paired finisher) | None. The sequence's authored motion replaces it. | D-20261005-60 |
| Unblockable hit through guard | As an interrupting hit, if it interrupts (3.11). | today |

**Layering** [D-20261004-38]:
- **Per-type defaults** live in the combat settings. Each attack overrides them field by field, and each slot overrides
  the attack [D-20261005-01]. The code values only seed a new settings asset. The combo set's type default (4.2) is the
  per-weapon layer noted for later [D-20261005-61].
- **Code seeds for the hit push:** Light 25 cm in 0.2 s; Heavy 20 cm in 0.25 s (`CombatSettings.cpp:10-21`)
  [D-20261001-07]. The owner's held settings asset sets other values (7.6).
- **Scaling.** Distance scales with charge and with the victim's `KnockbackScale` (`KnockbackResolution.cpp:30-38`)
  [D-20260930-01]. The block shove scales the same way (proposed).

**The block shove** [D-20261005-59]:
- a per-type default in the combat settings, overridable per attack and per slot like the hit push (the overrides are
  proposed; D-20261005-59 names only the per-type default);
- **Proposed starting values, not decided:** 40% of the type's hit-push distance over the type's push duration, which
  from the code seeds gives Light **10 cm in 0.2 s** and Heavy **8 cm in 0.25 s**. They are tuned on rendered captures,
  and a blocked light must read as lighter than a blocked heavy;
- the shove uses the attack's push direction (away from the attacker by default) and the same displacement executor.

**Today:** a push starts only when a full reaction starts (`HitReactionComponent.cpp:491-500`), so interrupting hits
push, and nothing else does. The block shove and the guard-break push are new.

**Slopes** are verified by a rendered capture [D-20261004-40].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Hit push | Light 25 cm / 0.2 s, Heavy 20 cm / 0.25 s (code seeds) | combat settings → set → attack → slot | D-20261004-38 | the same |
| Block shove | Light 10 cm / 0.2 s, Heavy 8 cm / 0.25 s (**proposed**) | combat settings → set → attack → slot (overrides proposed) | D-20261005-59 | none |
| Push on a flinch | none | — | D-20261005-58 | none (no reaction, no push) |
| Push during paired sequences | none | — | D-20261005-60 | none |

### 3.11 What stays as today

No decision touches these, so they keep working as they do today. Each one can change only through a later decision.
- **Unblockable attacks** hit through guard at full damage, and guard stays up (`DefenseResolver.cpp:337-341`).
- **Block-interruptible attacks** recoil when blocked (`DefenseResolver.cpp:366-374`).
- **A block uses one of the swing's hit slots**, and the swing cannot touch that defender again.
- **Guard facing while guard is held** keeps today's threat selection and automatic turn. Guard aim is "direction, with
  magnetism" [D-20261004-02], whose design is queued and not part of this spec.
- **Lands-first ties** fall back to today's lower tie-breaks (3.2, Parry-4).
- **Super armor, invulnerability, and i-frames on reaction entries** are unchanged **[reported]**.
- **Damage resistance** is unchanged, and so are the rules for paired partners.
- **Being Staggered again** replaces the remaining time (`HitReactionComponent.cpp:1439-1466`).
- **Hitstop, sound and effect** are configured per attack, now inside impact feedback (4.5).
- **The death outcome** (ragdoll or frozen pose), and the flag that makes a paired finisher's clip the death animation,
  are unchanged **[reported]**.
- **Input buffering** uses the last-input-wins queue.
- **Direction classification** uses the four-way classifier.
- **The path check before a paired finisher** is a 30 cm sweep. When it fails, the strike plays normally.
- **Targeting today.** No lock-on, strafe or target cycling exists in source (the 2026-10-07 log, E9). Today's soft aim
  picks the one best target in the character's facing direction, at 500 cm and 120°, and serves paired finishers only.
  Free mode (3.13) replaces it, and lock mode is new [D-20261007-12, D-20261007-13].

### 3.12 Tunables at a glance

| Tunable | Default | Home | Per character? | Decision |
|---|---|---|---|---|
| Guard arc | ±75° | combat settings; defense set override | per weapon | D-20261005-11, D-20261005-70 |
| Block turn rate (the turn is always the full angle) | 360°/s | combat settings; defense set override | per weapon | D-20261005-69 |
| Minimum parry window | 0.35 s | combat settings | no | D-20261005-09 |
| Parry window scale | 1.0 | reaction profile, parry assists | yes | D-20261005-09, D-20261005-78 |
| Parry lockout | 0.25 s, lenient; to be felt first, and it may go to zero | combat settings | no | D-20261005-32, D-20261007-18 |
| Attacker slowdown | ≥ 50% speed, ≤ 0.3 s | combat settings | no | D-20261005-33 |
| Fastest natural turn | 720°/s | combat settings; defense set override | per weapon | D-20261005-75, D-20261005-68 |
| Other parry assists (input buffer, contact grace, slow motion on window, telegraph, always perfect parry) | 0, 0, off, on, off | reaction profile | yes | D-20261004-07, D-20261005-78 |
| Staggered time | 1.5 s (one default) | combat settings → attack → slot | per attack | D-20261005-08, D-20261005-31 |
| Follow-up strike multiplier | ×1.5 of the resolved attack's damage | combat settings (proposed) | no | D-20261005-39, D-20261005-82 |
| Counter damage | 50 (default); each defense-set entry sets its own; a paired clip may override | combat settings → defense-set entry → paired clip | per entry | D-20261005-80, D-20261006-03 |
| Hit-stun dodge escape | opens at max(0.12 s, half the stun) | rule | no | D-20261005-87 |
| The player's Staggered time | 0.6 s, always the player's own | the player's reaction profile | yes | D-20261005-88, D-20261006-06 |
| Staggered dodge escape (player) | opens at max(0.12 s, half the player's Staggered time) | rule | no | D-20261005-88 |
| Guard-break threshold and window | 4 points within 3 s | reaction profile | yes | D-20261005-15, D-20261005-26 |
| Guard breaker | charged heavy and directional light: yes; others: no | combat settings → attack → slot | per attack | D-20261005-29 |
| Pressure | light 1, heavy 2, charged 3 | combat settings → set → attack → slot | per attack | D-20261005-17 |
| Base armor | 0 | reaction profile | yes | D-20261005-17 |
| Hit-stun | 0.25 / 0.45 / 0.6 s | combat settings → set → attack → slot | per attack | D-20261005-34 |
| Hit-stun floor | 0.12 s | combat settings | no | D-20261005-34 |
| Aim cone | 60° either side of the stick direction, or of the facing when no direction is held | combat settings | no | D-20261005-27, D-20261007-12 |
| Finisher range | 350 cm | combat settings | no | D-20261005-28 |
| Lock range, and "near" for the automatic re-pick | Q20 | combat settings (proposed) | D-20261007-13 |
| Flick threshold (lock switch) | no decided value; tuned on captures (proposed) | combat settings (proposed) | D-20261007-13 |
| Can finish the player | off | reaction profile | yes | D-20261005-20 |
| Guard-break counting toggle | base on; player off | reaction profile | yes | D-20261005-46, D-20261005-56 |
| Most attackers at once | 2 | combat settings | no | D-20261005-35 |
| Per-enemy attack cooldown | Q6 | combat settings | no | D-20261005-35 |
| Knockback (hit push) | Light 25 cm / 0.2 s, Heavy 20 cm / 0.25 s (code seeds) | combat settings → set → attack → slot | per attack | D-20261004-38 |
| Block shove | Light 10 cm / 0.2 s, Heavy 8 cm / 0.25 s (proposed) | combat settings → set → attack → slot | per attack | D-20261005-59 |

### 3.13 Targeting and camera

**Decisions:**
- D-20261007-12: free-mode aim: one rule for every attack press, the aim cone around the stick direction, or the
  character's facing when no direction is held; Staggered or eligible enemies win ties, nearest otherwise; the camera
  never decides;
- D-20261007-13: lock mode: a lock press picks the best target by the same cone and scores; while locked every press
  aims at the locked target, the player strafes facing it, and a flick of the look stick (or a scroll) switches to the
  nearest enemy in that direction; the lock drops on the target's death (the next is picked automatically when one is
  near) or on a second press;
- D-20261007-14: this design lives here, beside the rules that use it;
- D-20261005-27: the cone's 60° half-angle; its camera reference is replaced by D-20261007-12;
- D-20261005-28 and D-20261005-85: 350 cm for a paired finisher and for a follow-up strike;
- D-20261004-35: aim decides;
- D-20261004-26: the pillars: a free camera, with an optional lock.

The owner's description (the 2026-10-07 log, E9): "the camera will use input direction to let you turn and attack in
the direction you are pressing but then clicking lockon locks to the best target and enters strafe mode letting you
scroll through targets by direction and location".

**Two modes.** Free mode is the default. Lock mode starts with a lock press and ends as below. Neither a lock-on nor a
target cycle exists today, and today's soft aim (the facing, 500 cm, 120°) is what free mode replaces (3.11).

**Free mode** [D-20261007-12]. One rule for every attack press: ordinary attacks, follow-up strikes and paired
finishers alike.

| # | Rule |
|---|---|
| Free-1 | The cone's centre is the stick direction when a direction is held, else the character's facing. The stick direction is the world direction the stick points to in the frame movement uses; that is a reading (Appendix B). |
| Free-2 | Candidates are hostile, alive, not dying and not in a paired animation, within 60° either side of the centre [D-20261005-27]. |
| Free-3 | Staggered or eligible (lethal) enemies win ties; otherwise the nearest wins. Among several Staggered or eligible enemies the nearest wins; that is a reading (Appendix B). |
| Free-4 | A paired finisher needs its target within 350 cm, and a follow-up strike reaches the same 350 cm [D-20261005-28, D-20261005-85]. An ordinary attack has no range here beyond today's attack warp (3.0, "aimed at"); that is a reading (Appendix B). |
| Free-5 | The camera never decides. It follows the character and the look stick and plays no part in picking the target. |

**Lock mode** [D-20261007-13].

| # | Rule |
|---|---|
| Lock-1 | A lock press picks the best target by the free-mode cone and its scores (Free-1 to Free-3), at that moment. With no candidate nothing happens, and the mode does not change (a reading, Appendix B). |
| Lock-2 | While locked, every attack press aims at the locked target; the cone is not consulted. |
| Lock-3 | The player strafes, facing the locked target. |
| Lock-4 | A flick of the look stick, or a scroll, switches the lock to the nearest enemy in that direction. |
| Lock-5 | The lock drops when the target dies. If another enemy is near, it is picked automatically; what "near" means is Q20. |
| Lock-6 | A second lock press drops the lock. |

Nothing here designs the camera's framing while locked, the strafe speed or the lock indicator; they belong to the camera
and movement designs the pillars queued, with guard aim (3.11), and come later. Guard facing keeps today's threat
selection (3.11); the lock does not change what guard faces (a reading, Appendix B).

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Aim cone (half-angle) | 60° | combat settings | D-20261005-27, D-20261007-12 | soft aim from facing: 120° rejection, scored on 45° |
| Cone centre | the stick direction, else the facing | rule, no tunable | D-20261007-12 | the facing |
| Tie rule | Staggered or eligible first, then nearest | rule, no tunable | D-20261007-12 | scored on angle |
| Finisher and follow-up range | 350 cm | combat settings | D-20261005-28, D-20261005-85 | 500 cm, `SoftAimRange` |
| Lock range (how far a lock press looks) | Q20 | combat settings (proposed) | D-20261007-13 | none |
| "Near", for the automatic re-pick on death | Q20 | combat settings (proposed) | D-20261007-13 | none |
| Flick threshold (look-stick deflection that switches) | no decided value; tuned on captures | combat settings (proposed) | D-20261007-13 | none |

**Changes from today:**
- The cone's centre is the stick direction, else the facing. The earlier draft of this spec keyed it to where the
  camera looked [D-20261005-27, replaced by D-20261007-12].
- One rule for every press; today's soft aim serves paired finishers only.
- Lock mode is new: today there is no lock-on, strafe or target cycling.

---

## 4. Data model

### 4.1 One place per question

The asset layout is the owner's [D-20261005-48, D-20261005-57]: the **weapon data** references a **combo set** (attacks
only), a **defense set** (block and parry responses) and a **paired set** (every two-character sequence). The per-character
**reaction profile** keeps what belongs to the body. The old defense configuration is retired. A character with no weapon
data uses the combat settings' default weapon data, so its sets apply with no special case [D-20261005-49].

```
UCombatSettings ── DefaultWeaponData ──┐
                                        ▼
BaseCombatCharacter ── WeaponData ─▶ UWeaponData ──▶ UComboSet    (slots, links, openers, set-level defaults,
                    │                            │                 finisher references ─────────────┐)
                    │                            ├──▶ UDefenseSet  (block reactions, guard enter/exit,  │
                    │                            │                 counter entries: deflect, fallback   │
                    │                            │                 strike, damage, counter ref ─┐       │)
                    │                            └──▶ UPairedSet   (counters by direction and swing ◀──┘       │
                    │                                              each can kill, kill sequence optional;       │
                    │                                              finishers by direction ◀───────────────────┘)
                    └── ReactionProfile ──▶ UReactionProfile (hit reactions, Staggered reaction, recoil, armor,
                                                              guard break, marker, parry assists)
```

| Question | Where the answer lives | Decision |
|---|---|---|
| Which kit does this character fight with? | Its weapon data, or the combat settings' default weapon data | D-20261005-48, D-20261005-49 |
| What follows what, from which input? | The combo set: slots, links, openers, entry rules | D-20261005-01, D-20261005-03, D-20261005-40 |
| What does this move do: montage, timing, damage, pressure, flags, impact feedback? | The attack (`UAttackData`) | D-20261004-20, D-20261005-40 |
| How does this slot differ from its attack? | The slot's overrides | D-20261005-01 |
| How does this weapon block, deflect, and answer a parry? | The defense set: block reactions, guard enter and exit, the counter entries | D-20261005-48, D-20261005-45, D-20261005-55 |
| Which two-character counters and finishers does this weapon play? | The paired set, keyed by direction and role | D-20261005-57, D-20261005-47 |
| How does this character's body react, resist and break? | Its reaction profile | D-20261004-20, D-20261005-48 |
| What are the global rules and the per-type defaults? | The combat settings | D-20261003-08, D-20261004-20 |

**The rule for where a tunable lives** [D-20261005-68]: a tunable lives with the thing it describes.
- **The reaction profile covers reactions to hits only:** hit reactions, armor, guard-break counting, the Staggered
  reaction and recoil, hit-stun responses, and parry assists. It does not arbitrate movement or camera; movement and
  camera get their own profiles when they are needed.
- **Weapon behaviour lives on the weapon's sets:** how fast you turn to block, how wide the guard arc is, how fast the
  parry turn may go.
- **Global rules and defaults live on the combat settings.**

**Which side's asset is read.**
- The **defender's** defense set and paired set supply its block reactions, deflects, counters and the paired finisher
  of a lethal parry-counter, when its counter has one.
- The **attacker's** combo set and paired set supply the paired finisher when its follow-up strike or ordinary attack is
  lethal (3.8).
- The **victim's** reaction profile supplies its hit reactions, its Staggered reaction, its armor and its guard-break
  settings.
- The **attacking** character's reaction profile supplies the "can finish the player" marker.

### 4.2 Precedence

Every layered field resolves on its own, field by field. A layer that sets the field wins over the layers above it.
**The four layers are decided** [D-20261005-61].

1. **Combat settings type default**, per attack type. The tables have Light, Heavy, Charged and Special columns
   [D-20261004-20, D-20261004-38].
2. **Combo set type default**, the combo set's own per-type value [D-20261005-40]. It is the per-weapon layer
   [D-20261005-61], the one D-20261004-38 noted for later.
3. **Attack override**, on the attack asset [D-20261004-20].
4. **Slot override**, on the combo-set slot. **It wins** [D-20261005-01].

Notes:
- Each override is a toggle plus a value, the pattern `FKnockbackOverride` already uses (`CombatTypes.h:498-533`).
- **The Staggered time has one default**, not one per type [D-20261005-08]. Layer 1 holds that single value, and layers 3
  and 4 override it.
- **Heavy attacks carry values for both releases.** Where a type default distinguishes a charged release (pressure,
  hit-stun, guard breaker), a heavy attack's and a slot's override sets have one value for an uncharged release and one
  for a charged release. This is proposed (Q1 defines "charged").
- **Characters resolve the same way:** the base profile, then the character profile's overrides [D-20261004-20].
- **The editor always shows which layer an effective value came from** (5.3).

### 4.3 The weapon data

`UWeaponData` exists today (`WeaponData.h`). It keeps its mesh, sockets, equip and holster montages, trace settings,
damage multiplier, hit sound and effect, and effects pool.

**Changes** [D-20261005-48, D-20261005-57]:

| Field | Today | After |
|---|---|---|
| `AttackConfiguration` (`WeaponData.h:185`) | Points at `UAttackConfiguration`, which holds only the two openers that are read | Replaced by **`ComboSet`**, a reference to a `UComboSet` |
| — | — | **`DefenseSet`**, a reference to a `UDefenseSet` |
| — | — | **`PairedSet`**, a reference to a `UPairedSet` |

**Resolution** [D-20261005-49]. Today the weapon component uses its own `WeaponData`, else the combat settings'
`DefaultWeaponData` (`WeaponComponent.cpp:87-97`). That order stays, and all three sets come with whichever weapon data
wins. There is no separate fallback for any one set.

**Validation.** A weapon data missing any of the three sets is an error, and so is a defense set or combo set whose
references point into a paired set other than the weapon's.

### 4.4 The combo set

The combo set holds attacks only [D-20261005-48]: "the asset holding slots, links and entry rules for one moveset, plus
set-level type defaults" [D-20261005-40].

**Proposed class:** `UComboSet`. **Proposed asset name:** `DA_ComboSet_<Moveset>`, for example `DA_ComboSet_Katana`.
It is named for its purpose, following the project's naming rule [D-20261004-17].

| Field | Holds | Decision |
|---|---|---|
| Slots | An array of slots (below). | D-20261005-01 |
| Openers | The slot each input starts from when no combo is running: a Light opener and a Heavy opener (today `DefaultLightAttack` and `DefaultHeavyAttack`). | glossary "entry rules" [D-20261005-40] |
| Entry rules | Further openers under conditions (running, airborne), added when those moves are designed. None ships now (7.2). | D-20261005-40 |
| Set-level type defaults | Per-type values for the layered fields (4.2). | D-20261005-40 |
| Finishers | One reference per attacker direction into the paired set's finishers: the finisher pool that a lethal follow-up strike or lethal ordinary attack plays (3.8). The per-direction shape is proposed; D-20261005-57 says only that the combo set references finishers from the paired set. | D-20261005-57, D-20261005-47 |

**Slot** (proposed struct `FComboSlot`):

| Field | Holds |
|---|---|
| Id | A stable, hidden identifier. Links and the editor use it, so renaming or reordering never breaks a link. |
| Attack | A reference to an attack asset [D-20261005-01]. |
| Lane | Light, Heavy or Directional, for the grid [D-20261005-02]. |
| Links | One per input: Light, Heavy, and hold-and-release Light toward Forward, Back, Left or Right. Holding Heavy is the charge, so it has no directional links. |
| Overrides | Toggles and values for the slot-overridable fields (4.5) [D-20261005-01]. |
| AI selection | How an enemy AI weighs this slot: a selection weight (default 1.0) and a minimum and maximum range (today's `FEnemyAttackConfig` fields, `EnemyAITypes.h:113-125`). Enemies choose among their combo set's slots by these; the separate AI attack lists are retired [D-20261005-63]. |

**Link** (proposed struct `FComboLink`) has one of three kinds:
- **To a slot:** the combo continues into that slot.
- **Back to opener:** the combo starts over at the opener for the link's own input. It is shown with a loop icon. It is
  the only allowed kind of cycle [D-20261005-03, D-20261005-40].
- **None:** the slot has no follow-up for that input. What a press then does is Q9.

**Validation.** The set implements `IsDataValid`, and every result also shows in place as a badge (5.5).

| Severity | Rule |
|---|---|
| Error | A cycle that does not pass through a back-to-opener link [D-20261005-03]. |
| Error | A link to a missing slot; a slot with no attack; a slot whose attack's montage section does not exist; two openers for one input. |
| Warning | A slot no link reaches and that is not an opener (unreachable). `LightAttack_11` is one today. |
| Warning | A direction with no finisher reference [D-20261005-47]. |
| Warning | A combo set whose slots all have an AI weight of 0, so an enemy using it never attacks. |
| Warning | An attack with no authored parry window; it gets the 0.35 s minimum at runtime [D-20261005-74]. |
| Warning | An unparryable attack that has an authored parry window. |
| Warning | An attack flagged both unblockable and guard breaker: at contact unblockable wins and the guard does not break [D-20261007-16]. |
| Warning | A cancel notify on an attack montage: attacks cancel at the phase transition, and the notify is ignored (4.13) [D-20261006-02]. |
| Info | An override equal to the value it inherits. |
| Not flagged | A loop through a back-to-opener link. It is shown, not flagged [D-20261005-03]. |

**Edit-through** [D-20261005-01]:
- The slot card shows the referenced attack's own fields, and editing them edits the attack asset.
- Before the first edit, the card shows "used by N slots in M sets", so a shared change is never a surprise.

**Make unique** [D-20261005-01]:
- It copies the attack asset and points the slot at the copy.
- It is needed whenever a slot must differ in something a slot cannot override, above all montage-notify data: the parry
  window, the phases, the hold start and the armor windows. That data cannot vary per slot, because sections are shared
  **[reported]**.

### 4.5 The attack

The attack is "the move itself: montage, timing, damage, pressure, flags, impact feedback" [D-20261005-40].

**It keeps:**
- `AttackType`;
- the montage and section fields;
- `BaseDamage`;
- `MaxHitCount`;
- the editor timing-generation fields (with the action that reads timing from the montage, merged in #139
  [D-20261004-33]);
- the hold and charge fields;
- `WarpConfig`;
- the defense profile: height, lane, contact socket and bone, and the swing;
- the attack tags `Attack.Property.Unblockable` and `Attack.Defense.BlockInterruptible`;
- the combo timing fields `ComboInputWindow` and the blend times.

**The swing** [D-20261005-84]. A counter is chosen by the parried attack's swing: slash left, slash right, overhead or
thrust [D-20261005-45]. The attack carries a **`Swing`** field with those four values. An editor action,
**"Auto-detect swing"**, computes it from the weapon trace path (the `WeaponStart`/`WeaponEnd` socket path over the
attack's Active phase, in the attacker's local frame: a mostly lateral path is a slash, signed by its direction; a mostly
downward path is an overhead; a mostly forward path is a thrust) and writes the field once, like the timing action
merged in #139; the designer may override the value by hand. The classification thresholds are proposed and tuned on
the shipped attacks. Today's `SwingShape` and `NominalLane` (`AttackData.h:17-34`) stay: the lane is the optional
refinement [D-20261005-45], and the swing shape is kept for height and the editor's description of the move.

**The impact block.** One block per attack holds every layered field, each a toggle plus a value
[D-20261003-08, confirmed by D-20261004-20]:

| Field | Default comes from | Can a slot override it? | Decision |
|---|---|---|---|
| Damage | `BaseDamage` (not layered by type) | yes | D-20261005-01: the option the owner picked describes slot overrides as "knockback, damage, feedback" (session transcript, answered 02:54 EDT) |
| Pressure (uncharged; charged for heavies) | type defaults | yes | D-20261005-17 |
| Hit-stun (uncharged; charged for heavies) | type defaults. It replaces `HitStunDuration`. | yes | D-20261005-34, D-20261004-41 |
| Staggered time | the one default | yes | D-20261005-08 |
| Guard breaker (uncharged; charged for heavies) | type defaults | yes | D-20261005-29 |
| Knockback | type defaults (`FKnockbackOverride`, which exists) | yes | D-20261004-38 |
| Block shove | type defaults (proposed overrides; 3.10) | yes | D-20261005-59 |
| Impact feedback per outcome (hit, block, parry): sound, effect, hitstop, camera shake | type defaults | yes | D-20261004-20, D-20261004-06 |
| Unparryable | off | yes | D-20261005-07, D-20261005-40 |

"Unparryable" is a plain on/off flag. The project's semantics rule gives booleans "direct authored gates", and the
glossary calls it "an attack flagged". An attack flagged both unblockable and guard breaker saves, with a warning (4.4):
at contact unblockable wins [D-20261007-16].

**Deprecated, then removed after migration (7.2):**
- **The combo links** `NextComboAttack`, `HeavyComboAttack`, `DirectionalFollowUps` and `HeavyDirectionalFollowUps`
  (`AttackData.h:157-170`). They move to the combo set. `HeavyDirectionalFollowUps` is not read at runtime today; only
  validation and one check read it (`CombatComponent.cpp:3642-3643`).
- **`CounterData` and `FinisherData`** (`AttackData.h:322-331`). They move to the defense set's counter pool and
  finisher pool [D-20261005-48].
- **Fields nothing reads:** `CounterDamageMultiplier` (`:149-150`), `bCanTriggerFinisher` (`:353-354`),
  `bHasCounterVariant` (`:360-361`), and the two deprecated meter-damage fields (`:145-146`, `:219-221`, named in 7.7).
- **The opt-in `Attack.Defense.Parryable` tag**, replaced by the unparryable flag [D-20261005-07].
- **`HitStunDuration`**, which becomes the hit-stun override.
- **The separate feedback fields** `ImpactAudioConfig`, `ImpactVFXConfig`, `HitstopConfig` and the defense profile's
  blocked-impact overrides. They fold into the impact feedback per outcome.
- **The `Attack.Capability.Terminal` tag**, replaced by links of kind "None". This is proposed, and part of Q10.

### 4.6 The defense set

The defense set is the weapon's "block and parry responses: deflect, counter, lethal paired variant, fallback strike,
keyed by direction and swing" [D-20261005-48]. Since D-20261005-57 the two-character counters themselves live in the
paired set; the defense set keeps the deflect and the fallback strike and references the counter. Since D-20261006-03
each entry also holds the counter's damage, and since D-20261007-07 the referenced counter can kill on its own, with an
authored kill sequence optional.

**Proposed class:** `UDefenseSet`. **Proposed asset name:** `DA_DefenseSet_<Weapon>`, for example `DA_DefenseSet_Katana`.

| Field | Holds | Decision |
|---|---|---|
| Defense tunables | Overrides of the combat-settings defaults for this weapon: the guard arc [D-20261005-70], the block turn rate [D-20261005-69] and the fastest natural parry turn [D-20261005-75]. Each is a toggle plus a value. |
| Block reactions | One block reaction per attacker direction (front, left, right, behind). Only the front entry is used while the guard arc is ±75°; the others wait for full-circle blocks, which then need content plus one setting. | D-20261005-48, D-20261004-23 |
| Guard stance | The guard enter and exit clips, wired at last. | D-20260929-07, D-20261005-55 |
| Counter pool | Counter entries (below), keyed by attacker direction and swing. | D-20261005-24, D-20261005-45, D-20261005-48 |
| Turn-deflects | The 90° left, 90° right and 180° behind turn-deflects that flow into front entries. | D-20261004-30 |

**Counter entry** (proposed struct `FCounterEntry`):

| Field | Holds | Decision |
|---|---|---|
| Direction | Front, left, right or behind. | D-20261004-30, D-20261005-45 |
| Swing | Slash left, slash right, overhead or thrust. | D-20261005-45 |
| Lane | Optional: left, centre or right. An entry that names the lane is preferred when it matches (3.3). | D-20261005-45 |
| Deflect | The defender's deflect that meets the blade. An off-front entry may instead name its turn-deflect and a front entry. | D-20261005-38, D-20261004-30 |
| Counter | A reference to a counter in the weapon's paired set (4.7), or none, in which case this entry's fallback strike plays. The referenced counter shares this entry's deflect, can kill at its own contact, and may carry an authored kill sequence. | D-20261005-48, D-20261005-57, D-20261007-07 |
| Damage | This entry's counter damage: a toggle plus a value over the combat settings' default of 50. The referenced clip's own override beats it; the fallback strike always deals this value (3.3). | D-20261006-03, D-20261005-80 |
| Fallback strike | This entry's single-character strike, played when no paired counter fits. The glossary keys it by direction and swing like the rest of the entry. It carries its own cancel notify (3.0). | D-20261005-23, D-20261005-48, D-20261006-02 |

**Validation** (`IsDataValid`, with badges in the defense set editor, 5.9):

| Severity | Rule |
|---|---|
| Warning | A direction × swing cell with no entry, with the fallback that will play for it [D-20261005-30]. |
| Warning | An entry with no counter reference, so only its fallback strike can play. |
| Warning | A deflect, turn-deflect, fallback strike or block reaction with no cancel notify; it is read as ending at montage end [D-20261006-02]. |
| Error | An entry with no fallback strike; an empty counter pool; no front block reaction; no guard enter or exit clip. |
| Error | A counter reference into a paired set other than the weapon's, or to a counter whose direction differs from the entry's. |

### 4.7 The paired set

The paired set "holds every two-character sequence keyed by direction and role: counters (lethal and non-lethal
variants) and finishers. The defense set references counters from it; the combo set references finishers from it"
[D-20261005-57]. The owner's reason: finishers are offensive, so they do not belong with the weapon's defenses. Since
D-20261007-07 a counter is one sequence that can kill at its own contact; an authored kill sequence is optional.

**Proposed class:** `UPairedSet`. **Proposed asset name:** `DA_PairedSet_<Weapon>`, for example `DA_PairedSet_Katana`.

| Field | Holds | Decision |
|---|---|---|
| Counters | Counter sequences (below), keyed by attacker direction, with the parried swing as a tag so that a defense-set entry can pick the one that fits. | D-20261005-57, D-20261005-45 |
| Finishers | One paired finisher per attacker direction (front, left, right, behind). | D-20261005-57, D-20261005-47 |

**Counter sequence** (proposed struct `FPairedCounter`):

| Field | Holds | Decision |
|---|---|---|
| Direction | Front, left, right or behind. | D-20261005-57 |
| Swing tag | Optional: the swing this counter was authored against, for the defense set editor's picker. | D-20261005-45 |
| Counter | A paired-content reference (4.12): deflect plus counter. Its damage lands at the counter's contact and can kill; when it does not, the enemy is Staggered from its own cancel notify. | D-20261005-24, D-20261005-57, D-20261007-07 |
| Kill sequence (optional) | A paired-content reference (4.12): the same deflect flowing into an authored kill, played instead of the counter when the press is lethal. Not required. | D-20261005-38, D-20261005-57, D-20261007-07 |
| Damage override (optional) | A toggle plus a value that replaces the defense-set entry's damage for this clip (3.3). | D-20261006-03 |

**Finisher** (proposed struct `FPairedFinisher`): a direction and a paired-content reference (4.12).

**Validation** (`IsDataValid`, with badges in the paired set editor, 5.9):

| Severity | Rule |
|---|---|
| Warning | A direction with no counter. |
| Warning | A direction with no finisher [D-20261005-47]. |
| Warning | A counter or finisher that no defense set or combo set references. |
| Warning | A counter half, kill-sequence half or finisher half with no cancel notify; it is read as ending at montage end [D-20261006-02]. |
| Info | A counter with no kill sequence: a lethal press kills at the counter's own contact [D-20261007-07]. |
| Error | A counter, kill sequence or finisher whose two roles share a montage, or whose "can it start here?" check (4.12) can never pass, for example missing roles. |
| Error | A finisher reference whose content is a counter, or a counter reference whose content is a kill (4.12). |

### 4.8 The reaction profile

**Proposed class:** `UReactionProfile`. **Proposed assets:** `DA_ReactionProfile_Base`, and one `DA_ReactionProfile_<Character>`
per character [D-20261004-20].

**Inheritance.** A profile names a parent. Each section has override toggles, and anything unset comes from the parent.
Parents may be nested, but never in a cycle. This is proposed.

**It holds** a character's hit reactions, armor base, guard-break settings, the "can finish the player" marker and parry
assists [D-20261005-48, D-20261005-40 as amended]. It covers reactions to hits only, never movement or camera
[D-20261005-68]. Block and parry responses are the weapon's (4.6, 4.7), and there is no "can be finished" marker and no
body type [D-20261005-47].

| Section | Contents | Decision |
|---|---|---|
| Hit reactions | Directional light and heavy reactions (front, back, left, right), each an array of variants. Death reactions, also an array. The additive flinch for each direction. I-frames on each entry, as today. **No stun field.** Each reaction clip carries a cancel notify (3.0); one without it ends at montage end, with a warning (4.13). | D-20261002-07, D-20261004-41, D-20261004-42, D-20261006-02 |
| Body-side defense reactions | The Staggered reaction, by direction: what this character does when its attack is parried or its guard breaks, also played with the fallback strike. Recoil, when its attack is blocked by a block-interruptible defender. | D-20261005-23, D-20261005-55 |
| Armor | Base armor, default 0. | D-20261005-17, D-20261005-44 |
| Guard break | Counting toggle: on in the base profile, off for the player. Threshold N, default 4. Window W, default 3 s. | D-20261005-15, D-20261005-26, D-20261005-46, D-20261005-56 |
| Staggered | This character's own Staggered time, a toggle plus a value. Unset in the base profile, so enemies take the causing attack's time; set to 0.6 s on the player's profile. When set, it always applies: an attack's or a slot's override never changes it. | D-20261005-88, D-20261006-06 |
| Marker | Can finish the player, default off. | D-20261005-20, D-20261005-40 |
| Parry assists | Window scale 1.0, input buffer 0, contact grace 0, slow motion on window off, telegraph on, always perfect parry off. | D-20261004-07, D-20261005-09, D-20261005-78 |
| Other per-character hit values | `KnockbackScale` and damage resistance: how this body takes a hit. They move here; proposed. The guard-facing turn and the block turn do **not** live here [D-20261005-68]. | D-20260930-01 |

**Where it is referenced.**
- The combat settings' `HitReactionSettings` and `DefenseConfiguration` slots (`CombatSettings.h:82, 86`) become one
  reaction-profile slot.
- The component's per-instance override stays.
- **A missing profile falls back to the base profile and logs once**, naming the owner and what is lost. This carries over
  the approved, never-started fix for an empty defense slot [D-20261002-10].

### 4.9 The combat settings

**Global rules** [D-20261003-08, D-20261004-20]:

| Rule | Value | Decision |
|---|---|---|
| Guard arc | ±75°, overridable on a defense set | D-20261005-11, D-20261005-70 |
| Block turn rate | 360°/s, overridable on a defense set | D-20261005-69 |
| Minimum parry window | 0.35 s before contact | D-20261005-09 |
| Parry lockout | 0.25 s, lenient: a press during it is honoured if the window is still open when it ends; it may go to zero | D-20261005-32, D-20261007-18 |
| Attacker slowdown | no lower than 50% speed, at most 0.3 s | D-20261005-33 |
| Staggered time default | 1.5 s | D-20261005-08 |
| Hit-stun floor | 0.12 s | D-20261005-34 |
| Follow-up strike multiplier | ×1.5 of the resolved attack's damage | D-20261005-39, D-20261005-82 |
| Counter damage default | 50, under each defense-set entry's value and any clip override | D-20261005-80, D-20261006-03 |
| Aim cone | 60° either side of the stick direction, or of the facing when no direction is held | D-20261005-27, D-20261007-12 |
| Lock range, and "near" for the automatic re-pick | Q20 | D-20261007-13 |
| Flick threshold | no decided value; tuned on captures (proposed) | D-20261007-13 |
| Finisher range | 350 cm | D-20261005-28 |
| Most enemies attacking at once | 2 | D-20261005-35 |
| Per-enemy attack cooldown | Q6 | D-20261005-35 |
| Default weapon data | the kit used by a character with none (`CombatSettings.h:78`) | D-20261005-49 |
| Guard-facing threat selection | today's values, unchanged (3.11) | — |
| Fastest natural turn (default) | 720°/s, overridable on a defense set | D-20261005-75, D-20261005-68 |
| Attacker slowdown ease shape | tuned on captures (proposed) | D-20261005-76 |
| Guard-facing turn rate and budget | today's values, unchanged (3.11); overridable on a defense set, since they are weapon behaviour (proposed under D-20261005-68) | — |

**Per-type defaults:**

| Field | Light | Heavy | Charged | Special | Decision |
|---|---|---|---|---|---|
| Pressure | 1 | 2 | 3 | Q2 | D-20261005-17 |
| Hit-stun | 0.25 s | 0.45 s | 0.6 s | Q2 | D-20261005-34 |
| Guard breaker | no (yes for directional lights) | no | yes | Q2 | D-20261005-29 |
| Knockback | 25 cm / 0.2 s (code seed) | 20 cm / 0.25 s (code seed) | the Heavy value, scaled by charge, as today | as today | D-20261004-38 |
| Block shove | 10 cm / 0.2 s (proposed) | 8 cm / 0.25 s (proposed) | the Heavy value, scaled by charge (proposed) | Q2 | D-20261005-59 |
| Impact feedback (hit, block, parry) | seeded from today's values; the parry values come from a rendered sweep [D-20261005-78] | same | same | same | D-20261004-20, D-20261004-06 |

The block and parry defaults come from today's defense configuration: the sound `Impact_Sword_Parry_01_Cue` and the effect
`NS_GFXI_Yellow_Parry`, the same for both **[reported]**. They seed the block defaults. The parry's distinct feedback is
chosen from a rendered sweep before the parry stage ships [D-20261005-78].

### 4.10 Retiring the defense configuration

`UDefenseConfiguration` is removed [D-20261003-08, confirmed by D-20261004-20 and D-20261005-48]. Its contents split
three ways:
- **The weapon's responses** go to the defense set (the block reactions, the parry deflects, the guard enter and exit
  clips [D-20261005-55]) and the paired set (the two-character counters and finishers [D-20261005-57]).
- **The body's reactions** go to the reaction profile: the Staggered reaction and recoil [D-20261005-55].
- **The defense tunables** go to the combat settings as defaults, overridable on the defense set: the guard arc, the
  block turn rate, and the guard-facing turn rate and budget [D-20261005-68, D-20261005-69, D-20261005-70].
- **Global rules** go to the combat settings: threat selection and the slow-motion watchdog. The timers of today's
  post-parry stages are removed. The default sound and effect become impact-feedback type defaults.

The field-by-field map is in 7.4, where today's names can be given.

### 4.11 The resolved attack spec

**Today.** The hit record, `FHitReactionInfo`, carries only a pointer to the attack data (`CombatTypes.h:794, 813`), and
about 215 lines in 10 files read the attack through it **[reported]**. A slot override cannot be seen at hit time.

**Proposed: `FResolvedAttackSpec`.**
- **When it is built.** Once, when an attack starts, from the four layers (4.2).
- **Immutability.** It is a snapshot and never changes. It travels with every hit, every block contact and every parry
  query of that attack.
- **Identity:** the attack, the combo set, the slot id, and the attack's instance and generation.
- **Resolved values:**
  - damage (base × weapon multiplier);
  - pressure;
  - hit-stun;
  - Staggered time;
  - guard breaker;
  - knockback and the block shove;
  - impact feedback per outcome;
  - the unparryable, unblockable and block-interruptible flags;
  - swing, lane and height.
- **Charge:** whether the release was charged (Q1), latched at release. Today a latched charge field exists on the hit
  record but nothing writes it (`CombatTypes.h:867-869`).
- **Before a press executes.** The lethal checks of 3.3 and 3.8 resolve the spec of the strike the press would start.

**Why a snapshot:**
- an edit or an override that changes mid-attack never changes a hit in flight;
- a test can build a spec without any assets.

### 4.12 The paired-content interface

This follows the plugin review's "narrow seam" idea (proposed).

**Purpose.** The paired set's counters and finishers refer to two-character content through one narrow interface. Today's
`UPairedAnimationData` and a later Duet asset can then both sit behind it, and the choice of Duet stays open.

**Proposed shape:** an interface, `ICombatPairedContent`, that answers:
- its roles: who starts it and who receives it, with their montages and sections;
- its warp targets and warp limits;
- its sync moments: the deflect and the damage, and each role's cancel notify (3.0) [D-20261006-02];
- its direction and swing tags;
- whether it is a kill (a paired finisher, or a counter's authored kill sequence) or a counter [D-20261007-07];
- a quiet "can it start here?" check that returns refusal reasons. The editor and the "why didn't it play" debug readout
  use those reasons.

`UPairedAnimationData` implements it first.

### 4.13 Notifies on montages

Several rules read data from the montage itself. They are listed here once, with their validation, so that the timing
strip (5.4) and `IsDataValid` agree.

| Notify | On | Carries | Read by | When missing |
|---|---|---|---|---|
| Phase transition (`AnimNotify_AttackPhaseTransition`) | attack montages | Windup, Active, Recovery | combo timing; the Active start as the contact fallback (3.2); the attack's cancel point [D-20261006-02] | today's rules |
| Parry window | attack montages | a lead time before the predicted arrival (R-20261005-01, proposed) | 3.2 | the 0.35 s minimum at runtime, with a warning [D-20261005-74] |
| Contact marker | attack montages | the authored contact frame (proposed, 3.2) | the predicted arrival | the Active start |
| Hold start (`AnimNotify_HoldWindowStart`) | attack montages | the moment the hold check runs | the hold | no hold |
| Armor window | character animations | an armor level (proposed notify state, 3.6) | 3.6 | base armor only |
| Cancel notify | every non-attack montage: deflects, turn-deflects, fallback strikes, block reactions, each half of a paired counter, kill sequence or finisher, hit and Staggered reactions | nothing; its position is the meaning (3.0) | 3.3, 3.4, 3.6, 3.7 | montage end, with a warning [D-20261006-02] |

**The cancel notify's shape** (proposed; D-20261006-02 fixes its meanings, not its class): one notify class with no
payload, placed once per montage. A second one on the same montage is an error. Attacks never carry it: their cancel
point is the phase transition from Active to Recovery; a cancel notify on an attack montage is a warning and is ignored
(4.4).

---

## 5. Editor UX

The owner chose a moveset grid with a read-only node map [D-20261005-02], and asked for an improvement on OnSight rather
than a copy of it (the 2026-10-04 log, E45). Everything in this section is proposed.

### 5.1 The moveset grid

- **Rows** are slots, grouped by lane: light, heavy, directional.
- **Columns** are inputs: Light, Heavy, and hold-and-release Light toward Forward, Back, Left and Right.
- **Each cell** is a chip for the link's target. It shows the slot name and the montage section. A back-to-opener link
  shows a loop icon, and an empty cell means no link.
- **Drag a row** to reorder it within its lane [D-20261005-02]. The lane's Light or Heavy links are rewired so that they
  follow the new order. The rewiring rule is proposed.
- **Drop a slot on a cell** to make a branch.
- **Right-click a cell** to set "back to opener" or to clear it.
- **"Append to lane"** creates the next slot, already linked.
- **"New attack from montage section"** creates an attack and its slot in one step.
- **Each drag is one undo step.** The selection is held by slot id, so undo and redo keep it.
- The weapon's defenses have their own editor (5.10).

### 5.2 The node map

- Read-only.
- Lanes are columns, and links are arrows labelled with their input.
- A back-to-opener link is a return arrow with the loop icon.
- Clicking a node selects its row in the grid.

### 5.3 Inline overrides

- **Every slot-overridable field** shows its effective value and where it came from, for example "from combat settings
  (Light): 25 cm", "from set", or "from attack".
- **Inherited values are greyed.**
- **Turning an override on** seeds it with the current value, and a reset arrow clears it.
- **The edit-through section** shows the attack's own fields, with "used by N slots in M sets" before the first edit.
- **A "make unique" button** sits next to it.
- **An "Auto-detect swing" button** on the attack card and on the attack asset's details panel, and the same action in
  the Content Browser on one or many attack assets, computes the `Swing` field from the weapon trace path and writes it
  once; the field stays editable by hand, and a value set by hand is marked so that re-running the action asks before
  overwriting it [D-20261005-84].

### 5.4 The timing strip

Each slot has a read-only strip, derived from its montage's notifies. This is the analysis that PR #139's timing action
uses [D-20261004-33]. It shows:
- the Windup, Active and Recovery phases;
- the authored parry window **and** the effective window after the minimum and the scale, with the extension hatched;
- the hold start;
- the armor windows;
- the contact moment;
- on a non-attack montage, the cancel notify (4.13).

A warning appears when the animation has changed under its notifies.

### 5.5 Validation badges

- Each badge sits on the row or cell it concerns. It states the problem and the fix, and it navigates there in one click.
- The rules are those of 4.3.
- Loops through a back-to-opener link are shown, not flagged.

### 5.6 Who uses this set

- A combo set, defense set or paired set lists the weapon data that reference it, and each weapon data lists the
  characters and AI that use it.
- Each attack lists the sets it appears in.
- A project-wide report lists attacks that no set references, and paired content that no defense set references.

### 5.7 Pickers

- Slots, montage sections, tags and enum values are chosen from pickers, never typed as names.
- A picker always shows the current value, even an invalid one.
- An empty list shows a hint.

### 5.8 Labels

- **Every field's tooltip** says what the field does in play, which side reads it (attacker or victim), its units, and
  how it combines with its defaults.
- **Fields not yet wired** are hidden under Advanced.
- **Terms come from the glossary** [D-20261005-40], and there are no internal planning labels [D-20261004-17].

### 5.9 The defense set and paired set editors

**The defense set editor:**
- **Block tab:** one row per direction (front, left, right, behind) with the block reaction; the guard enter and exit
  clips above them [D-20261005-55]; the rows outside the guard arc are greyed while the arc is ±75°.
- **Counter tab:** a grid with the four directions as rows and the four swings as columns [D-20261005-45]. Each cell shows
  its entry's deflect, its fallback strike and the paired-set counter it references, as three chips, with an empty chip
  for missing content, and the entry's counter damage with its source (the entry, or the settings default)
  [D-20261006-03]. The counter chip opens the paired set entry. A cell is coloured by the fallback that would play for
  it, so that a gap is seen before it is felt [D-20261005-30]. Optional lane refinements appear as sub-rows.
- **Pickers** for every clip and reference; the picker for a counter reference lists only the weapon's paired set, filtered
  to the cell's direction, with the swing tag shown.

**The paired set editor** [D-20261005-57]:
- **Counter tab:** one row per direction, each with its counters; every counter shows its counter clip and, when it has
  one, its authored kill sequence as two chips, with the swing tag and any damage override [D-20261007-07,
  D-20261006-03]. A direction with no counter is marked; a counter with no kill sequence is noted, not flagged, since it
  kills at its own contact.
- **Finisher tab:** one row per direction with its paired finisher; an empty row is a warning [D-20261005-47].
- **Coverage strip:** the four directions, coloured by whether a counter and a finisher exist, so missing directions are
  seen at a glance.
- **Used by:** each counter and finisher lists the defense-set entries and combo sets that reference it.
- The paired content's "can it start here?" refusal reasons are shown on hover (4.12).

### 5.10 What this improves on

- **Over OnSight's three flat card lists**, and ComboCreator's design (which has no UI yet): one grid plus a map,
  per-field overrides that name their source, coverage grids for counters and finishers, and the timing strip inside
  the set view.
  OnSight keeps its timing preview collapsed.
- **Over today's pain points [reported]:**
  - swapping light hits 3 and 4 means editing 3 assets;
  - changing the heavy branch partway through the light combo means editing 8 assets;
  - changing forward routing means editing 11 assets;
  - finding where an attack is used takes 4 hops.

  The editor stage measures the clicks each of these tasks takes, against today's editor.

---

## 6. Runtime flow

### 6.1 State

- the weapon data in use: the character's own, or the combat settings' default [D-20261005-49];
- the current combo set, defense set and paired set, from that weapon data [D-20261005-48, D-20261005-57];
- the current slot id;
- the combo state machine (`FAttackStateMachine`, `CombatTypes.h:1736`);
- the resolved attack spec of the running attack;
- the parry lockout timer, and the press remembered during it [D-20261007-18];
- the targeting mode, free or lock, and the locked target (3.13);
- the guard-break ledger;
- the Staggered and hit-stun timers.

### 6.2 From input to transition

One pure function serves both the game and the editor preview (proposed, from the plugin review):

```
SelectTransition(Set, CurrentSlot, Input, Context) -> { Chosen slot or none; Rejections: [ { link or slot id, reason } ] }
```

**Reasons** include:
- no link for this input;
- the target slot is missing;
- the slot has no attack;
- the montage section is missing.

Timing is never a rejection. Input is always captured, and the combo timing only decides when the chosen attack starts
(CLAUDE.md rule 2). A debug readout uses the reasons to answer "why didn't my combo fire".

**This replaces:**
- today's resolution: the directional follow-up, then the combo link, then the default, in `GetAttackForInput`
  (`CombatComponent.cpp:6526`) and `ResolveNextAttackContextual` (`MontageUtilityLibrary.cpp:1119`);
- today's cycle guard, which does nothing, because `VisitedAttacks.Empty()` runs on every resolution
  (`CombatComponent.cpp:693, 6615`).

### 6.3 Order of evaluation

**An attack press executes (Light or Heavy):**
1. The player is in hit-stun, or Staggered, or inside their own parry-counter before their cancel notify: the press is
   buffered, last input wins (3.7, 3.4, 3.3) [D-20261005-37, D-20261005-88, D-20261007-09].
2. Pick the target (3.13): in free mode by the aim cone around the stick direction, or the facing when no direction is
   held, Staggered or eligible enemies first and the nearest otherwise; in lock mode the locked target
   [D-20261007-12, D-20261007-13]. A paired finisher and a follow-up strike need 350 cm [D-20261005-28, D-20261005-85].
3. The target is Staggered: a follow-up strike. A light press uses the light opener from the initial press, and the hold
   does not open [D-20261007-17]. If it is lethal, a paired finisher (3.3).
4. Otherwise, the lethal check on the attack the press resolves to: a paired finisher (3.8) [D-20261005-42].
5. Otherwise, select the transition, start the attack, and build its resolved attack spec.

**A lock press** (3.13) [D-20261007-13]:
1. Not locked: pick the best target by the cone and its scores; lock on it, and strafe facing it.
2. Locked: drop the lock.

**A look-stick flick, or a scroll, while locked:** switch to the nearest enemy in that direction [D-20261007-13].

**The locked target dies:** drop the lock; pick the next automatically when one is near [D-20261007-13].

**A dodge press:**
1. The player is in hit-stun: the press waits for the escape to open, then ends the hit-stun and the committed dodge
   plays (3.7) [D-20261005-87].
2. The player is Staggered: the press waits for the escape to open, then ends the Staggered state, the enemy's opening
   with it, and the committed dodge plays (3.4) [D-20261005-88].
3. Both run: the escape needs both minimums to have passed [D-20261006-01].
4. Otherwise, the dodge (not designed in this spec).

**A Block press:**
1. The guard rules (3.1).
2. The parry rules (3.2), including the press remembered through a lockout [D-20261007-18].
3. The parry-counter (3.3): content, damage, outcome; then the turn, the parry and the counter; then each character's
   cancel notify.

**A weapon contact** (this order is proposed; today the resolver decides the block before anything else):
1. Block resolution (3.1).
2. Guard break (3.5).
3. Interruption (3.6).
4. Hit-stun (3.7).
5. Knockback (3.10).

### 6.4 Components that change

| Component | What changes |
|---|---|
| **Combat** (`UCombatComponent`) | The combo set and current slot; the selection function; the resolved spec at attack start; guard dropping when an attack starts, and Block buffered until Recovery; the parry lockout, with the press remembered through it [D-20261007-18]; hit-stun holding attacks and movement, with the dodge escape after its minimum [D-20261005-87]; the target of every attack press, from Targeting (3.13); presses buffered inside the parry-counter until the player's cancel notify [D-20261007-09]. |
| **Targeting** | Free mode: the aim cone around the stick direction, or the facing when no direction is held, Staggered or eligible enemies first and the nearest otherwise, for every attack press, with 350 cm for a paired finisher and a follow-up strike [D-20261007-12]; lock mode: the lock press, the locked target for every press, the strafe facing it, the flick (or scroll) switch, the drop on death with the automatic re-pick, and the second press [D-20261007-13]; today's soft aim retired. The code-driven turn for block and parry (the adaptive rate and its cap), through the alignment arbiter. Motion warping stays for attacks and paired content [D-20261004-21]. |
| **HitReaction** | Pressure against armor, armor windows and the flinch; hit-stun from the resolved spec, with its floor, holding the AI too; one Staggered state using the causing attack's time, and always the player's own time for the player [D-20261006-06], running beside hit-stun with both ending before the character is free [D-20261006-01]; the cancel notify on hit and Staggered reactions [D-20261006-02]; the guard-break count, built dormant [D-20261007-15]; knockback by outcome, with the block shove and no push on a flinch or in a paired sequence; the reaction profile in place of the hit-reaction settings; death reading the variants array. |
| **PairedAnimation** | The parry-counter: content, damage and outcome at the press; the turn, the deflect and the counter; damage at the counter's contact, never clamped; the kill at contact, or the authored kill sequence; each half's cancel notify, and the protection until it [D-20261006-03, D-20261006-05, D-20261007-07, D-20261007-08]; the defense set's counter entries, the paired set's counters and their fallback; the follow-up strike and the combo set's finisher references into the paired set; the paired-content interface. The post-parry timers of today go. |
| **Weapon** (`UWeaponComponent`, `UWeaponData`) | The weapon data references a combo set, a defense set and a paired set in place of the attack configuration; the default weapon data stays the fallback for a character without one [D-20261005-48, D-20261005-49, D-20261005-57]. |
| **Defense resolver** (`DefenseResolver.cpp`) | The guard arc at 75°; parry candidates (any direction, unparryable, effective window); lands first. The cone and reachability gates go. |
| **Enemy AI and attack tokens** (`EnemyCombatAIComponent`, `CombatTokenSubsystem`) | Attacks chosen among the weapon's combo-set slots by their AI weight and range, with `AvailableAttacks` and `FEnemyAttackConfig` retired [D-20261005-63]; 2 attackers and a per-enemy cooldown; others hold instead of aborting; hit-stun and Staggered holding attacks and movement; enemy paired finishers only for marked types. |
| **Cinematic effects** (`CombatEffectsWorldSubsystem`) | The attacker slowdown, as an actor time lease (no lower than 50%, at most 0.3 s), on the existing overlap-safe lease system. |
| **Animation notifies** | The cancel notify class and its validation (4.13) [D-20261006-02]; the parry window as a lead time (R-20261005-01, proposed). |
| **Editor module** | The combo set editor: the grid, the map, overrides, the timing strip and validation; the defense set and paired set editors; the migration operation (7.1). |

**Project instructions change too.** `CLAUDE.md`'s rule "Stateful Block input is immediate" no longer holds: a Block press
during your own attack is buffered [D-20261005-13]. `CLAUDE.md` and `AGENTS.md` also gain the glossary requirement
[D-20261005-40].

---

## 7. Migration

This section names today's code and assets, so the retired words appear here where they are part of a name.

### 7.0 How migration runs

- **Headless operations, never silent edits.** Asset changes go through the project's headless migration workflow:
  audit, then plan, then apply, with an explicit save step (`docs/guides/HEADLESS_ASSET_MIGRATIONS.md`). When the
  reusable editor operation workflow exists, they go through that instead [D-20260930-04].
- **Each stage migrates only what it needs**, behind a green baseline.
- **The owner's work in progress is never resaved or committed without the owner's go-ahead** (`AGENTS.md`).
- **Renames happen in the editor, with redirectors.** Tests, manifests and source move in the same change.

### 7.1 What ships today

| Kind | Assets |
|---|---|
| `UAttackData` | **29:** 11 light (`LightAttack_1`–`11`), 4 heavy (`HeavyAttack_1`–`4`), 5 directional (`DA_DirectionalAttack_F`, `_B`, `_L`, `_None`, `DirectionalAttack_R`), and 9 contact-matrix test attacks (`DA_GateB_*`). |
| `UAttackConfiguration` | **1:** `DA_Configuration_Katana`. Light is `LightAttack_1` and Heavy is `HeavyAttack_1`. `SprintAttack`, `JumpAttack` and `PlungingAttack` are never read (`AttackConfiguration.h:48-68`). |
| `UDefenseConfiguration` | **2:** `DA_DefenseConfiguration_GateA`, used by the player and every enemy, and `DA_DefenseConfiguration_GateB`, used by the test level. |
| `UHitReactionSettings` | `DA_HitReaction`. |
| `UCombatSettings` | `DA_CombatSettings_Default` and `DA_CombatSettings_DefenseMatrix`. |
| `UPairedAnimationData` | `DA_ParryBridge_GateA`, `DA_Counter_GateA`, `DA_Finisher_GateA`, `Finisher_A`, and `Counter_LightAttack_1`–`11` **[reported]**. |

### 7.2 Combo links into the combo set

**The shipped graph [reported]:**
- **Light:** L1 → … → L10 → L1, a loop.
- **Heavy:** H1 → H2 → H3 → H4 → H1, a loop.
- **Across:** a Heavy press from L3–L10 goes to H3, and back.
- **Directional:** the same map on all 11 lights, Forward, Back, Left and Right to `DA_DirectionalAttack_F`, `_B`, `_L`
  and `DirectionalAttack_R`. There is also a `None` key, to `DA_DirectionalAttack_None`, that nothing can reach.

**The migration:**
- reads every attack's links and reproduces them exactly, as `DA_ComboSet_Katana`'s slots and links;
- turns each edge that returns to a lane's opener into a back-to-opener link [D-20261005-03];
- reports any other cycle to the owner as an error, without guessing;
- takes the openers from `DA_Configuration_Katana`.

**Clean-ups, decided by the answer to Q10:**
- **`LightAttack_11`** has no incoming link. It is Unblockable and Terminal.
- **The `None` key.**
- **`SprintAttack`, `JumpAttack` and `PlungingAttack`**, which nothing reads.
- **The context-tag mismatch.**
  - The five directional attacks require `Context.DirectionalFollowUp`, and nothing grants it.
  - The hold-release path skips the gate (`CombatComponent.cpp:5165`) **[reported]**.
  - In the combo set, the hold link itself is the gate.
- **`Attack.Capability.Terminal`.** `HeavyAttack_4` is Terminal yet has follow-ups, and fails validation today (the
  2026-10-05 log, E1). The back-to-opener link replaces that intent.

**Order and dependencies:**
- **Timing first.** `HeavyAttack_1`–`4` keep default timing that does not match their animations. PR #139's action
  [D-20261004-33] runs first, so that the combo set validates.
- **Old fields.** After migration, the old link fields are read-only and ignored. They are removed once the combo set is
  the only path (stage 9.10).
- **The weapon's reference.** `UAttackConfiguration` is replaced by the combo set. The weapon data's slot
  (`WeaponData.h:185`) points at a combo set and a defense set [D-20261005-48]. The combat settings' own
  `AttackConfiguration` field (`CombatSettings.h:112`, kept only for serialization) is removed; the settings reach the
  sets through `DefaultWeaponData` (`CombatSettings.h:78`) [D-20261005-49].
- **Enemies.** Enemies choose among their combo set's slots by per-slot weight and range [D-20261005-63]. Today
  `BP_EnemyCharacter` lists `LightAttack_1`, and the four placed enemies override that list with one attack each
  (`LightAttack_4`, `_3`, `_1`, `_2`) **[reported]**. The migration gives enemies their own combo set, seeded from today's
  lists as weights; how the per-actor differences survive is Q15.

### 7.3 Attack fields

| Today | After |
|---|---|
| `HitStunDuration` | No shipped attack saves it **[reported]**, so there is nothing to move. It becomes the hit-stun override. |
| `Attack.Defense.Parryable` (only on `LightAttack_1`) | Removed: every attack is parryable unless flagged [D-20261005-07]. |
| `LightAttack_1`'s 0.10 s window | Unchanged in the asset. The 0.35 s minimum makes it effective. |
| Other attacks' parry windows | None exist **[reported]**; they get the 0.35 s minimum at runtime with an editor warning [D-20261005-74]. |
| `CounterData` | The paired set `DA_PairedSet_Katana` takes `DA_Counter_GateA`, the only counter the shipped flow reaches, as its front counter, and the defense set's front entries reference it, with its damage 25 kept as those entries' value [D-20261005-57, D-20261006-03]. `Counter_LightAttack_1`–`11` are never reached and use placeholder editor sounds **[reported]**, so they are migrated only if the owner says so. |
| `FinisherData` (`DA_Finisher_GateA`, `Finisher_A`) | The paired set's front finisher, referenced by the combo set [D-20261005-47, D-20261005-57]. Which of the two is the front finisher is for the owner to pick during migration. |
| `ImpactAudioConfig`, `ImpactVFXConfig`, `HitstopConfig` | The impact feedback for the hit outcome. |
| `DefenseProfile` blocked-impact overrides | The impact feedback for the block outcome. |
| `SwingShape`, `NominalLane` | Kept. The new `Swing` field is filled for all 29 attacks by one run of "Auto-detect swing", and the designer reviews the result [D-20261005-84]. |
| The non-attack montages the migration reaches (the deflect, the counter halves, the hit and Staggered reactions, guard enter and exit) | Each gets a cancel notify where the owner places it; until then it is read as ending at montage end, with a warning [D-20261006-02]. |

### 7.4 Defense configuration and hit reactions into the defense set and reaction profiles

- **`DA_DefenseConfiguration_GateA`** splits into `DA_DefenseSet_Katana` (block reactions, the parry deflect, guard enter
  and exit), `DA_PairedSet_Katana` (the counter and finisher sequences) and `DA_ReactionProfile_Base` (the Staggered
  reaction, recoil and the global guard-facing values), plus the player's and the enemy's profiles
  [D-20261005-48, D-20261005-55, D-20261005-57].
- **`DA_DefenseConfiguration_GateB`** splits the same way, into the contact-matrix test level's defense set, paired set and
  profile.
- **`DA_HitReaction`** becomes the base profile's hit reactions:
  - the stun fields are dropped (all 0 on shipped data **[reported]**) [D-20261004-41];
  - `HeavyDamageHealthPercent` is dropped [D-20261004-42];
  - the single variant slots become arrays [D-20261002-07];
  - the stale saved `KnockbackForce` is dropped.

**Field by field, `UDefenseConfiguration` and its rows (section 4.10):**

| Today's field or group | What it does today | Where it goes | Why |
|---|---|---|---|
| `HardGuardConeHalfAngle` | Gates a parry at 70° | Removed | Any direction can be parried, and turning is never limited [D-20261004-23, D-20261005-05]. |
| `PerfectParryFinalTolerance` | Leftover angle allowed for a parry, 10° | Removed from the parry. Motion warping inside the deflect hides the leftover angle. | D-20261004-30 |
| `MaximumAutomaticTurn`, `DefenseTurnRate` | Guard-facing and block turn; parry reach; the turn budget for each paired step | Combat settings defaults, overridable on the defense set: guard facing keeps today's values; the block turn rate starts at 360°/s. They no longer limit a parry. | D-20261005-05, D-20261004-21, D-20261005-68, D-20261005-69 |
| `NormalBlockFinalTolerance` | The 35° block gate | The combat settings' guard arc, 75° | D-20261005-11 |
| `CenterLaneHalfAngle`, `BoneHeightRows` | Lane and height classification for matching rows | Kept as global classification in the combat settings if direction or swing keys need them, otherwise removed. Proposed. | — |
| `ThreatLockMinSeconds`, `ThreatSwitchLeadSeconds`, `GuardedThreatRefreshSeconds`, `DefenseThreatRange`, `GuardManualOverrideThreshold`, `GuardAutoFacingResumeSeconds` | Guard facing | Combat settings, values unchanged | Unchanged (3.11). |
| `MaximumHighConfidencePredictionAge` | How fresh a prediction must be for a parry | Removed, with the prediction gate | D-20261005-64 |
| `InteractionTombstoneSeconds`, `TerminalInteractionCacheCap`, `NormalBlockTranslationDriftTolerance` | Nothing at runtime | Removed | Unread **[reported]**. |
| `GuardEnterMontage`, `GuardExitMontage` | Nothing; never played | Defense set, and wired | D-20260929-07 ("wire it"), D-20261005-55 |
| `NoMontageParryBridgeSeconds`, `CounterWindowSeconds`, `FinisherReadySeconds` | Timers between the parry and later presses | Removed. There is no separate press window; the Staggered time replaces them. | D-20261005-24, D-20261005-22 |
| `ParryStaggerDuration` (the **attacker's** configuration) | Staggered length after a parry | The Staggered default in the combat settings, plus attack overrides | D-20261005-08 |
| `TimeDilationLeaseWatchdogSeconds` | Emergency restore for slow motion | Combat settings | Unchanged. |
| `NormalBlockTranslationAllowance`, `PerfectParryTranslationAllowancePerRole` | Warp translation caps | Block: removed, because the code-driven turn does not translate. Parry: the paired content's own warp limits (4.12). | D-20261004-21 |
| `DefaultBlockImpactAudio` and `VFX`, `DefaultParryImpactAudio` and `VFX` | Defense sound and effect defaults | Combat settings type defaults for the block and parry outcomes | D-20261004-20 |
| `DefenderPresentationRows` | Block and parry presentation, keyed by outcome, height, lane, swing and tags | Defense set: block reactions by direction, and the deflect of each counter entry | D-20261005-48, D-20261004-23 |
| `AttackerResponseRows` (`ParryStagger`, `Recoil`) | The attacker's reaction to being parried, and its recoil | Reaction profile: the Staggered reaction by direction, and recoil | D-20261005-55 |
| Row payload: `PairedBridgeData`, `ReviewedDeflectionMarker`, `bRequiresBridgePreflight` | Today's paired parry selection, and the dead re-pick | Removed. The defense set's counter entry carries the deflect and fallback strike and references the paired set's counter. | D-20261004-22, D-20261005-48, D-20261005-57 |
| Row payload: `SourceSocketOverride`, `TargetBoneOverride` | Nothing | Removed | Unread **[reported]**. |
| Row payload: `Hitstop`, the audio and VFX overrides | Block and parry feedback | The attack's impact feedback per outcome | D-20261004-20 |
| Row payload: blend times, `bEnableRotationWarp`, `MaximumTranslation` | Montage blending and warping | Fields on the defense-set and reaction-profile entries where still meaningful. The rotation warp is replaced by the code-driven turn. | D-20261004-21 |

### 7.5 Renaming the GateA and GateB assets

The decisions: [D-20261004-19, D-20261004-17]. Assets the redesign retires are removed rather than renamed. **The names
below are proposed** (Q16). They describe each asset, and they avoid the retired words [D-20261005-40].

| Today | Proposed |
|---|---|
| Folder `Data/PDA/Defense/GateA/` | `Data/PDA/Defense/Paired/` |
| Folder `Animation/Montages/Defense/GateA/` | `Animation/Montages/Defense/ParryAndBlock/` |
| Folder `Data/PDA/Defense/GateB/` (and `GateB/Attacks/`) | `Data/PDA/Defense/DefenseMatrix/` (and `DefenseMatrix/Attacks/`) |
| Folder `Animation/Montages/Defense/GateB/` | `Animation/Montages/Defense/DefenseMatrix/` |
| `DA_DefenseConfiguration_GateA` | removed: its contents go to `DA_DefenseSet_Katana`, `DA_PairedSet_Katana` and `DA_ReactionProfile_Base` |
| `DA_DefenseConfiguration_GateB` | removed: its contents go to `DA_DefenseSet_DefenseMatrix`, `DA_PairedSet_DefenseMatrix` and `DA_ReactionProfile_DefenseMatrix` |
| `DA_ParryBridge_GateA` | `DA_Paired_ParryDeflect_Front`, referenced by the paired set's front counter |
| `DA_Counter_GateA` | `DA_Paired_Counter_Front`, the paired set's front counter |
| `DA_Finisher_GateA` | `DA_Paired_Finisher_Front`, the paired set's front finisher |
| `AM_ParryBridge_Defender`, `AM_ParryBridge_Attacker` | `AM_ParryDeflect_Defender`, `AM_ParryDeflect_Attacker` |
| `AM_ParryStagger_Fallback_Middle_Center` | `AM_Staggered_Front` |
| `AM_Parry_Fallback_Middle_Center`, `AM_Block_Middle_Center`, `AM_Counter_*`, `AM_Finisher_*` | names kept; folder moves |
| `DA_GateB_HighCenter` … `DA_GateB_LowRight` (9) | `DA_DefenseMatrix_HighCenter` … `DA_DefenseMatrix_LowRight` |
| `AM_GateB_ContactMatrix` | `AM_DefenseMatrix_Attacks` |
| `AM_GateB_LowCenter` | `AM_DefenseMatrix_LowCenter` |
| `DA_CombatSettings_DefenseMatrix`, `AM_Recoil_Generic` | names kept; folder moves |
| `AI/ST_EnemyCombatProof` | `AI/ST_EnemyCombat`: it is the live enemy StateTree |

**Matching changes in the same pass:**
- the test-level names (`Lvl_DefenseMatrix`, `BP_DefenseMatrix*`) already follow this pattern;
- the automation tests named with GateA and GateB (12 of them **[reported]**) and the manifests under
  `Tools/Codex/manifests/` are renamed together with the assets.

The proof-split draft of 2026-10-03 proposed `<Seed>_GateA` names. D-20261004-17 supersedes that.

### 7.6 The owner's held content

Held for this work [D-20261004-09]. This list comes from the 2026-10-04 log (E13) and the current-rules reference
**[reported]**:

| Held change | Its new home |
|---|---|
| `LightAttack_1`–`5` knockback overrides, and `LightAttack_5`'s 0.3 s hitstop | Attack overrides |
| `DA_CombatSettings_Default` knockback defaults: Light 60 cm / 0.2 s, Heavy 150 cm / 0.4 s, as the condensed 10-04 summary records them. **Verify on the asset.** | Combat settings type defaults. The asset is the source [D-20261004-38]. |
| Trimmed hit-reaction montages, about 1–2 s per section | The base profile's hit reactions |
| Force Root Lock on all 274 DynamicKatana sequences | Kept [D-20261004-34] |
| The GhostSamurai block, parry, broken, rebound and guard enter and exit montages (untracked) | The defense set: the front block reaction, the front deflect, and guard enter and exit [D-20261005-55]. The base profile: the Staggered reaction ("broken") [D-20261005-55]. They must be committed together with the assets that reference them. |
| `ABP_SamuraiCharacter` and blend-space edits | Land with the inertialization fix below |
| `DA_HitReaction` with `HeavyDamageHealthPercent` = 1 | The field is removed [D-20261004-42] |
| The `DA_DefenseConfiguration_GateA` edit (GhostSamurai rows, guard enter and exit) | The defense set and the base profile, as above |
| `AM_Heavy_Katana_Event` | Kept as the owner left it |

**The charge-loop snap fix** lands with the animation-blueprint edits, as the owner chose [D-20261004-32]:
- the code requests inertialization after each section jump, using the authored blend times;
- `ABP_SamuraiCharacter` gets one Inertialization node, between the layered blend per bone and the output pose
  (the 2026-10-04 log, E14).

**A known breakage, from code reading only [reported]:** the uncommitted `DA_DefenseConfiguration_GateA` edit likely
breaks today's defense-authoring recipe test. Retiring the asset removes that test's subject.

### 7.7 Code retired

- **The defense configuration:** `UDefenseConfiguration`, its row and payload types, and its resolution path.
- **Today's post-parry stages** and their timers: the deflect stage before the press, the press window, and the
  finisher-ready window. PR #137 (merged as `e47da15e`) fixed the live bug in that window and is the interim behaviour;
  the parry-counter stage (9.7) retires it and keeps its failure-path tests.
- **`FFinisherTriggerConfig`.** It has no owner today. D-20260929-07 said to wire it, but D-20261005-18 removes its
  thresholds, so it is retired rather than wired (Appendix B).
- **The hard-coded 25%** (`HitReactionComponent.cpp:1423`), and the never-granted `Context.LowHealthFinisher` tag.
- **The posture stubs and fields** (`ApplyPostureDamage`, `PostureDamage`, `ChargedPostureDamage`): "no meter exists"
  [D-20261005-40].
- **Identifiers that use retired words**, renamed to the glossary's terms [D-20261005-40]. Serialized names get a
  CoreRedirect. Today's source has these counts by grep (some uses of "Chain" mean combos, not the parry, and need a
  case-by-case look):

  | Word | Files | Matches | Examples |
  |---|---|---|---|
  | `GuardBroken` | 12 | 35 | `IsGuardBroken`, `bIsGuardBroken` |
  | `Stunned` | 12 | 42 | `bIsStunned` |
  | `CounterWindow` | 42 | 240 | `AnimNotifyState_CounterWindow`, `IsInCounterWindow` |
  | `Chain` | 55 | 999 | `EChainCounterState`, `FPairedChainTransitionPolicy` |
  | `Bridge` | 21 | 397 | `PairedBridgeData` |
  | `Posture` | 10 | 49 | `UPostureSettings` |

### 7.8 Order

The migration follows the stages in section 9. Each stage converts only the data its rules need, and leaves the rest
readable as it was.

---

## 8. Testing

### 8.1 Principles

- **Fixtures own their values** [D-20261002-11]:
  - every test sets each value its assertion depends on;
  - shipped assets get only invariant checks: finite, inside their clamps, in a valid order, and structurally sound;
  - no test asserts a shipped tunable.
- **Every float assertion also checks finiteness.** In this build `TestEqual` and `IsNearlyEqual` pass on NaN (project
  memory).
- **Test the premise.** Each rule's test is shown to fail first, against today's code or against a deliberate mutation of
  the new code, before it is trusted (the user's global instructions; PRs #136 and #137 did this).
- **Pure functions are tested without a world:**
  - the selection function;
  - precedence;
  - the effective window;
  - the slowdown arithmetic;
  - the lethal check;
  - the guard-break ledger;
  - the interrupt rule.
- **Names describe behaviour**, follow `KatanaCombat.*`, use glossary terms [D-20261005-40], and carry no planning labels
  [D-20261004-17].
- **Feel is judged on rendered captures** with the capture plugins. Their metrics are reported to the owner and never gate
  the baseline, because they measure tuning.
- **Every stage ends with a green baseline** (`Tools/Codex/run-agent-baseline.ps1`).

### 8.2 Tests by rule

In each test, the fixture sets the values named, never the shipped ones.

| Rule | Automated tests (examples of names) | Rendered check |
|---|---|---|
| Guard arc | `KatanaCombat.Rules.Guard.BlocksOnlyInsideArc`: the fixture sets the arc, then attacks just inside and just outside it. | A block from about 70° off reads as frontal after the turn. |
| Guard during your own attack | `…Guard.AttackStartDropsGuard`; `…Guard.BufferedBlockRaisesGuardOnlyIfStillHeld`; `…Guard.BufferedBlockNeverParries`. | — |
| No chip damage | `…Guard.BlockDealsNoDamageOnEveryAttackType`. | — |
| Block turn | `…Guard.BlockTurnFacesAttackerFully`; `…Guard.BlockTurnRateFromDefenseSetElseSettings`: the fixture sets the rate. | The turn reads as natural at the tuned rate. |
| Staggered player | `…Staggered.PlayerCannotAttackGuardOrSteer`; `…Staggered.PlayerCarriedByFullPush`; `…Staggered.PlayerUsesOwnProfileTime` (the fixture sets the time); `…Staggered.PlayerDodgeEscapeOpensAtMaxOfFloorAndHalf`; `…Staggered.PlayerDodgeOutEndsTheStateForBothSides`; `…Staggered.PlayerInputBufferedFiresAfter`. | The stumble, the push and the dodge out read as one beat. |
| Parry window | `…Parry.EffectiveWindowNeverBelowMinimum`; `…Parry.FloorAppliesAfterScale`; `…Parry.WindowNeverOpensBeforeAttackStarts`; `…Parry.NoAuthoredWindowGetsMinimum`; `…Parry.ArrivalPredictedFromTrajectoryMovesWithSlowdown`. | — |
| Parry lockout | `…Parry.MissedPressLocksOutParries`; `…Parry.PressWithNothingIncomingIsNotAMiss`; `…Parry.SuccessStartsNoLockout`; `…Parry.BlockingWorksDuringLockout`; `…Parry.LockedOutPressCountsWhenWindowStillOpenAtLockoutEnd`; `…Parry.LockedOutPressForgottenWhenWindowClosed`; `…Parry.ZeroLockoutNeverLocksOut` (the fixture sets the lockout). | — |
| Unparryable | `…Parry.UnparryableAttackIsNotACandidate`; `…Parry.AttacksAreParryableByDefault`. | — |
| Lands first | `…Parry.AnswersEarliestContact`; `…Parry.EqualContactFallsBackToAngle`. | — |
| Adaptive turn and slowdown | `…Parry.TurnFinishesExactlyAtPredictedArrival`; `…Parry.SlowdownEasesInAndOut`; `…Parry.SlowdownNeverBelowFloorSpeedOrBeyondMaxTime`; `…Parry.SucceedsWhenTurnCannotFinish`. | Side and rear parries pressed late, midway and early, measured for foot slide, pelvis jump and yaw rate, as in the earlier prototype. |
| Parry-counter outcome | `…Counter.LethalCounterKillsAtItsContact`; `…Counter.AuthoredKillSequencePlaysInsteadWhenLethal`; `…Counter.NoKillSequenceNeeded`; `…Counter.NonLethalCounterLeavesStaggered`; `…Counter.OutcomeFixedAtPress`, in which the health changes between the press and contact and the clip does not; `…Counter.DamageAppliedAtContactNeverClamped`. | Front, side and rear parry-counters, lethal and non-lethal. |
| Counter damage | `…Counter.ContentThenDamageThenOutcome`; `…Counter.UsesEntryDamageElseSettingsDefault`; `…Counter.ClipOverrideBeatsEntryDamage`; `…Counter.FallbackStrikeAlwaysDealsEntryDamage`; `…Counter.StaggeredStartsAtEnemyCancelNotify`. | — |
| Cancel notify | `…CancelNotify.PlayerFreeFromTheirOwn`; `…CancelNotify.EnemyStaggeredFromItsOwn`; `…CancelNotify.BothUntouchableUntilTheirOwn`; `…CancelNotify.HitReactionActsOnceHitStunAlsoOver`; `…CancelNotify.FallbackStrikeCarriesItsOwn`; `…CancelNotify.AttacksCancelAtPhaseTransition`; `…CancelNotify.MissingMeansMontageEndAndWarns`. | The hand-off from the counter to free play reads as one motion. |
| Swing detection | `…Editor.AutoDetectSwingClassifiesLateralOverheadAndThrustPaths`, on synthetic socket paths; `…Editor.AutoDetectSwingAsksBeforeOverwritingAHandValue`. | — |
| Counter fallback | `…Counter.FallbackOrderDirectionSwingThenDirectionThenAnyThenStrike`; `…Counter.LethalFallbackStrikeKillsAtContact`; `…Counter.FinisherPoolNotConsultedForParryCounter`. | — |
| Follow-up strike | `…FollowUp.AppliesMultiplierToResolvedAttackDamage`; `…FollowUp.ReachIs350cm`; `…FollowUp.NonLethalEndsStaggeredImmediately`; `…FollowUp.OnlyOnePerParryCounter`; `…FollowUp.PairedClipWhenDirectionMatches`; `…FollowUp.LethalPlaysPairedFinisher`; `…FollowUp.PressElsewhereLeavesStaggeredRunning`; `…FollowUp.LightPressUsesLightOpenerFromInitialPress`; `…FollowUp.HoldDoesNotOpenOnStaggeredTarget`; `…FollowUp.DirectionalLightNeverFollowsUp`; `…FollowUp.PressInsideSequenceFiresAtPlayerCancelNotify`; `…FollowUp.LastInputWinsInsideSequence`. | The follow-up strike reads clearly. |
| Staggered | `…Staggered.OneStateOneDuration`; `…Staggered.CausingAttackOverrideWins`; `…Staggered.PlayerTimeIgnoresAttackAndSlotOverrides`; `…Staggered.EnemyHeldWhileStaggered`; `…Staggered.RunsBesideHitStunFreeWhenBothEnd`; `…Staggered.NoGuardWhileStaggeredEvenInHitStun`; `…Staggered.DodgeOutNeedsBothMinimums`; `…Staggered.HitNeverShortensALock`. | — |
| Guard break | `…GuardBreak.GuardBreakerBreaksAtOnce`; `…GuardBreak.SlidingWindowSumsLastWSeconds` (the fixture sets N and W, on a fixture enemy that guards); `…GuardBreak.CountClearsOnBreak`; `…GuardBreak.CountingToggleOffNeverCounts`; `…GuardBreak.PlayerOnlyBreaksToGuardBreakers`; `…GuardBreak.BreakingHitDealsNoDamage`; `…GuardBreak.UnblockableWinsOverGuardBreaker`; `…Content.UnblockableGuardBreakerPairWarns`. | The guard-break reaction reads clearly. |
| Interruption | `…Interrupt.OnlyWhenPressureExceedsArmor`, using strict inequality; `…Interrupt.ArmorIsMaxOfBaseAndWindow`; `…Interrupt.FlinchKeepsCurrentAction`. | A flinch and an interrupt are told apart at a glance. |
| Hit-stun | `…HitStun.NeverBelowFloor`; `…HitStun.ActionsWaitGuardAllowed`; `…HitStun.InputBufferedFiresAfter`; `…HitStun.EnemyAIHoldsThenResumes`; `…HitStun.DodgeEscapeOpensAtMaxOfFloorAndHalf`; `…HitStun.EscapeDodgeIsCommitted`. | Hit-stun lengths read well. |
| Knockback by outcome | `…Knockback.FlinchDoesNotPush`; `…Knockback.BlockedHitUsesBlockShove`; `…Knockback.GuardBreakUsesFullPush`; `…Knockback.StaggeredOrInHitStunVictimsPushNormally`; `…Knockback.NoneDuringPairedSequence`. | The block shove and the break push read as different weights. |
| Paired finishers | `…Finisher.OnlyWhenLethal`; `…Finisher.AnyLethalAimedStrikeQualifies`; `…Finisher.NoHealthPercentRoute`; `…Finisher.StaggeredAloneDoesNotFinish`; `…Finisher.KeyedByDirectionOnly`; `…Finisher.NoClipForDirectionPlaysNormalDeath`; `…Finisher.AimConeAndRange`; `…Finisher.EnemyOnlyWhenMarked`; `…Finisher.GuardBreakAloneGrantsNoEnemyFinisher`; `…Finisher.MarkedEnemyLethalHitOnStaggeredPlayerStillFinishes`. | Alignment from up to 350 cm. |
| Several attackers | `…Attackers.AtMostConfiguredCount`; `…Attackers.CooldownPerEnemy`; `…Attackers.OthersHoldDuringParryCounterAndPairedFinisher`; `…Attackers.InProgressSwingsFinish`; `…Attackers.InProgressSwingCannotTouchBeforeCancelNotify`. | A one-against-four crowd fight. |
| Targeting | `…Targeting.FreeModeConeAroundStickDirection`; `…Targeting.FreeModeConeAroundFacingWhenNoDirectionHeld`; `…Targeting.StaggeredOrEligibleWinsTiesElseNearest`; `…Targeting.CameraNeverDecides`; `…Targeting.OneRuleForOrdinaryFollowUpAndFinisher`; `…Targeting.LockPicksBestByConeAndScores`; `…Targeting.LockedPressesAimAtLockedTarget`; `…Targeting.FlickSwitchesToNearestInThatDirection`; `…Targeting.LockDropsOnDeathAndRePicksWhenNear`; `…Targeting.SecondPressDropsLock`. | Attacks go where the stick points; the lock, strafe and switch read as one system. |
| Precedence | `…Data.SlotOverrideWins`; `…Data.AttackOverrideBeatsSetDefault`; `…Data.SetDefaultBeatsSettings`; `…Data.ResolvedSpecUnchangedByMidAttackEdit`. | — |
| Weapon kit | `…Data.WeaponDataSuppliesThreeSets`; `…Data.NoWeaponDataUsesSettingsDefaultKit`; `…Data.ReferencesStayInsideTheWeaponsPairedSet`. | — |
| Selection function | `…ComboSet.ReturnsChosenAndRejectionReasons`; `…ComboSet.BackToOpenerIsTheOnlyLegalCycle`. | — |
| Migration | `…Migration.ComboSetReproducesSourceGraph`, run on a fixture graph; `…Migration.NeverOverwritesDesignerEdits`; `…Migration.RenamesLeaveRedirectors`. | — |
| Shipped content | `…Content.ShippedSettingsFiniteAndInClamp`; `…Content.ShippedComboSetsValidate`; `…Content.ShippedDefenseAndPairedSetsValidate`; `…Content.CoverageGapsWarnedNotFailed`. | — |
| Glossary | `…Content.NoRetiredWordsInNewIdentifiers`: a scan that ratchets down as renames land. | — |

### 8.3 Rendered checks

- They use the pinned PresentationCapture and AnimationAnalysis tooling.
- The knockback slope scenario: up, down and across a ramp [D-20261004-40].
- Measures carried over from the prototype: foot slide, pelvis velocity jump, yaw rate, blend discontinuity, and pair
  error at the clash. They are reported, and the owner judges them.

---

## 9. Staged delivery

**Every stage:**
- ships working software, with a green baseline;
- has its rule tests and, where feel is involved, rendered checks;
- has the owner's review;
- goes in a pull request named for what it does, with no planning labels [D-20261004-17].

**The order below is proposed** (Q18). Each stage lists the decisions it implements and the questions it needs answered
first.

### 9.1 Combo sets with today's behaviour

- **What:**
  - `UComboSet` with slots, links, openers and the back-to-opener link;
  - the selection function with its reasons;
  - the runtime reading the set;
  - set validation;
  - migrating `DA_Configuration_Katana` and the 20 combat attacks into `DA_ComboSet_Katana`;
  - the old link fields kept read-only.
- **Ships:** the same combos, now with loops that validate.
- **Decisions:** D-20261005-01 (structure), D-20261005-03.
- **Needs:** Q9, Q10 to Q14. PR #139 has merged.

### 9.2 The moveset grid

- **What:**
  - the grid and the node map;
  - the timing strip, the badges and "who uses this set";
  - pickers, edit-through and make unique;
  - inline overrides for the fields that are already layered (knockback, damage).
- **Ships:** the owner's chosen editor.
- **Decisions:** D-20261005-02.
- **Needs:** approval of section 5.

### 9.3 Layered attack data, the weapon kit and reaction profiles

- **What:**
  - the impact block on attacks, as data, with set-level defaults and slot overrides;
  - the resolved attack spec carried on hits;
  - `UDefenseSet` and `UPairedSet`, filled with today's block, deflect, counter and finisher content, and the weapon
    data referencing a combo set, a defense set and a paired set, with the default weapon data as the fallback;
  - `UReactionProfile` with a base profile and inheritance;
  - retiring `UDefenseConfiguration`;
  - the owner's held content;
  - the asset renames;
  - the charge-loop snap fix.
- **Ships:** the same rules, with one place per question and the owner's tuning live.
- **Decisions:** D-20261004-20, D-20261004-22 (placement per D-20261005-48), D-20261004-32, D-20261004-38,
  D-20261004-41 (data), D-20261004-19, D-20261002-07, D-20261005-45 (keys), D-20261005-46, D-20261005-47 (no body
  types), D-20261005-48, D-20261005-49, D-20261005-55, D-20261005-56, D-20261005-57.
- **Needs:** Q1, Q16, Q15.

### 9.4 Hit rules

- **What:**
  - pressure against armor, with armor windows and the flinch;
  - hit-stun with its floor, holding attacks and movement while input buffers;
  - one Staggered state, still caused only by a parry-counter at this stage, running beside hit-stun with both ending
    before the character is free [D-20261006-01];
  - the cancel notify on hit reactions, read as montage end with a warning where missing [D-20261006-02];
  - knockback by outcome: no push on a flinch, the block shove, the full push on a guard break, none in paired sequences;
  - the damage ÷ health rule removed.
- **Decisions:** D-20261004-42, D-20261005-17, D-20261005-34, D-20261005-37, D-20261005-53, D-20261005-58,
  D-20261005-59, D-20261005-60, D-20261005-87, D-20261006-01, D-20261006-02.
- **Needs:** Q2, Q3, Q8.

### 9.5 Guard rules

- **What:**
  - the ±75° arc and the code-driven block turn, with their settings defaults and defense-set overrides;
  - guard dropping when an attack starts, and Block buffered until Recovery;
  - guard break by guard breakers, live for the player;
  - the counted guard break, built now and proven on fixtures whose enemy guards, dormant in shipped play until a
    shipped enemy guards [D-20261007-15];
  - unblockable wins over guard breaker at contact, with the validation warning on the pair [D-20261007-16].
- **Decisions:** D-20261005-11 to D-20261005-15, D-20261005-25, D-20261005-26, D-20261005-29, D-20261005-31,
  D-20261005-46, D-20261005-52, D-20261005-56, D-20261005-59, D-20261005-66, D-20261005-69, D-20261005-70,
  D-20261005-83, D-20261005-88, D-20261004-21, D-20261007-15, D-20261007-16.
- **Needs:** nothing open. PR #136 has merged.

### 9.6 Parry rules

- **What:**
  - the effective window, with its minimum and scale;
  - the lockout in its lenient form, with the press remembered through it, and the unparryable flag;
  - lands first;
  - the adaptive turn and the attacker slowdown;
  - parry feedback and parry assists.
- **Meanwhile:** after a successful parry, today's post-parry sequence still plays.
- **Decisions:** D-20261005-05 to D-20261005-07, D-20261005-09, D-20261005-10, D-20261005-32, D-20261005-33,
  D-20261005-36, D-20261005-50, D-20261005-64, D-20261005-71 to D-20261005-79, D-20261004-06, D-20261004-07,
  D-20261007-18.
- **Also needs:** the owner's veto or approval of the lead-time authoring (R-20261005-01).
- **Needs:** Q8.

### 9.7 The parry-counter, and paired finishers by lethal damage

- **What:**
  - the counter entries with their damage, the paired set's counters and their fallback, and the optional kill sequence;
  - the decision at the press: content, damage, outcome; damage at the counter's contact, never clamped; the kill at
    contact, or the authored kill sequence;
  - the cancel notify on every clip the sequence plays, and the protection until each character's own;
  - the follow-up strike, paired when a clip matches the direction; the light opener from the initial press, with no
    hold; presses buffered inside the sequence, last input wins;
  - no latch and no end-position rule; a press aimed elsewhere leaves Staggered running;
  - free-mode aim for every attack press: the cone around the stick direction, or the facing when no direction is held,
    replacing today's soft aim (3.13);
  - paired finishers on any lethal aimed strike, keyed by direction, at 350 cm;
  - the finisher pool, and the normal death when no clip matches;
  - enemy paired finishers for marked types only;
  - today's post-parry stages, including #137's interim window, and the 25% threshold removed.
- **Decisions:** D-20261005-18 to D-20261005-24, D-20261005-27, D-20261005-28, D-20261005-30, D-20261005-38,
  D-20261005-39, D-20261005-42, D-20261005-43, D-20261005-45, D-20261005-47, D-20261005-51, D-20261005-54,
  D-20261005-57, D-20261005-80 to D-20261005-82, D-20261005-84 to D-20261005-86, D-20261006-02 to D-20261006-05,
  D-20261007-07 to D-20261007-12, D-20261007-14, D-20261007-17.
- **Needs:** Q4, Q5, Q19; counter content (3.3).
- **Content notes [reported]**, which bind no rule [D-20261006-04]:
  - 8 counter-shaped two-character pairs are owned, all starting with the attacker in front. No pack on the market has
    counter pairs where the attack comes from behind.
  - 90° guarded turns: DSAS `Block_Turn_L90` and `R90` at 0.571 s, and DynamicKatana `Turn_Block_90` at 0.833 s.
  - 180° turns of 0.867–1.5 s.
  - A speed sweep found ×2.0 the best-looking rate for the DSAS left 90° and right 180° turns. It also found that every
    DSAS turn starts with a pelvis jump of about 181 cm/s, and that the DSAS right 90° turn spins its capsule about 150°
    in 4 frames. The sweep measured these clips; it chose no values.
  - Where the owned clips do not fit the rules, the gap is met by creating, buying or procedurally fitting content, not
    by narrowing a rule [D-20261006-04].

### 9.8 Lock mode

- **What:** the lock press and the best target by the cone and its scores; every press aimed at the locked target; the
  strafe facing it; the flick (or scroll) switch to the nearest enemy in that direction; the drop on the target's death
  with the automatic re-pick when one is near; the second press (3.13).
- **Ships:** the optional lock the pillars ask for.
- **Decisions:** D-20261007-13, D-20261007-14.
- **Needs:** Q20; the camera's framing while locked, which belongs to the camera design and is not in this spec.

### 9.9 Several attackers

- **What:** two attack tokens, a per-enemy cooldown, and others hold instead of aborting.
- **Decisions:** D-20261005-21, D-20261005-35, D-20261005-36, D-20261007-08.
- **Needs:** Q6, Q7.

### 9.10 Removing the deprecated fields

- **What:**
  - the old link fields, `CounterData` and `FinisherData`, and the unread fields;
  - the remaining identifiers that use retired words.
- **Decisions:** D-20261005-40.

---

## 10. Open questions for the owner

Each question is an A–D form, and the recommended option is marked. The recommendations are this spec's. **None is a
decision until the owner picks it.** Eight questions of the first draft were answered by D-20261005-42 to -49 and are
removed; one new question, how today's attacks map onto the four swings, came out of D-20261005-45 and was answered by
D-20261005-84. The second review removed three more that active decisions
already answer (the window floor, the aimed-elsewhere press, the follow-up on a guard break), and one that events
settled (PR #137). The owner's answers of 5 October (D-20261005-50 to -70) removed the turn-rate, flinch-push,
enemy-kit, buffered-Block, Staggered-player and block-turn questions; the walk of 3.2 (D-20261005-71 to -79) removed the
eight parry questions; the walk of 3.3 to 3.5 (D-20261005-80 to -88) removed the counter, Staggered and guard-break
questions, and D-20261005-88 added one. The answers of 6 and 7 October (D-20261006-01 to D-20261007-18) removed three
more: the tie rule in the aim cone (D-20261007-12), the in-progress swing during your own sequence (D-20261007-08) and
the player's Staggered time against an attack's override (D-20261006-06); they added two, a Heavy press on a Staggered
target (Q19) and lock mode's numbers (Q20).

### Hits

**Q1. What makes a heavy attack "charged"?**
- A. Its hold check found Heavy still held, so it entered its charge loop. This is today's yes-or-no charge state.
  **(Recommended.)**
- B. Only a full charge, when `MaxChargeTime` is reached.
- C. A charge level above a threshold.
- D. A separate attack asset marked as charged.

**Q2. What defaults do Special-type attacks use?** No Special attack ships, and today Special looks Heavy.
- A. The Heavy column, until special attacks are designed. **(Recommended: it matches today's look.)**
- B. Explicit values required, with a validation error without them.
- C. The Light column.
- D. Their own column, with values to choose.

**Q3. A second interrupting hit lands during hit-stun. What happens to the hit-stun?** Today the new duration replaces
the old one, even when it is shorter.
- A. Restart with the new hit's duration, as today.
- B. Keep the longer of the time left and the new duration. **(Recommended: a light hit cannot cut a heavy's hit-stun
  short.)**
- C. Add them.
- D. Ignore new hits until it ends.

### Paired finishers

**Q4. For a Heavy press the charge is unknown at the press. When is "lethal" decided?**
- A. At the press, from the uncharged damage. A charge that later makes the hit lethal kills with the ordinary heavy hit.
  **(Recommended: the decision stays at the press.)**
- B. At release, from the latched charge.
- C. At the press and again at release.
- D. A Heavy press never starts a paired finisher.

**Q5. For a buffered press, is "the press" the moment it is pressed or the moment it executes?**
- A. When it executes, against the target and its health at that moment. **(Recommended.)**
- B. When it is pressed; the result is kept until it executes.
- C. When it is pressed, and cancelled if the target has changed.
- D. A buffered press never starts a paired finisher.

### Several attackers

**Q6. What is the per-enemy attack cooldown, and when does it start?**
- A. 1.0 s, starting when the enemy's attack ends. **(Recommended: today's value, counted so that it actually spaces
  attacks out.)**
- B. 1.0 s, starting when the token is granted, as today.
- C. 0.5 s, from the end of the attack.
- D. Set per enemy type.

**Q7. Do other enemies hold during a non-lethal follow-up strike?**
- A. No: D-20261005-21 covers parry-counters and paired finishers only. **(Recommended.)**
- B. Yes, as for a parry-counter.
- C. Only when the strike is paired.
- D. Only against the struck enemy.

### Guard, clocks and data

**Q8. Which clock do combat durations use?**
- A. Game time for all of them (Staggered, hit-stun, the lockout, the guard-break window and the windows), so that slow
  motion and hitstop stretch them alike. The slowdown's 0.3 s cap counts real time. **(Recommended.)**
- B. Real time for all of them.
- C. Game time for character states (Staggered, hit-stun), and real time for input timers (lockout, buffer, grace,
  guard-break window).
- D. Chosen per setting.

**Q9. What does a press do when the slot has no link for that input?**
- A. The combo ends: the press is buffered and starts the opener once the attack finishes, without a combo blend.
  **(Recommended: it stays distinct from an explicit back-to-opener link, which continues the combo.)**
- B. The same as a back-to-opener link.
- C. The press is dropped.
- D. Whatever today's code does, once verified.

### Migration and delivery

**Q10. `LightAttack_11`.** No link reaches it; it is Unblockable and Terminal.
- A. Migrate it as an unlinked slot, flagged unreachable, for the owner to place. **(Recommended: nothing is lost.)**
- B. Link it after `LightAttack_10` as the light combo's last hit.
- C. Delete it.
- D. Leave it out of the combo set but keep the asset.

**Q11. The `None` direction key.** Every light attack maps the `None` direction to `DA_DirectionalAttack_None`, which
nothing can reach.
- A. Drop the key and leave the asset unreferenced, reported by the orphan report. **(Recommended.)**
- B. Drop the key and delete `DA_DirectionalAttack_None`.
- C. Keep it as the hold-release link for a release with no direction.
- D. Migrate it exactly and flag it.

**Q12. The unused Sprint, Jump and Plunging slots** on the attack configuration, which nothing reads.
- A. Remove them; they come back as entry rules when those moves are designed. **(Recommended.)**
- B. Migrate them as entry rules now, empty.
- C. Migrate them as empty slots in the combo set.
- D. Keep the attack configuration asset for them.

**Q13. The directional context tag.** The five directional attacks require `Context.DirectionalFollowUp`, nothing grants
it, and the hold-release path skips the gate.
- A. Drop the requirement; the hold link is the gate. **(Recommended.)**
- B. Grant the tag on hold release and keep the requirement.
- C. Keep the requirement and let the editor flag it.
- D. Migrate exactly and flag it.

**Q14. The Terminal tag.** `HeavyAttack_4` is Terminal yet has follow-ups, and fails validation today.
- A. Replace Terminal with empty links; the tag is removed. **(Recommended.)**
- B. Keep the tag and make it set every link to None at migration.
- C. Keep the tag as a validation hint only.
- D. Migrate exactly and flag it.

**Q15. Enemies' per-actor attack choices.** Enemies now pick slots of their combo set by per-slot weight and range
[D-20261005-63]. Today `BP_EnemyCharacter` lists `LightAttack_1`, and each of the four placed enemies in
`Lvl_ThirdPerson1` overrides that list with a different single attack; a weight stored on a shared combo set cannot
express that.
- A. Give enemies their own combo set, seeded from the Blueprint's list, and let a placed actor override slot weights per
  instance (an optional map on the AI component). **(Recommended: one set per enemy type, instance tweaks where
  needed.)**
- B. One combo set per distinct list: four small sets for the four placed enemies.
- C. Drop the per-actor differences; every enemy uses the same weights.
- D. Keep per-actor attack lists as a list of slot ids with weights.

**Q16. Do you approve the proposed asset names (7.5)?**
- A. Approve as proposed. **(Recommended.)**
- B. Approve, but keep test-only assets under one `Test` folder.
- C. Rename the data assets only; montages keep their names.
- D. Propose different names.

**Q17. What remains of "defense role per enemy type" (D-20261004-27), now that there are no per-enemy-type modes
(D-20261005-24)?** D-20261004-27 chose counter-led grunts and a parry-centric kit for duelists and elites. D-20261005-24
made one parry model for every enemy.
- A. The rules are the same for every enemy; enemy types differ only in content: which attacks they have (unparryable,
  guard breakers), whether they guard, and their reaction profile values (armor, guard-break threshold).
- B. D-20261004-27 is superseded entirely, and enemy types are designed from scratch later.
- C. Enemy types keep distinct parry rules, for example different Staggered times or parry windows per type.
- D. Decide when enemy types are designed; until then the spec assumes nothing about them.

Recommendation: A. It keeps D-20261004-27's intent (grunts fall to counters, elites demand parries) through content and
per-character values, which this data model already provides, without a second rule set.

**Q18. Do you approve the stage order (section 9)?**
- A. As proposed: the authoring foundation, the editor and the data, then hits, guard, parry, the parry-counter with
  free-mode aim, lock mode and attackers. **(Recommended: attack data changes once, as D-20261005-04 intended.)**
- B. Rules first, on today's attack data, with authoring after.
- C. Data and rules first, and the editor last.
- D. A different order.

### The follow-up strike and targeting

**Q19. With a Staggered target in the cone, what does a Heavy press do?** D-20261007-17 settles the light press: the
light opener, from the initial press, with the hold not opening. It says nothing about Heavy, whose hold is the charge.
- A. The heavy opener, from the initial press, with the charge hold not opening, as for the light. **(Recommended: one
  rule for both buttons, and the decision stays at the press.)**
- B. The heavy opener, and the charge hold opens; a charged release is the follow-up at ×1.5 of its charged damage.
- C. A Heavy press is never a follow-up strike; it is an ordinary attack on the Staggered enemy.
- D. The light opener, whichever button was pressed.

**Q20. Lock mode's two distances: how far does a lock press look, and how near must the next enemy be for the
automatic re-pick on death?** D-20261007-13 names neither. The spec has two distances already: the 350 cm finisher
range [D-20261005-28] and today's 500 cm soft-aim range.
- A. Both 350 cm, the finisher range. **(Recommended: one number, and a lock never reaches past a finisher.)**
- B. Both 500 cm, today's soft-aim range.
- C. The lock press looks to 500 cm; "near" is 350 cm.
- D. Two separate tunables, with no decided value, tuned on captures.

---

## Appendix A. Decisions used

| Decision | What it decided | Where |
|---|---|---|
| D-20261002-07 | Reaction variants become arrays; death reads the array | 4.8, 7.4 |
| D-20261002-10 | Log when the defense slot is empty (carried to the profile slot) | 4.8 |
| D-20261002-11 | Tests never pin configurable values | 8.1 |
| D-20261003-01, -03 | The hit lock in the current reaction mode; inputs buffered | 3.7 |
| D-20261003-08 | Consolidation into attack, profile and settings (confirmed by D-20261004-20) | 4 |
| D-20261004-02 | Guard aim with magnetism (queued) | 3.11 |
| D-20261004-03 | Player free, attacker held | 3.3, 3.4 |
| D-20261004-06, -07 | Parry readability; parry assists as settings | 3.2, 4.8 |
| D-20261004-09 | Hold the owner's content for this work | 7.6 |
| D-20261004-17, -19 | No planning labels; Gate assets renamed here | 7.5, 8.1, 9 |
| D-20261004-20 | Data model with defaults and overrides, and a base profile. Its "profile holds block and parry" is superseded by D-20261005-48 | 1.3, 4 |
| D-20261004-21 | Code-driven block and parry turn | 3.1, 3.2 |
| D-20261004-22 | Explicit parry fallback; its placement is superseded by D-20261005-48 | 4.6, 7.4 |
| D-20261004-23 | Parry any direction; block front; data keyed by direction | 3.1, 3.2 |
| D-20261004-24 | One block rule (merged in #136) | 3.1 |
| D-20261004-26 | Design pillars | 1.2 |
| D-20261004-27 | Defense role per enemy type (Q17) | 3.5, Q17 |
| D-20261004-30 | Turn-deflect into front pairs | 3.2, 3.3 |
| D-20261004-32 | Charge-loop snap fix with the redesign | 7.6, 9.3 |
| D-20261004-33 | Timing read from the montage (merged in #139) | 5.4, 7.2 |
| D-20261004-34 | Keep Force Root Lock | 7.6 |
| D-20261004-35 | Aim decides; the state is called Staggered | 3.3, 3.4 |
| D-20261004-38 | Knockback layering | 3.10, 4.2 |
| D-20261004-41, -42, -43 | Hit-stun on the attack; type looks, pressure interrupts; guard break added | 3.5–3.7 |
| D-20261004-45 | Exact mechanics before code | header |
| D-20261005-01 to -04 | Combo set with edit-through; grid and node map; back-to-opener links; one spec | 4.4, 5 |
| D-20261005-05 to -10 | The parry window, lockout, parryable by default, Staggered default, minimum and scale, slowdown | 3.2, 3.4 |
| D-20261005-11 to -15 | Guard arc and turn, no chip, block in recovery, the player's guard, counted guard break | 3.1, 3.5 |
| D-20261005-16, -17 | Interruption is pressure against armor | 3.6 |
| D-20261005-18 to -21 | Finisher eligibility, trigger, enemy finishers, others hold | 3.8, 3.9 |
| D-20261005-22 to -24 | Opportunity equals Staggered time; strike fallback; the parry is the counter (the parry-counter, since D-20261007-11) | 3.3 |
| D-20261005-25, -26 | Count by pressure; 4 within 3 s | 3.5 |
| D-20261005-27, -28 | 60° aim cone; 350 cm. **The cone's camera reference is replaced by D-20261007-12** (the stick direction, else the facing); the 60° stands | 3.3, 3.8, 3.13 |
| D-20261005-29 to -31 | Guard breakers; counter fallback order; guard-break Staggered | 3.3–3.5 |
| D-20261005-32, -33 | Lockout 0.25 s; slowdown bounds | 3.2 |
| D-20261005-34 to -37 | Hit-stun values; 2 attackers; lands first; what hit-stun blocks | 3.7, 3.9 |
| D-20261005-38, -39 | A lethal parry-counter ends in a kill, decided at the press; the follow-up strike. **The lethal-variant shape is amended by D-20261007-07:** optional, not required | 3.3, 3.8 |
| D-20261005-40 | The glossary; its parry and counter entries amended by D-20261007-11 | 2, throughout |
| D-20261005-42 | Any lethal aimed strike plays a paired finisher; the glossary line amended | 1.3, 2.1, 3.8 |
| D-20261005-43 | The follow-up strike is paired when a clip matches | 3.3 |
| D-20261005-44 | Armor is the higher of the base and any active window | 2.1, 3.6 |
| D-20261005-45 | Counters keyed by direction and swing, lane optional | 3.3, 4.5, 4.6 |
| D-20261005-46 | Guard-break counting is a per-character toggle, off for the player | 3.5, 4.8 |
| D-20261005-47 | No body types; finishers by direction only; normal death without a clip; no "can be finished" marker | 3.3, 3.8, 4.6, 4.8 |
| D-20261005-48 | Weapon data → combo set + defense set (plus the paired set since D-20261005-57); the reaction profile keeps the body's data | 2.1, 4 |
| D-20261005-49 | No weapon data means the combat settings' default weapon data | 4.3, 6.1 |
| D-20261005-50 | The parry turn finishes exactly at contact: rate = angle ÷ time left; the slowdown is the safety valve | 3.2 |
| D-20261005-51 | Staggered ends at once after a non-lethal follow-up strike; one follow-up per parry-counter | 3.3, 3.4 |
| D-20261005-52 | The hit that breaks a guard deals no damage; the guard breaks and the defender is Staggered | 3.5 |
| D-20261005-53 | Hit-stun holds enemies as it holds the player; the AI resumes after | 3.7 |
| D-20261005-54 | A lethal counter with no lethal variant uses the finisher for that direction, else the non-lethal counter and the normal death. **Read as superseded in its middle step by D-20261007-07** (Appendix B) | 3.3 |
| D-20261005-55 | Guard enter and exit on the defense set; the Staggered reaction and recoil on the reaction profile | 4.6, 4.8, 7.4 |
| D-20261005-56 | Guard-break counting on in the base profile, off for the player | 3.5, 4.8 |
| D-20261005-57 | The paired set: a third weapon asset holding the counters and finishers by direction and role. **Its lethal-and-non-lethal-variant shape is amended by D-20261007-07** | 2.1, 4.1, 4.3, 4.7, 5.9, 7 |
| D-20261005-58 | No push on a hit that does not interrupt | 3.6, 3.10 |
| D-20261005-59 | The block shove on a blocked hit; the full push on the hit that breaks a guard | 3.5, 3.10, 4.9 |
| D-20261005-60 | Normal pushes on victims that are Staggered or in hit-stun; none during paired sequences | 3.10 |
| D-20261005-61 | Four precedence layers; the combo set's is the per-weapon layer | 1.3, 4.2 |
| D-20261005-62 | The player is fully free after a non-lethal counter; aiming elsewhere ends the opening. **The aimed-elsewhere half is superseded by D-20261007-10**; the freedom stands | 3.3, 3.4 |
| D-20261005-63 | Enemies pick slots of their combo set by per-slot weight and range; AI attack lists retired | 4.4, 6.4, 7.2 |
| D-20261005-64 | The parry succeeds on timing alone; room matters only for paired content | 3.2, 3.3, 3.8 |
| D-20261005-65 | Sections 1, 2 and 3.1 approved as written at `a34ecd69` | 1, 2, 3.1 |
| D-20261005-66 | A buffered Block press raises guard only if still held, and never parries | 3.1, 3.2 |
| D-20261005-67 | A Staggered player can do nothing until it ends; input buffered. **Superseded by D-20261005-88** | — |
| D-20261005-68 | A tunable lives with the thing it describes; the reaction profile covers reactions to hits only | 2.1, 4.1, 4.6, 4.8, 4.9 |
| D-20261005-69 | The block turn faces the attacker fully; its rate is a defense tunable, 360°/s to start | 3.1, 4.6, 4.9 |
| D-20261005-70 | The guard arc's default is in the combat settings, overridable on the defense set | 3.1, 4.6, 4.9 |
| D-20261005-71 | A missed parry is a press while a parryable attack aimed at you is in progress, outside its window | 3.2 |
| D-20261005-72 | No lockout after a successful parry | 3.2, 3.9 |
| D-20261005-73 | Contact is the predicted blade arrival, computed from the swing trajectory | 2.2, 3.2 |
| D-20261005-74 | No authored window: the 0.35 s minimum at runtime plus an editor warning | 3.2, 4.4, 7.3 |
| D-20261005-75 | The fastest natural parry turn: defense set with a settings default, 720°/s to start | 3.2, 4.6, 4.9 |
| D-20261005-76 | The attacker slowdown eases in and out | 3.2 |
| D-20261005-77 | A Block press during hit-stun can parry | 3.1, 3.2, 3.7 |
| D-20261005-78 | Assists default neutral; parry feedback values from a rendered sweep | 3.2, 4.8, 4.9 |
| D-20261005-79 | The window and the turn are independent; both reference the predicted arrival | 3.2 |
| R-20261005-01 | A ruling, proposed: the window is authored as a lead time before arrival | 3.2 |
| D-20261005-80 | Each counter's own damage; settings default 50. **Its home is amended by D-20261006-03:** the defense-set entry, with a clip override optional | 3.3, 4.6, 4.7, 4.9 |
| D-20261005-81 | Staggered starts when the non-lethal counter sequence ends; read as the enemy's cancel notify since D-20261006-02 and D-20261007-07 | 3.3, 3.4 |
| D-20261005-82 | ×1.5 multiplies the damage of the attack the press resolves to | 3.3 |
| D-20261005-83 | A sliding 3 s guard-break window; the count clears on a break | 3.5 |
| D-20261005-84 | A per-attack Swing field, filled by an "Auto-detect swing" editor action, overridable by hand | 2.2, 3.3, 4.5, 5.3, 7.3 |
| D-20261005-85 | Follow-up strike reach 350 cm | 3.3, 3.8, 6.3 |
| D-20261005-86 | A guard break grants an enemy nothing; a marked enemy's lethal hit still finishes | 3.5, 3.8 |
| D-20261005-87 | A dodge press ends the player's hit-stun early, after max(0.12 s, half the stun); the dodge is committed | 3.7, 6.3 |
| D-20261005-88 | The player's Staggered state: reaction and full push; no attack, guard or stick movement; a dodge after the minimum ends it and closes the enemy's opening; 0.6 s by default on the player's profile | 2.1, 3.1, 3.4, 3.5, 4.8, 6.3 |
| D-20261006-01 | Hit-stun and Staggered both run, each with its own timer and rules; free when both end; a dodge-out needs both minimums; a hit never shortens a lock or restores guard early | 3.4, 3.7, 6.3, 9.4 |
| D-20261006-02 | One general cancel notify on any non-attack montage, with one meaning per context; attacks keep the phase transition; missing means montage end with a warning | 2.2, 3.0, 3.3, 3.4, 3.6, 3.7, 4.6–4.8, 4.12, 4.13, 5.4, 7.3, 9.4, 9.7 |
| D-20261006-03 | Counter damage on the defense-set entry, settings default 50, a clip override optional; content, then damage, then the outcome; the fallback strike deals the entry's value. Amends D-20261005-80's home | 2.1, 2.2, 3.2, 3.3, 3.12, 4.6, 4.7, 4.9, 7.3 |
| D-20261006-04 | Rules before content: no rule from what content exists; content made, bought or fitted to the rules | 1.1, 1.3, 3.0, 3.3, 9.7 |
| D-20261006-05 | Counter damage lands at the counter's contact and never clamps | 3.3, 6.4 |
| D-20261006-06 | The player's own Staggered time always applies; an attack's or slot's override never changes it | 3.4, 3.12, 4.8, 6.4 |
| D-20261007-07 | Every counter can kill: lethal damage at the counter's contact kills there, the enemy's death outcome taking over; an authored kill sequence plays instead when one exists. Amends the lethal-variant shape of D-20261005-38 and D-20261005-57 | 1.3, 2.1, 2.2, 3.3, 3.4, 3.8, 4.6, 4.7, 4.12, 9.7 |
| D-20261007-08 | Inside a parry-counter each character is untouchable until its own cancel notify | 3.0, 3.3, 3.9, 9.9 |
| D-20261007-09 | An attack press inside the sequence is buffered and fires as the follow-up at the player's cancel notify; last input wins | 3.3, 6.3, 6.4 |
| D-20261007-10 | A press aimed elsewhere no longer ends the opening; Staggered runs its time. Supersedes the aimed-elsewhere half of D-20261005-62 | 3.3, 3.4 |
| D-20261007-11 | The words: parry (the deflect), counter (the strike back), parry-counter (the one-press sequence). Amends the glossary's parry and counter entries | 2.1, throughout |
| D-20261007-12 | Free-mode aim: one rule for every press; the cone around the stick direction, else the facing; Staggered or eligible win ties, nearest otherwise; the camera never decides. Replaces D-20261005-27's camera reference | 2.1, 2.2, 3.3, 3.8, 3.11, 3.12, 3.13, 4.9, 6.3, 6.4 |
| D-20261007-13 | Lock mode: the lock press, the locked target for every press, the strafe, the flick switch, the drop on death with a re-pick, the second press | 2.2, 3.3, 3.8, 3.11, 3.12, 3.13, 4.9, 6.3, 6.4, 9.8 |
| D-20261007-14 | The targeting and camera design lives in this spec, as 3.13 | 3.13 |
| D-20261007-15 | The counted guard break is built with the guard-break stage, tested on fixtures, and dormant until a shipped enemy guards | 3.5, 6.4, 9.5 |
| D-20261007-16 | Unblockable wins over guard breaker; validation warns on the pair | 3.1, 3.5, 4.4, 4.5, 9.5 |
| D-20261007-17 | A light press with a Staggered target is the follow-up with the light opener; the hold does not open; directional lights never follow up | 3.3, 6.3, 9.7 |
| D-20261007-18 | The lenient lockout: a press during it is remembered and counts if the window is still open at its end; the lockout may go to zero. **My reading of "the more lenient version"; the owner may veto** | 2.1, 3.2, 3.12, 4.9, 6.1, 6.3, 6.4, 9.6 |

**Superseded, and not used as rules:**
- D-20261004-28, the two modes by enemy type, superseded by D-20261005-24;
- D-20261004-36's window lead, superseded by D-20261005-05;
- D-20261004-11 and D-20261004-13, superseded by D-20261004-20 and D-20261004-21;
- D-20261004-22's placement of parry entries on the reaction profile, and D-20261004-20's "the reaction profile holds
  block and parry", superseded by D-20261005-48;
- D-20261005-62's aimed-elsewhere half (a press elsewhere ends the opening), superseded by D-20261007-10;
- D-20261005-67, superseded by D-20261005-88.

## Appendix B. Ambiguities, conflicts and readings

Each item is either an open question, or a reading this spec chose that the owner should see.

**Resolved since the first draft.** These conflicts and readings were put to the owner on 5 October and decided:
- the glossary's "never plays otherwise" against D-18, D-19 and D-20: any lethal aimed strike plays a paired finisher
  [D-20261005-42];
- the "can be finished" marker against "no per-character exemptions": the marker is dropped [D-20261005-47];
- armor "plus" against "raise": the higher value wins [D-20261005-44];
- the 04:10 table's single-character follow-up against D-39's "paired when content exists": paired when a clip matches
  [D-20261005-43];
- where the parry's presentation lives: the weapon's defense set [D-20261005-48], with the two-character sequences in the
  weapon's paired set [D-20261005-57];
- "per player" guard-break counting against D-14: a per-character toggle, off for the player, on in the base profile
  [D-20261005-46, D-20261005-56];
- the counter keys and the finisher keys: direction and swing, and direction only [D-20261005-45, D-20261005-47];
- the turn rate: angle ÷ time left, finishing exactly at contact [D-20261005-50];
- "the enemy recovers": Staggered ends at once after a non-lethal follow-up, one follow-up per parry-counter [D-20261005-51];
- the guard-breaking hit deals no damage [D-20261005-52];
- hit-stun holds enemies too [D-20261005-53];
- the lethal-counter lookup: the lethal variant, then the finisher for that direction, then the non-lethal counter and the
  normal death [D-20261005-54];
- the Staggered reaction and recoil on the profile, guard enter and exit on the defense set [D-20261005-55];
- the finisher pool's home: the weapon's paired set, referenced by the combo set [D-20261005-57];
- knockback by outcome: none on a flinch, a block shove on a block, the full push on a guard break, normal on victims
  that are Staggered or in hit-stun, none in paired sequences [D-20261005-58, D-20261005-59, D-20261005-60];
- the four precedence layers, with the combo set's as the per-weapon layer [D-20261005-61];
- the player's freedom after a non-lethal counter, and the aimed-elsewhere press ending the opening [D-20261005-62];
- enemies choosing among their combo set's slots [D-20261005-63];
- the parry on timing alone, with room mattering only for paired content [D-20261005-64];
- the buffered Block press: guard only if still held, never a parry [D-20261005-66];
- the Staggered player: the reaction and the push, a dodge out after a minimum, 0.6 s by default [D-20261005-88, which
  supersedes D-20261005-67];
- where tunables live: reactions to hits on the profile, weapon behaviour on the weapon's sets [D-20261005-68];
- the block turn: the full angle, at a defense-set rate with a 360°/s default; the arc overridable on the defense set
  [D-20261005-69, D-20261005-70];
- the eight parry questions: what a miss is, no lockout after success, contact as the predicted arrival, the minimum for
  unauthored windows, the 720°/s cap, the eased slowdown, parries from hit-stun, and neutral assist defaults
  [D-20261005-71 to D-20261005-79];
- counter damage, when Staggered starts, what ×1.5 multiplies, the sliding guard-break window, the swing field and its
  auto-detect action, the follow-up reach, and "not on guard break" [D-20261005-80 to D-20261005-86];
- the dodge out of hit-stun and out of the player's Staggered state [D-20261005-87, D-20261005-88];
- the register's D-20261004-27 entry: its "evade becomes a universal escape" line has been removed;
- two locks on one character: both run, free when both end [D-20261006-01];
- the counter's end, the hit-stun hand-off and the attack's cancel point: one general cancel notify [D-20261006-02];
- where counter damage lives, and its order against content: the defense-set entry, a clip override optional; content,
  then damage, then the outcome [D-20261006-03];
- rules against content: rules before content, as a standing principle [D-20261006-04];
- when counter damage lands, and that it never clamps [D-20261006-05];
- the player's Staggered time against an attack's override: the player's own, always [D-20261006-06];
- how a lethal parry-counter is presented: every counter can kill, the enemy's outcome changes, a kill sequence is
  optional [D-20261007-07];
- protection inside the sequence, and the early press: untouchable until your own cancel notify; the press buffered and
  fired then [D-20261007-08, D-20261007-09];
- the aimed-elsewhere release: retired [D-20261007-10];
- the words parry, counter and parry-counter [D-20261007-11];
- targeting: free mode by the stick, else the facing; lock mode; in this spec as 3.13 [D-20261007-12 to D-20261007-14];
- the counted guard break built dormant; unblockable over guard breaker; the light press on a Staggered target; the
  lenient lockout [D-20261007-15 to D-20261007-18].

**Conflicts between active decisions:**
1. D-20261004-35's "a press aimed elsewhere … releasing the held enemy" became the Staggered-time rule by D-20261005-22
   and was confirmed by D-20261005-62. D-20261007-10 then retired the release itself: a press elsewhere leaves
   Staggered running. Resolved; kept here because the pillars page still describes the old window.
2. D-20261003-02's "phase resistance" against D-20261005-17's armor windows. The form answered at 03:38 EDT offered a
   state-based resistance, which the owner did not pick. Phase resistance is treated as replaced. The design pillars
   page still lists it.
3. D-20261004-27's per-type defense role against D-20261005-24's "no per-enemy-type modes" (Q17).
4. D-20260929-07 says to wire `FFinisherTriggerConfig`. D-20261005-18 removes its thresholds, so it is retired.
5. `CLAUDE.md`'s "Stateful Block input is immediate" against D-20261005-13's buffered Block press.
6. The design pillars page, now on `main`, predates the 10-05 decisions: the two modes, the window lead, phase
   resistance, the post-parry window, and block and parry reactions on the reaction profile.
7. D-20261005-43's register text says a follow-up clip matches "direction and body"; D-20261005-47, decided seven minutes
   later, rules out body types. This spec matches on direction only.
8. D-20261005-48's glossary entry gives the defense set "the non-lethal counter, the lethal paired variant";
   D-20261005-57 moves both into the paired set. The defense set keeps a reference, and the glossary entry is marked
   amended.
9. **D-20261005-88 supersedes D-20261005-67.** The Staggered player is no longer locked until the state ends: the
   reaction and the push play, and a dodge ends it after a minimum.

**Wording that leaves the rule open:**

10. How the predicted blade arrival is computed (D-20261005-73 names the reference, not the method); the computation in
   3.2 is proposed.
11. The ease shape of the attacker slowdown (D-20261005-76 fixes the kind of curve, not its numbers).
12. The default of "always perfect parry", which D-20261005-78 does not name; the spec sets it off.
13. The "Auto-detect swing" classification thresholds (D-20261005-84 names the action, not the thresholds); 4.5's are
    proposed.
14. "Charged" is not an attack type, and the latched charge is never written today (Q1).
15. The interplay of D-20261005-21, -35 and -36: answered for the parry-counter by D-20261007-08 (untouchable until
    your own cancel notify); the rest is read in 3.9 and in item 47 below.
16. "A short cooldown" in D-20261005-35 (Q6).
17. No decision names a clock (Q8).
18. D-20261005-59 names the block shove's per-type default but no value; the values in 3.10 are proposed.
19. D-20261005-63 stores AI weights per slot, which a shared combo set cannot vary per placed actor (Q15).

**Readings this spec chose, for the owner to see:**

20. D-20261005-24's "tough ones two" is narrowed by D-20261005-39 to "several exchanges".
21. The counter is read as a strike that drops guard under D-20261005-13.
22. A guard break's Staggered time comes from the breaking attack, and starts at the breaking contact (D-20261005-31).
23. The directional-light guard-breaker default applies to slots reached through a hold link.
24. A window cannot open before its attack starts. This is arithmetic, not a decision.
25. The limit of two attackers counts the attackers of the player; it is set in the combat settings.
26. The 0.12 s floor came from a proposed, unmerged hit-reaction spec. The owner adopted the number by picking
    D-20261005-34; the option's description (session transcript, 04:02 EDT) calls it "the 0.12 s floor decided earlier".
27. The glossary's back-to-opener entry uses a word that its own last table retires, for the parry-counter. This spec
    says "combo" for a run of linked attacks.
28. The owner's held knockback values (60 cm and 150 cm) come from a condensed summary. Check them on the asset.
29. An entry that names the parried attack's lane is preferred over one that does not, as the ordering of D-20261005-45's
    "optional refinement".
30. A follow-up strike's paired non-lethal clip is taken from the paired set's counters by direction, any swing, because a
    Staggered target has no incoming swing (D-20261005-43).
31. The effective parry window is max(0.35 s, authored × scale): the scale is applied first, then the floor, so that the
    window is "never shorter than 0.35 s" while "a designer setting scales every window" (D-20261005-09).
32. Only Light and Heavy presses start a follow-up strike or a paired finisher. Special and Evade resolve to no attack
    today, so they are not aimed strikes; a later special attack would join the rule.
33. The 350 cm finisher range (D-20261005-28) bounds the paired finisher; it is not applied to the aim-cone candidate set
    itself. A non-lethal follow-up strike reaches the same 350 cm [D-20261005-85].
34. Each counter entry's fallback strike is the one played when that entry's paired content is missing, and when no entry
    matches at all, the fallback strike of the entry reached by the fallback order is used. The glossary keys the fallback
    strike by direction and swing (D-20261005-48); the no-entry case is this spec's.
35. A lethal counter whose entry has no paired counter at all plays the fallback strike and the enemy dies its normal
    death, by the same logic as D-20261005-54's last step.
36. The combo set's finisher references are one per direction, and the paired set's counters carry a swing tag.
    D-20261005-57 fixes the homes and the keys (direction and role); these shapes are this spec's.
37. The block shove's per-attack and per-slot overrides, its scaling by charge and victim scale, and its direction follow
    the hit push; D-20261005-59 names only the per-type default.
38. D-20261005-88 says "the player's profile"; the spec reads it as the player's reaction profile, because the state is a
    reaction to a hit (D-20261005-68). How that time combines with an attack's Staggered-time override was open and is
    now decided: the player's own time, always [D-20261006-06].
39. D-20261005-88 does not restate what happens to attack and Block presses while the player is Staggered; the spec
    keeps D-20261005-67's "buffered, fires afterwards".
40. D-20261005-88's dodge minimum is "the same rule as hit-stun"; the spec measures it against the player's Staggered
    length, from the breaking contact.

**Readings of the 6 and 7 October decisions, for the owner to see:**

41. **The lenient lockout** [D-20261007-18]. The owner's words, verbatim: "im uncertain if i even want to punish parry
    attempts but i want to start with the parry lock in to see how it feels so the more lenient version sounds good to
    me". My reading: "the more lenient version" is the option in which a press made during the lockout is remembered
    and counts as a parry if the window is still open when the lockout ends, not the option that caps the lockout; the
    owner may veto. The lockout is kept as a tunable to be felt first and may go to zero.
42. D-20261007-07 says "where an entry carries an authored kill sequence". This spec places the kill sequence on the
    paired-set counter the entry references (4.7), since D-20261005-57 keeps two-character content in the paired set;
    the entry reaches it through its reference.
43. D-20261005-54's middle step (the finisher pool for a lethal counter without a lethal variant) is read as superseded
    by D-20261007-07: no separate kill clip is needed, so the pool is not consulted for a parry-counter, and a lethal
    press with no kill sequence kills at the counter's contact. The owner may veto.
44. D-20261005-81's "when the counter sequence ends" is read as the enemy's cancel notify on its half of the counter
    (D-20261006-02, D-20261007-07).
45. D-20261007-12's "nearest otherwise": among several Staggered or eligible enemies, the nearest wins. With that, the
    tie rule is settled and the former question on it is removed.
46. The stick direction (Free-1) is read in the frame movement uses, as the owner's "attack in the direction you are
    pressing" implies; no decision names the frame.
47. D-20261007-08 answers the former question on an in-progress swing during your own sequence, for the parry-counter.
    For a paired finisher today's rule stands: the character that started it cannot be touched (Contact-3). An
    in-progress swing's contact on an untouchable character is ignored and the swing continues to its end.
48. Lock-1 with no candidate does nothing and leaves free mode in place; the lock does not change what guard faces.
    D-20261007-13 says neither.
49. D-20261006-03's order "content first, then damage, then the variant" is read with the room check and the variety
    rule inside "content", so that when the fallback strike is chosen for lack of room, the damage read is the entry's
    value, which the fallback strike always deals.
50. D-20261007-17 names the light press only. The Heavy press on a Staggered target is Q19.
51. The cancel notify's shape (one class, no payload, one per montage, ignored on attacks with a warning) is proposed;
    D-20261006-02 fixes its meanings, not its class (4.13).
52. D-20261006-01's "no guard while Staggered, whatever the hit-stun allows" is read as Guard-2 taking precedence over
    Guard-4 while both states run.
53. An ordinary attack has no range in free mode beyond today's attack warp; D-20261007-12 names no range, and the
    350 cm of D-20261005-28 and D-20261005-85 bound the paired finisher and the follow-up strike only.
54. Lock mode's distances (how far a lock press looks, and "near" for the re-pick on death) and the flick threshold are
    not in D-20261007-13; the distances are Q20 and the threshold is a proposed tunable with no decided value.
