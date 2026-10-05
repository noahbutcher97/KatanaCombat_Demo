# Combat rules, data model and attack authoring: design

- **Date:** 2026-10-05.
- **Status:** draft for the owner's review, section by section. It authorizes no code change and no asset save.
- **Why this exists.** The owner's standing rule [D-20261004-45]: "i want to get more specific with the rules for guard
  breaks, finishers, parries, and counters, and what causes what and how and why and exact mechanics i dont want to find
  out how something is being calculated or arbitrated later once youve already generated code". The owner also chose one
  combined spec for the combo set, attack data, impact layering and the reaction profile, built in stages
  [D-20261005-04].
- **Baseline:** `origin/main` at `a7693bb4`. Open pull requests that touch these rules:
  - #136, one block rule (retires the legacy 70° cone);
  - #137, the rework of today's post-parry press window;
  - #138, the October findings, design pillars and decision log;
  - #139, attack timing read from the montage.

  Where this spec relies on an open pull request, it says so.
- **Sources.**
  - The decisions register, `docs/sessions/registers/decisions.md` (local and git-excluded), through D-20261005-40. Only
    active decisions are used, and "superseded by" is respected.
  - The owner's own answers, read from the session transcript, where the register condenses them.
  - The adopted combat glossary [D-20261005-40], reproduced as section 2.
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

### 1.1 Purpose

This document holds three things in one place:
- the exact combat rules;
- the data that carries them;
- the attack-authoring overhaul.

It puts rules first and code later. Implementation briefs carry the rule text from this document. Anything a rule needs
that this document does not state is an open question for the owner, not a choice for the implementer.

### 1.2 Design pillars

The pillars page is `docs/reference/DESIGN_PILLARS.md`
([link, on PR #138's branch](../../reference/DESIGN_PILLARS.md)). **It is not on `main` yet**, so the link resolves only
once #138 merges.

Parts of that page predate the decisions of 5 October and are superseded:
- the two parry modes by enemy type, now one model [D-20261005-24];
- the parry window lead, now an adaptive turn [D-20261005-05];
- resistance by attack phase, now armor [D-20261005-17];
- the separate press window after a parry, now the Staggered time [D-20261005-22].

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

1. **Health decides kills.** A paired finisher plays only when the strike's resolved damage is lethal. That is decided at
   the press, from damage after resistances [D-20261005-38, D-20261005-39, D-20261005-40].
2. **One Staggered state, one number.** A non-lethal parry and a guard break lead to the same state, with one default
   duration. The follow-up strike opportunity lasts exactly that long [D-20261005-08, D-20261005-22, D-20261005-31].
3. **Defaults with overrides.** Each value has a per-type default in the combat settings, an override on the attack and
   an override on the combo-set slot. Each character's reaction profile inherits from a base profile and overrides it
   [D-20261004-20, D-20261004-38, D-20261005-01].
4. **One place per question.**
   - What does this attack do? The attack.
   - How does this character react? Its reaction profile.
   - What are the global rules? The combat settings.
   - What follows what? The combo set.

   Sources: [D-20261004-20, D-20261005-01].
5. **Input is never lost.** Presses made during your own attack or during hit-stun are buffered
   [D-20261005-13, D-20261005-37, D-20261003-03].
6. **Feel is judged on rendered captures**, not on code or automation alone. This is project practice, recorded in the
   project memory and in [D-20261004-40].

---

## 2. Glossary

### 2.1 The adopted glossary

The owner adopted this glossary as written [D-20261005-40]. It is reproduced verbatim from
`.superpowers/design/combat-glossary.md`; only the heading levels are adjusted.

One meaning per term. Required in code identifiers, tooltips, logs, specs and docs. Retired words are rejected in review.

#### States
- **Staggered** — the open state. Caused by a non-lethal parry-counter or a guard break. Lasts the Staggered time
  (default 1.5 s, overridable per attack). While Staggered, a strike lands without reply.
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
- **Parry** — a block press inside the parry window. Always plays the deflect plus a counter in one sequence.
- **Counter** — the strike that is part of the parry. Never a separate press.
- **Follow-up strike** — the ×1.5 strike from an aimed attack press while the target is Staggered.
- **Lethal** — resolved damage (after resistances) ≥ the target's remaining health, decided at the press.
- **Paired finisher** — the synced two-character kill clip. Plays whenever a counter or follow-up strike is lethal.
  Never plays otherwise.
- **Parry window** — the input timing on the attacker's animation, never shorter than 0.35 s before contact, scaled by
  the parry-assist setting. Turning is never limited to it.
- **Parry lockout** — 0.25 s after a missed parry press during which no parry can trigger. Blocking still works.
- **Aim cone** — 60° either side of the camera direction; Staggered or eligible enemies win ties. Used for follow-up
  strikes and for choosing finisher targets.

#### Authoring
- **Combo set** — the asset holding slots, links and entry rules for one moveset, plus set-level type defaults.
- **Slot** — one placement of an attack in a combo set, with optional per-field overrides.
- **Attack** (`UAttackData`) — the move itself: montage, timing, damage, pressure, flags, impact feedback.
- **Reaction profile** — a character's hit and defense reactions, armor base, guard-break threshold and window, the
  "can be finished" / "can finish the player" markers, and parry assists.
- **Back-to-opener link** — the explicit link type that restarts a chain; the only sanctioned loop.

#### Retired words (do not use)
| Retired | Use instead |
|---|---|
| guard broken | Staggered |
| stunned | hit-stun |
| counter window | Staggered time |
| chain, bridge | parry (the sequence) / paired finisher |
| ending (alone) | paired finisher, or non-lethal counter |
| posture | — (no meter exists) |

**Requirement.** Code identifiers, tooltips and log text use these terms, and review rejects the retired words
[D-20261005-40].

**Where the glossary needs a decision.** Three entries collide with other active decisions. This spec does not choose
between them; each is a question in section 10.
- **Paired finisher**, "Never plays otherwise": this conflicts with finishing an enemy whose next ordinary attack would be
  lethal [D-20261005-18, D-20261005-19], and with enemies finishing the player [D-20261005-20] (Q28).
- **Reaction profile**, the "can be finished" marker: this conflicts with "no per-character exemptions" for a lethal
  parry [D-20261005-38] (Q31).
- **Armor**, "a per-character base … plus authored armor windows": this can be read as adding the window to the base or
  as the window setting a level (Q23).

### 2.2 Further terms used in this spec

These terms are not in the adopted glossary. They name parts of the design that the glossary does not cover. **Proposed
additions**, so that the product uses one word for each.

| Term | Meaning |
|---|---|
| **Attack type** | The attack's `EAttackType`: Light, Heavy or Special (`CombatTypes.h:48-54`). It decides whether a hit looks light or heavy, and it selects the per-type defaults. "Charged" is not an `EAttackType`; it has its own column in the default tables, and its exact definition is open (Q24). |
| **Directional light** | A light attack started by releasing a held Light with a direction: today's hold-release follow-up (`CombatComponent.cpp:5165`). The five shipped ones are Light-type assets (read from the assets' bytes: no saved `AttackType`, so the code default Light applies). |
| **Resolved damage** | The damage a strike would deal to a given target: base damage × weapon multiplier × any multiplier this spec adds (×1.5 for a follow-up strike) × the target's damage resistance. Today the first two factors are applied at `WeaponComponent.cpp:927` and the resistance at `HitReactionComponent.cpp:215`. A character dies when its health reaches 0 or below (`BaseCombatCharacter.cpp:277`). |
| **Contact** | The moment an attack would land. The parry window is measured back from it, and "lands first" compares it. Its exact definition is open (Q3). |
| **Effective parry window** | The authored parry window after the 0.35 s minimum and the parry-assist scale are applied (section 3.2). |
| **Missed parry** | A Block press that counts as a failed parry and so starts the parry lockout. Its exact definition is open (Q1). |
| **Deflect** | The defender's part of a parry that meets the incoming blade. |
| **Turn-deflect** | An authored 90° turn (for the sides) or 180° turn (for behind) that flows into a front counter pair. Motion warping hides the leftover angle [D-20261004-30]. |
| **Counter pool** | The combo set's directional counters, keyed by the attacker's direction and the parried attack's swing [D-20261005-24]. |
| **Counter entry** | One item in the counter pool: one deflect shared by a non-lethal counter and a paired finisher [D-20261005-38]. |
| **Non-lethal counter** | What a parry plays when the counter's damage is not lethal: a paired non-lethal animation, or the single-character strike when none fits. It leaves the enemy Staggered [D-20261005-23, D-20261005-24]. |
| **Single-character strike** | A strong cinematic strike played by the defender alone, together with the enemy's stagger reaction. It is the last fallback when no paired animation fits [D-20261005-23, D-20261005-30]. |
| **Finisher pool** | The paired finishers to choose from, by direction and body type. Where it lives is open (Q19). |
| **Finisher range** | 350 cm: the maximum distance at which a paired finisher can start [D-20261005-28]. |
| **Attack token** | An enemy's permission to attack the player. At most 2 are held at once (configurable), and each enemy has a short cooldown [D-20261005-35]. |
| **Others hold** | While the player performs a parry or a paired finisher, other enemies start no new attack. Swings already in progress finish, and the enemies resume when it ends [D-20261005-21]. Always written as "others hold", to keep it apart from holding a button. |
| **Lane** | A group of slots shown together in the moveset grid: light, heavy and directional [D-20261005-02]. |
| **Link** | What a slot does next for one input: Light, Heavy, or hold-and-release Light toward Forward, Back, Left or Right. A link points at a slot, back to the opener, or nowhere. |
| **Opener** | The slot a combo starts from when an input is pressed and no combo is running. Today: `DefaultLightAttack` and `DefaultHeavyAttack` on the attack configuration. |
| **Type default** | A value the combat settings hold for each attack type [D-20261004-20, D-20261004-38]. |
| **Set-level type default** | A per-type default held by a combo set. It is in the adopted glossary's "Combo set" entry [D-20261005-40]; its place in the precedence order is section 4.2. |
| **Attack override** | A field the attack asset sets for itself, replacing the default [D-20261004-20]. |
| **Slot override** | A field a slot sets, replacing the attack's value for that slot only [D-20261005-01]. |
| **Edit-through** | Editing the referenced attack's own fields inline from the combo set [D-20261005-01]. |
| **Make unique** | Copying an attack asset so that one slot can differ in fields a slot cannot override [D-20261005-01]. |
| **Base profile** | The reaction profile that character profiles inherit from and override [D-20261004-20]. |
| **Combat settings** | `UCombatSettings`: the global rules and the per-type defaults [D-20261003-08, D-20261004-20]. |
| **Resolved attack spec** | An attack's effective values after every layer, captured when the attack starts and carried on each of its hits (section 4.8). |
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

Q39 asks for one rule. Until it is answered, the rules give durations without naming a clock.

**Shared words.** These keep today's meanings:
- "Hostile" uses the team interface.
- "Alive" means not dead and not dying.
- "Aimed at" means the attack's intended target is this character. Enemy AI sets it when it attacks; player attacks set it
  through attack warping **[reported: `CombatComponent.cpp:4200-4204, 6060-6080`]**.

### 3.1 Guard and block

**Decisions:**
- D-20261005-11: ±75°, turning toward the blocked attack;
- D-20261005-12: no chip damage;
- D-20261005-13: starting an attack drops guard, and a press during an attack is buffered;
- D-20261005-14: the player's guard breaks only to guard breakers;
- D-20261005-37: guard can be raised during hit-stun;
- D-20261005-40: the glossary;
- D-20261004-21: a code-driven turn;
- D-20261004-23: block covers the front arc, and data is keyed by direction;
- D-20261004-24: one block rule, landing in PR #136.

**Raising guard.** Evaluated on each Block press, in order:

| # | Condition | If it fails |
|---|---|---|
| G1 | The character is alive, is not a paired victim, and is not playing a parry or a paired finisher [D-20261004-24; PR #136]. | The press is refused. |
| G2 | The character is not Staggered. | Enemies: refused, because a Staggered enemy is held (3.4). The player: Q20. |
| G3 | The character is not in its own attack. | The press is buffered. It takes effect when the attack reaches Recovery, or when the attack ends [D-20261005-13]. Whether Block must still be held then, and whether the buffered press can parry, is Q9. |
| G4 | Hit-stun does not refuse the press: guard can be raised during hit-stun [D-20261005-37]. | Whether that press can parry is Q10. |
| G5 | Guard rises. The same press is offered to the parry rule (3.2). | — |

**Guard ends** on any of these:
- Block is released or cancelled;
- death;
- becoming a paired victim;
- **starting an attack** [D-20261005-13];
- a guard break (3.5).

The counter that a parry plays is a strike. This spec reads it as an attack for this rule, so guard ends when the counter
starts (Appendix B). Guard comes back only through a new press, or through the buffer rule G3.

**Block at contact.** The defense resolver evaluates each weapon contact in today's order, with the arc changed
(`DefenseResolver.cpp:289-375`):

| # | Condition | Outcome |
|---|---|---|
| C1 | The contact is invalid, friendly, already consumed or stale. | Ignored (today). |
| C2 | The defender is invulnerable or in i-frames. | Ignored (today). |
| C3 | The defender started the paired animation now playing. | Ignored (today). |
| C4 | The attack carries `Attack.Property.Unblockable`. | A hit at full damage through guard; guard stays up (today, `:337-341`). |
| C5 | The defender is not guarding. | A hit (3.6). |
| C6 | \|yaw\| > 75°. | A hit. |
| C7 | Otherwise. | **Block.** |

**How yaw is measured (C6).** As today: the angle between the defender's facing and the line from the defender to the
attacker's location, at contact. It is compared with a strict greater-than (`DefenseResolver.cpp:354-361`;
`BaseCombatCharacter.cpp:568-570, 704-706`). Whether to keep this reference is Q38.

**What a block does:**
- **No damage on any attack**, and so no hit reaction, hit-stun or knockback [D-20261005-12]. Today the same holds
  (`BaseCombatCharacter.cpp:816-820`; knockback starts only from a played reaction, `HitReactionComponent.cpp:491-500`).
- **Guard break is evaluated** (3.5).
- **The defender turns toward the attack**, so that the block reads as coming from in front. The turn is code-driven and
  rate-limited [D-20261005-11, D-20261004-21]. The owner asked for a turn of "a bit"; the amount and the rate are Q38.
- **Presentation.** The block reaction for the incoming direction, from the defender's reaction profile. While the arc
  is ±75° only the front entry is used. The data is keyed by direction now, so that full-circle blocks later need only
  content plus one setting, the guard arc [D-20261004-23].
- **The attacker** recoils if the attack carries `Attack.Defense.BlockInterruptible`. Otherwise its swing continues but
  cannot touch this defender again (today, `DefenseResolver.cpp:366-374`).
- **The block uses one of the swing's hit slots** (today).

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Guard arc (half-angle) | 75° | combat settings | D-20261005-11 | 35°, `NormalBlockFinalTolerance` on the defense configuration |
| Block turn: amount | Q38 | reaction profile (proposed) | D-20261005-11 ("a bit") | at most min(70°, measured yaw), through motion warping |
| Block turn: rate | Q38 | reaction profile (proposed) | D-20261004-21 (rate-limited) | 180°/s |

**Changes from today:**
- The arc widens from 35° to 75°.
- Starting an attack drops guard. Today guard stays up through your own attack, and contacts within 35° are blocked
  mid-swing **[reported]**.
- A Block press during your own attack is buffered. Today it is refused and dropped (`CombatComponent.cpp:3317-3348`).
- The turn is a code-driven rotation, not a motion-warp window that each block clip must author.
- The legacy 70° rule is gone (PR #136).
- A guard can now break (3.5).

### 3.2 Parry

**Decisions:**
- D-20261005-05: the window is input timing only and forgiving, the turn adapts, and there is no earlier press for the
  rear;
- D-20261005-06 and D-20261005-32: a 0.25 s lockout after a missed press;
- D-20261005-07: every attack is parryable unless flagged;
- D-20261005-09: a 0.35 s minimum plus a scale;
- D-20261005-10 and D-20261005-33: the attacker is slowed to no less than 50% for at most 0.3 s;
- D-20261005-36: lands first;
- D-20261005-40: the glossary;
- D-20261004-06 and D-20261004-07: readability features and parry assists;
- D-20261004-23: a parry from any direction;
- D-20261004-30: turn-deflects.

**Input.** The Block press that raises guard (G5). A Block already held when a window opens does nothing; only a press
counts (today).

**Evaluation at a Block press, in order:**

| # | Condition | If it fails |
|---|---|---|
| P1 | The press passed G1–G4. Buffered presses are Q9; presses during hit-stun are Q10. | No parry. |
| P2 | The defender is not in the parry lockout. | No parry. Guard still rises: "blocking still works" [D-20261005-06]. Whether this press restarts the lockout is part of Q1. |
| P3 | At least one **candidate** exists. A candidate is an attack that: (a) belongs to a hostile, living character; (b) is aimed at this defender; (c) is in progress and not consumed; (d) is not flagged unparryable [D-20261005-07]; (e) has its effective parry window open now, including the input buffer [D-20261004-06; value Q8]; and (f) has not already touched this defender (today, `CombatComponent.cpp:3078-3086`). **The attacker's direction does not matter** [D-20261004-23]. | A Block press with no candidate raises guard only. Whether it is a missed parry, which starts the lockout, is Q1. |
| P4 | **Arbitration: lands first.** Of several candidates, the one whose contact comes first is parried [D-20261005-36]. Equal contact times fall back to today's lower tie-breaks: smaller angle, then shorter distance, then stable id (`DefenseResolver.cpp:119-146`). | — |
| P5 | **The parry succeeds.** | — |

**Gates removed from today.** These are removed because the window is input timing only and turning is never limited
[D-20261005-05], and because any direction can be parried [D-20261004-23]:
- the 70° hard cone;
- the reachability formula (tolerance plus turn budget × time left);
- angle allowances that widen with an early press.

Today's prediction-confidence and path tests reduce in practice to "window open and target set" **[reported]**, so (b)
and (e) cover them. That removal is a reading, listed in Appendix B.

**Effective parry window.**
- C is contact (Q3). The authored window is [S, C], with its end at contact (Q3).
- Authored length: L_a = C − S.
- Minimum: never shorter than **0.35 s** before contact [D-20261005-09].
- Scale: k, the defender's parry-assist window scale [D-20261005-09, D-20261005-40, D-20261004-07]. Its default is in Q8;
  1.0 means no change.
- **Effective length L.** How the floor and the scale combine is Q5:
  - either L = max(0.35 s, L_a × k), where the floor always holds;
  - or L = max(0.35 s, L_a) × k, where the scale can go below the floor.
- **Effective window:** [C − L, C]. The input buffer extends it before the opening, and the contact grace after C
  [D-20261004-06; values Q8].
- **A window cannot open before its attack starts.** An attack whose contact comes less than L after its start is
  parryable from its first frame. This follows from the arithmetic; it is not a decision (Appendix B).
- **Worked example.** The only shipped window is on `LightAttack_1`, 0.20–0.30 s, with Active starting at 0.30 s
  **[reported]**. With k = 1 and contact at 0.30 s, L = 0.35 s. The window then covers the attack from its first frame to
  0.30 s.

**On success, in order:**
1. **The parried attack is consumed** for every target, as today **[reported: `CombatComponent.cpp:1997-2069`]**.
2. **The outcome is decided now**, from the counter's resolved damage against the attacker's current health: a paired
   finisher or a non-lethal counter (3.3) [D-20261005-38, D-20261005-40].
3. **Others hold** (3.9) [D-20261005-21].
4. **The defender turns to face the attacker** [D-20261005-05]:
   - θ is the angle from the defender's facing to the attacker at the press. T is the time from the press to contact.
   - Rate: ω = θ ÷ T, so the turn finishes at contact. This is "the rotation rate adapts to the direction and the time
     left". ω is capped at the fastest natural turn, ω_max (Q6).
   - With turn-deflect clips [D-20261004-30], the clip's play rate adapts the same way, under the same cap (Q6). Until
     those clips exist, the code-driven turn covers off-front parries [D-20261004-23].
   - **When θ ÷ ω_max > T**, so that a natural turn has too little time, the attacker's animation is slowed
     [D-20261005-10]:
     - to a speed s of no less than 0.5, for a time d of no more than 0.3 s [D-20261005-33];
     - the attacker then advances s × d of its animation in d seconds, so contact arrives d × (1 − s) later;
     - the most a slowdown can buy is 0.3 × (1 − 0.5) = **0.15 s**;
     - how s and d are chosen within those bounds is Q7.
   - **If 0.15 s is still not enough**, the turn plays at ω_max and the parry still succeeds [D-20261005-33]. Motion
     warping inside the deflect hides what it can [D-20261004-30]; the rest is visible.
   - **The turn is never limited to the window** [D-20261005-05]. It may continue after the window closes, until
     contact.
5. **Readability** [D-20261004-06]:
   - the parry has its own contact feedback (hitstop, camera kick, sound and effect), distinct from a block;
   - its animation differs from the block reaction;
   - values are Q8.

   Today a parry uses the block's halved hitstop (**[reported]** `CombatComponent.cpp:184-190`), and on shipped data the
   same sound and effect as a block **[reported]**.
6. **Parry assists** are designer settings on the reaction profile, not debug switches [D-20261004-07]:
   - window scale;
   - input buffer;
   - contact grace;
   - slow motion while a window is open;
   - telegraph;
   - always perfect parry.

   Values are Q8.

**Failure paths:**
- **No candidate:** guard rises. If the press counts as a missed parry (Q1), it starts the 0.25 s lockout
  [D-20261005-06, D-20261005-32].
- **Locked out:** guard rises, with no parry.
- **An unparryable attack inside its window** is not a candidate [D-20261005-07]. The press is a guard. Whether it is a
  miss is Q1.
- **The attack touched first:** once an attack has touched the defender, blocked or hit, a later press cannot parry it
  (today).
- **Two attacks:** lands-first picks one. The other can be parried by a second press after the lockout (Q2), or blocked
  [D-20261005-36].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Minimum parry window | 0.35 s before contact | combat settings | D-20261005-09 | none; the one shipped window is 0.10 s |
| Window scale (parry assist) | Q8 | reaction profile, parry assists | D-20261005-09, D-20261005-40, D-20261004-07 | none |
| Parry lockout | 0.25 s | combat settings | D-20261005-06, D-20261005-32 | none: a failed press is a free guard |
| Attacker slowdown, lowest speed | 50% | combat settings | D-20261005-33 | none |
| Attacker slowdown, longest time | 0.3 s | combat settings | D-20261005-33 | none |
| Fastest natural turn ω_max | Q6 | Q6 | D-20261005-05, D-20261005-10 | guard facing turns at 180°/s |
| Input buffer, contact grace, slow motion on window, telegraph, always perfect parry | Q8 | reaction profile, parry assists | D-20261004-06, D-20261004-07 | none |
| Parry contact feedback | Q8 | the attack's impact feedback for the parry outcome (4.4) | D-20261004-06, D-20261004-20 | the block's feedback |
| Unparryable flag | off: every attack is parryable | the attack | D-20261005-07, D-20261005-40 | opt-in: only `LightAttack_1` is tagged parryable |

**Changes from today:**
- **Windows:** at least 0.35 s, where today the only one is 0.10 s.
- **Direction:** any direction can be parried, with no angle gate. Pressing early no longer widens the angle.
- **Lockout:** a missed press locks out further parries for 0.25 s.
- **Unparryable** is an opt-out flag. Today parryable is an opt-in tag.
- **Lands first** replaces today's ranking by credible intent, then deadline.
- **Turning:** an adaptive turn, plus a bounded attacker slowdown.

### 3.3 The parry is the counter

**Decisions:**
- D-20261005-24: the parry is the counter, with one fluid sequence;
- D-20261005-38: a lethal parry plays a paired finisher, chosen at the press, with no exemptions;
- D-20261005-39: the follow-up strike;
- D-20261005-22: the follow-up opportunity equals the Staggered time;
- D-20261005-23 and D-20261005-30: the single-character strike and the fallback order;
- D-20261005-08: one Staggered default;
- D-20261005-21: others hold;
- D-20261005-27: the aim cone;
- D-20261005-40: the glossary;
- D-20261004-30: turn-deflects;
- D-20261004-03: player free, attacker held;
- D-20261004-35: aim decides.

**A parry always plays the deflect and a counter, in one sequence.** The counter is never a separate press
[D-20261005-24, D-20261005-40].

**What each press does.** This is the table confirmed in the session at 04:10, with its wording for the non-lethal case
changed to the glossary's "non-lethal counter" [D-20261005-40]:

| Press | Not lethal | Lethal |
|---|---|---|
| Parry (press 1) | Deflect + counter strike (paired non-lethal counter, or single-character strike fallback per D-20261005-23); enemy Staggered | Deflect + paired finisher |
| Follow-up while Staggered (press 2) | ×1.5 single-character strike; enemy recovers | Paired finisher |

- **Choosing the finisher.** It comes from the finisher pool by direction and body type, with the fallback order of
  D-20261005-30. The session stated this at 04:10; the owner did not pick it as a separate question, so Q19 confirms it.
- **When it is decided.** Lethal or not is decided at the press, from resolved damage after resistances, so the right
  clip starts immediately [D-20261005-38, D-20261005-40].
- **Paired or single-character.** The owner-picked text of D-20261005-39 says the follow-up strike is "×1.5, paired when
  content exists". The table says single-character. Q16 resolves it.
- **How many presses.** "Weak enemies take one press, tough ones two" [D-20261005-24] is narrowed by D-20261005-39. A
  tough enemy takes several parry-and-strike exchanges, because health always decides.

**Press 1: the parry.** On success (3.2), in order:
1. **Direction.** Classify the attacker against the defender's facing at the press: front, left, right or behind
   [D-20261004-30]. This uses today's four-way classifier (`CombatMath.h:53-56`; the dominant axis wins, so each
   direction spans 90°). The sector boundaries are part of Q12.
2. **Swing.** The parried attack's swing. The proposal is its `NominalLane` and `SwingShape` (`AttackData.h:17-34`)
   (Q12).
3. **Counter damage.** The counter's resolved damage against the attacker. How much damage a counter deals is not
   decided (Q11). It is **lethal** if it is ≥ the attacker's current health [D-20261005-38, D-20261005-40].
4. **Content.** From the defender's combo set's counter pool [D-20261005-24], in the fallback order of D-20261005-30:
   1. an entry for this direction and this swing;
   2. an entry for this direction and any swing;
   3. an entry for any direction, warped to fit;
   4. the single-character strike, with the enemy's stagger reaction [D-20261005-23].

   The editor warns about missing coverage [D-20261005-30].
   - When several entries match at one step, the proposal reuses today's variety rule for reaction variants: one entry
     always plays; two alternate; three or more are random, excluding the last two
     (**[reported]** `HitReactionComponent.cpp:1292-1358`).
   - A lethal counter that falls through to the last step plays the single-character strike, and the enemy dies with its
     death reaction. That is a reading (Appendix B).
5. **The sequence.** The turn (3.2, "On success", item 4), then the deflect at contact, then:
   - **if lethal:** the entry's paired finisher. The attacker dies, with its death in the clip. Today's death-handling
     flag stays, so no separate death animation plays (**[reported]** `HitReactionComponent.cpp:1518-1533`);
   - **if not lethal:** the entry's non-lethal counter. Its damage applies, and the attacker becomes **Staggered** for the
     parried attack's Staggered time (3.4) [D-20261005-08, D-20261005-22]. When Staggered starts is Q14.
6. **After a non-lethal counter the player is free:** they can move, guard, attack and be hit, while the Staggered enemy
   stays held. This is D-20261004-03 carried into the new model, which is a reading (Appendix B).

**Hand-off timing.** Two things are measured in the rendered parry stage and reported with captures, not decided here:
- when the attacker leaves its own attack animation for its half of the counter entry;
- how the deflect lines up with contact.

Others hold throughout press 1's sequence [D-20261005-21].

**Press 2: the follow-up strike.** Evaluated when an attack press executes, in order:

| # | Condition | If it fails |
|---|---|---|
| F1 | The press is Light or Heavy. | Not a follow-up. |
| F2 | Target selection by the aim cone: within 60° either side of the camera direction, with a Staggered or eligible enemy winning ties [D-20261005-27, D-20261005-40]. What a tie means is Q32. The follow-up reach is Q17. | An ordinary attack. |
| F3 | The chosen target is Staggered. | An ordinary attack, and the paired-finisher rule (3.8) applies. |
| F4 | Damage: ×1.5 [D-20261005-39]. What the 1.5 multiplies is Q15. Lethal is decided at the press [D-20261005-38, D-20261005-39, D-20261005-40]. | — |
| F5 | **Lethal:** a paired finisher (Q19). **Not lethal:** the ×1.5 strike lands and the enemy recovers [D-20261005-39]. This spec reads "recovers" as: Staggered ends when the strike lands (Appendix B). How the strike is presented is Q16. | — |

- **A press aimed elsewhere** becomes an ordinary attack. It ends the Staggered state and releases the held enemy
  [D-20261004-35]. Q18 confirms that this still holds now that the opportunity is the Staggered time.
- **No press:** the enemy recovers when its Staggered time ends.
- **Which enemies.** The follow-up strike applies to any Staggered enemy, including one Staggered by a guard break. This
  follows D-20261005-39's wording, "on a Staggered enemy" (Q34).

**Content this needs:**
- **Counter entries.** For each direction (front, left, right, behind) and swing, one entry: one deflect shared by a
  paired non-lethal counter and a paired finisher [D-20261005-38].
- **Turn-deflects.** 90° left and right, and 180° behind, each flowing into a front pair [D-20261004-30]. The decision
  says the 90° turns are mirrorable, but mirroring is not possible in the project today: there is no mirror data table
  and no Mirror node **[reported, from the turn speed sweep]**.
- **The single-character strike**, and each enemy body's stagger reaction [D-20261005-23].
- **The finisher pool**, by direction and body type (Q19).

**What is owned today [reported]:**
- 8 counter-shaped two-character pairs, all starting with the attacker in front. No pack on the market has counter pairs
  where the attack comes from behind.
- 90° guarded turns: DSAS `Block_Turn_L90` and `R90` at 0.571 s, and DynamicKatana `Turn_Block_90` at 0.833 s.
- 180° turns of 0.867–1.5 s.
- A speed sweep found ×2.0 the best-looking rate for the DSAS left 90° and right 180° turns. It also found:
  - every DSAS turn starts with a pelvis jump of about 181 cm/s;
  - the DSAS right 90° turn spins its capsule about 150° in 4 frames.

  The sweep measured these clips; it chose no values.

**Changes from today:**
- **Presses.** Today a parry plays a two-character deflect animation, then waits up to 2.0 s for a press that plays a
  counter, then up to 2.0 s more for a press that plays the kill. Now there is one press, plus an optional follow-up.
- **Counter choice.** Today the counter is the defender's own default attack's counter data
  (`PairedAnimationComponent.cpp:4288-4321`), so a Heavy press can never counter **[reported]**. Now the counter comes
  from the parried direction and swing.
- **Lethality.** Today a counter never kills: it is capped at health − 1 unless a component flag is set **[reported]**.
  Now a lethal counter is a paired finisher.
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
- D-20261005-40: the glossary.

**Causes.** Only these two [D-20261005-40]:
1. a non-lethal parry (3.3);
2. a guard break (3.5).

**Duration.** The Staggered time of the attack that caused it:
- one default for every attack, **1.5 s**, in the combat settings;
- overridable per attack, and per slot (4.2) [D-20261005-08, D-20261005-31].

For a parry, the causing attack is the parried attack. For a guard break, it is the blocked attack that broke the guard;
that is a reading (Appendix B).

**Start.** For a parry, Q14. For a guard break, at the breaking contact; that is a reading.

**Clock.** Q39.

**While an enemy is Staggered:**
- it starts no attack and does not move: it is held [D-20261004-03]. Today the AI cannot start an attack while staggered
  (`EnemyCombatAIComponent.cpp:1140-1152`);
- a strike lands without reply [D-20261005-40];
- an aimed attack press on it is a follow-up strike (3.3) [D-20261005-39].

**Staggered ends:**
- when its time runs out;
- when a non-lethal follow-up strike lands, because the enemy recovers [D-20261005-39];
- when a press aimed elsewhere releases it [D-20261004-35] (Q18);
- on death.

**While the player is Staggered:** Q20. Enemy attacks on a Staggered player are ordinary attacks, because the follow-up
strike is the player's press [D-20261005-39]. Enemy paired finishers are covered in 3.8.

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
- D-20261005-40: the glossary.

There is no meter and no damage-derived stagger [D-20261005-15].

**Evaluated after a contact is decided as a block (C7), in order:**

| # | Condition | Result |
|---|---|---|
| B1 | The blocked attack's resolved guard-breaker flag is on. | **Guard break, immediately**, whoever the defender is [D-20261005-29]. |
| B2 | The defender is the player. | No guard break. The player's guard breaks only to guard breakers [D-20261005-14, D-20261005-40]. |
| B3 | The defender's profile counts blocked hits (Q22). The attack's resolved pressure is added to the guard-break count at the contact time t [D-20261005-25]. The count is the sum of blocked pressure within the guard-break window W that closes at t (Q21). | If the count ≥ N: **guard break** [D-20261005-26]. |

- **N** is 4 pressure points and **W** is 3 s by default. Both are set per character, through the base profile and
  per-character overrides [D-20261005-15, D-20261005-26].
- The decided option's own examples of a break within 3 s: four lights; two heavies; a heavy and two lights.

**Guard-breaker defaults** [D-20261005-29], overridable per attack and per slot:

| Attack | Guard breaker by default |
|---|---|
| Charged heavy | yes |
| Directional light | yes |
| Everything else | no |

"Directional light" is not an attack type. The proposal applies this default to attacks reached through a hold-and-release
link (Appendix B).

**On a guard break:**
- **The breaking hit is still a block**, so it deals no damage. This reads D-20261005-12 and the glossary's "negates all
  damage" (Appendix B).
- **Guard ends.**
- **The defender becomes Staggered** for the breaking attack's Staggered time [D-20261005-31].
- **The guard-break count resets** (Q21).
- **No enemy paired finisher** comes from it [D-20261005-20] (3.8, Q33).

**What does not count:**
- unblockable hits, hits outside the arc and hits while not guarding, none of which is a block;
- parries.

**Who it applies to.** Any character whose guard is up. Enemies do not guard today **[reported]**. Enemy guarding is not
designed, and guard-breaker enemy types come later [D-20261004-27].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Guard-break threshold N | 4 pressure points | reaction profile: base, with per-character overrides | D-20261005-26 | nothing breaks a guard |
| Guard-break window W | 3 s | reaction profile: base, with per-character overrides | D-20261005-26 | — |
| Counting on or off | Q22 | reaction profile | D-20261005-14, D-20261005-15 | — |
| Guard breaker | see the defaults table above | combat settings type defaults; attack and slot overrides | D-20261005-29 | — |

### 3.6 Interruption and hit look

**Decisions:**
- D-20261004-42: the attack type decides the look, and pressure decides interruption;
- D-20261005-16 and D-20261005-17: pressure against armor;
- D-20261005-40: the glossary.

**Scope.** Hits that land. Not blocks, not parries, and not damage applied by a paired animation.

**Look.** Decided by the attack type [D-20261004-42]:
- Light: a light reaction;
- Heavy, charged or not: a heavy reaction;
- Special: Q25.

The direction is classified as today: front, back, left or right of the victim
(**[reported]** `HitReactionComponent.cpp:645-655`).

**Interrupt rule** [D-20261005-17, D-20261005-40]:

```
interrupts  ⇔  P > A
P = the hit's resolved pressure:
    type default (light 1, heavy 2, charged 3) → set-level type default → attack override → slot override
A = the victim's current armor:
    base armor (reaction profile, default 0) combined with any open armor window on the victim's animation (Q23)
```

**Examples:**
- Base 0 and no window: every hit interrupts.
- A swing whose armor window is at 2, the decided option's own brute example: light (1) and heavy (2) flinch, and
  charged (3) interrupts. This holds if a window sets the armor level; under Q23's other reading it would be base plus
  window.

**An interrupting hit:**
- stops the victim's current action. An interrupted attack ends, as today (**[reported]**
  `CombatComponent.cpp:5810-5831`);
- plays the full directional reaction, with the look above;
- applies hit-stun (3.7);
- applies knockback (3.10).

**A hit that does not interrupt:**
- plays the additive flinch over the current action, which continues [D-20261004-42];
- applies its damage;
- applies **no** hit-stun, which comes only "when a hit interrupts" [D-20261005-34];
- knockback is Q27.

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
- D-20261005-18: hit-stun is no longer a finisher route.

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
- guard can be raised (3.1). Whether that press can parry is Q10;
- input is buffered and fires afterwards [D-20261005-37, D-20261003-03]. The buffer is today's last-input-wins queue.

**Who it applies to.** Every character. The decided option says "the victim can't act". For an enemy, this spec reads that
as: its AI starts no attack and does not move (Appendix B). Today hit-stun stops nothing: neither the player's input nor
the AI (`EnemyCombatAIComponent.cpp:1140-1152`).

**A second interrupting hit during hit-stun:** Q26. Today the new duration overwrites the old one, even when it is shorter
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

### 3.8 Paired finishers

**Decisions:**
- D-20261005-18, narrowed by D-20261005-39: eligibility;
- D-20261005-19: the trigger;
- D-20261005-20: enemy paired finishers;
- D-20261005-21: others hold;
- D-20261005-27: the aim cone;
- D-20261005-28: 350 cm;
- D-20261005-38 and D-20261005-39: lethal only;
- D-20261005-40: the glossary.

**Rule.** A paired finisher plays when a strike's resolved damage against its target is lethal. That is decided at the
press [D-20261005-38, D-20261005-39, D-20261005-40]. Staggered on its own does not grant one [D-20261005-39]. No health
percentage is involved [D-20261005-18].

**Which strikes can play one:**

| Strike | Status |
|---|---|
| The parry's counter (3.3) | Decided [D-20261005-38]. |
| A follow-up strike on a Staggered enemy (3.3) | Decided [D-20261005-39]. |
| Any other attack press, aimed at an enemy, whose resolved damage is lethal | D-20261005-18 ("when the attacker's next attack would deal lethal damage") and D-20261005-19 ("a normal attack press aimed at an eligible enemy") decide it. The adopted glossary says a paired finisher "plays whenever a counter or follow-up strike is lethal. Never plays otherwise" [D-20261005-40]. **These conflict; Q28.** |
| An enemy's attack on the player | Decided by D-20261005-20, with the same conflict (Q28). |

**The player's trigger.** Evaluated when an attack press executes, in order. When a buffered press is decided is Q30.

| # | Condition | If it fails |
|---|---|---|
| T1 | The press is Light or Heavy. | — |
| T2 | **Target, by the aim cone.** Candidates are hostile, alive, not dying and not in a paired animation, within **350 cm** [D-20261005-28] (measured between the two characters' locations, as today), and within **60° either side of the camera direction** [D-20261005-27]. A Staggered or eligible (lethal) enemy wins ties [D-20261005-27, D-20261005-40]. What a tie means is Q32. | No paired finisher; the attack plays normally. |
| T3 | **The strike.** If the target is Staggered, the follow-up strike at ×1.5 (3.3). Otherwise, the attack the press resolves to (subject to Q28). | — |
| T4 | **Lethal check:** resolved damage ≥ the target's current health. For a Heavy press, Q29. | The strike plays normally. |
| T5 | **Content and start checks.** Content comes from the finisher pool, by direction and body type, falling back in this order (Q19): the same direction and body type; the same body type in any direction, warped to fit; the single-character strike with the target's death reaction. Today's start checks stay: a clear path (a 30 cm sweep), and neither character already in a paired animation (**[reported]** `PairedAnimationComponent.cpp:3863-3998`). | When a start check fails, the strike plays normally. That is today's behaviour (3.11). |

**During a paired finisher:**
- others hold [D-20261005-21];
- the victim's death is in the clip.

**Enemy paired finishers on the player** [D-20261005-20]. All of these must hold:
- the enemy's reaction profile carries the "can finish the player" marker;
- the enemy's next attack would be lethal to the player;
- the finisher is never because of a guard break (Q33);
- the enemy is within 350 cm [D-20261005-28];
- Q28 allows it.

**The "can be finished" marker** is in the adopted glossary. What it does, given D-20261005-38's "no per-character
exemptions", is Q31.

**Removed from today:**
- the 25% low-health threshold (`HitReactionComponent.cpp:1423`);
- hit-stun and Staggered as routes on their own;
- choosing the one best soft-aim target in the character's facing direction, at 500 cm and 120°
  (**[reported]** `PairedAnimationComponent.cpp:2777-2799`). This becomes the camera aim cone, at 350 cm;
- finisher data on every attack, replaced by the finisher pool;
- every shipped enemy attack carrying lethal finisher data **[reported]**, replaced by marked enemy types only.

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Aim cone (half-angle) | 60° | combat settings | D-20261005-27 | soft aim from facing: 120° rejection, scored on 45° |
| Finisher range | 350 cm | combat settings | D-20261005-28 | 500 cm, `SoftAimRange` |
| Can finish the player | off | reaction profile | D-20261005-20, D-20261005-40 | every enemy attack can |
| Can be finished | Q31 | reaction profile | D-20261005-40 | — |

### 3.9 Several attackers

**Decisions:**
- D-20261005-35: up to 2 attackers;
- D-20261005-21: others hold;
- D-20261005-36: lands first;
- D-20261004-01: enemy coordination comes after the player's combat mode.

**Attack tokens:**
- At most **N = 2** enemies attack the player at once by default. N is configurable, in the combat settings
  [D-20261005-35]. Today N is 1, global and C++ only (`CombatTokenSubsystem.h:41`).
- Each enemy has **a short cooldown** between attacks [D-20261005-35]. Its value, and when it starts, are Q36. Today it is
  1.0 s, counted from the grant (`CombatTokenSubsystem.h:45`).
- Waiting enemies queue first in, first out, as today.

**Others hold** [D-20261005-21]:
- While the player performs a parry or a paired finisher, no other enemy starts a new attack.
- Swings already in progress finish.
- The enemies resume when it ends.
- Today every enemy targeting the player aborts its swing instead **[reported]**.
- Whether an in-progress swing can touch the player during the parry or the paired finisher is Q35.
- Whether other enemies hold during a non-lethal follow-up strike is Q37.

**Two parryable attacks** [D-20261005-36]. A parry press answers whichever lands first (3.2, P4). The other can be parried
by a second press after the lockout (Q2), or blocked.

**Not in this spec.** Wider enemy coordination, such as attack slots and orbits, comes after the player's combat mode
[D-20261004-01].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Most enemies attacking at once | 2 | combat settings | D-20261005-35 | 1, C++ only |
| Per-enemy cooldown | Q36 | combat settings | D-20261005-35 | 1.0 s from the grant |

### 3.10 Knockback: layering only

**Decisions:** D-20261004-38, D-20261001-07, D-20260930-02 and D-20260930-01.

- **Per-type defaults** live in the combat settings. Each attack overrides them field by field. The code values only seed
  a new settings asset [D-20261004-38]. A per-weapon layer was noted for later [D-20261004-38]; this spec reads the
  set-level type default (4.2) as that layer (Appendix B).
- **Slots** override per field [D-20261005-01].
- **Code seeds:** Light 25 cm in 0.2 s; Heavy 20 cm in 0.25 s (`CombatSettings.cpp:10-21`) [D-20261001-07]. The owner's
  held settings asset sets other values (7.6).
- **Scaling.** Distance scales with charge and with the victim's `KnockbackScale`
  (`KnockbackResolution.cpp:30-38`) [D-20260930-01].
- **When.** Today knockback applies only when a full reaction starts (`HitReactionComponent.cpp:491-500`). So interrupting
  hits push. Flinches are Q27.
- **Slopes** are verified by a rendered capture [D-20261004-40].

### 3.11 What stays as today

No decision touches these, so they keep working as they do today. Each one can change only through a later decision.
- **Unblockable attacks** hit through guard at full damage, and guard stays up (`DefenseResolver.cpp:337-341`).
- **Block-interruptible attacks** recoil when blocked (`DefenseResolver.cpp:366-374`).
- **A block uses one of the swing's hit slots**, and the swing cannot touch that defender again.
- **Guard facing while guard is held** keeps today's threat selection and automatic turn. Guard aim is "direction, with
  magnetism" [D-20261004-02], whose design is queued and not part of this spec.
- **Lands-first ties** fall back to today's lower tie-breaks (3.2, P4).
- **Super armor, invulnerability, and i-frames on reaction entries** are unchanged **[reported]**.
- **Damage resistance** is unchanged, and so are the rules for paired partners.
- **Being Staggered again** replaces the remaining time (`HitReactionComponent.cpp:1439-1466`).
- **Hitstop, sound and effect** are configured per attack, now inside impact feedback (4.4).
- **The death outcome** (ragdoll or frozen pose), and the flag that makes a paired finisher's clip the death animation,
  are unchanged **[reported]**.
- **Input buffering** uses the last-input-wins queue.
- **Direction classification** uses the four-way classifier.
- **The path check before a paired finisher** is a 30 cm sweep. When it fails, the strike plays normally.

### 3.12 Tunables at a glance

| Tunable | Default | Home | Per character? | Decision |
|---|---|---|---|---|
| Guard arc | ±75° | combat settings | no | D-20261005-11 |
| Block turn amount and rate | Q38 | reaction profile (proposed) | yes | D-20261005-11, D-20261004-21 |
| Minimum parry window | 0.35 s | combat settings | no | D-20261005-09 |
| Parry window scale | Q8 | reaction profile, parry assists | yes | D-20261005-09, D-20261005-40 |
| Parry lockout | 0.25 s | combat settings | no | D-20261005-32 |
| Attacker slowdown | ≥ 50% speed, ≤ 0.3 s | combat settings | no | D-20261005-33 |
| Fastest natural turn | Q6 | Q6 | Q6 | D-20261005-05, D-20261005-10 |
| Other parry assists (input buffer, contact grace, slow motion on window, telegraph, always perfect parry) | Q8 | reaction profile | yes | D-20261004-07 |
| Staggered time | 1.5 s (one default) | combat settings → attack → slot | per attack | D-20261005-08, D-20261005-31 |
| Follow-up strike multiplier | ×1.5 | combat settings (proposed) | no | D-20261005-39 |
| Guard-break threshold and window | 4 points within 3 s | reaction profile | yes | D-20261005-15, D-20261005-26 |
| Guard breaker | charged heavy and directional light: yes; others: no | combat settings → attack → slot | per attack | D-20261005-29 |
| Pressure | light 1, heavy 2, charged 3 | combat settings → set → attack → slot | per attack | D-20261005-17 |
| Base armor | 0 | reaction profile | yes | D-20261005-17 |
| Hit-stun | 0.25 / 0.45 / 0.6 s | combat settings → set → attack → slot | per attack | D-20261005-34 |
| Hit-stun floor | 0.12 s | combat settings | no | D-20261005-34 |
| Aim cone | 60° either side | combat settings | no | D-20261005-27 |
| Finisher range | 350 cm | combat settings | no | D-20261005-28 |
| Can finish the player | off | reaction profile | yes | D-20261005-20 |
| Can be finished | Q31 | reaction profile | yes | D-20261005-40 |
| Most attackers at once | 2 | combat settings | no | D-20261005-35 |
| Per-enemy attack cooldown | Q36 | combat settings | no | D-20261005-35 |
| Knockback | Light 25 cm / 0.2 s, Heavy 20 cm / 0.25 s (code seeds) | combat settings → set → attack → slot | per attack | D-20261004-38 |

---

## 4. Data model

### 4.1 One place per question

| Question | Where the answer lives | Decision |
|---|---|---|
| What follows what, from which input? | The combo set: slots, links, openers, entry rules | D-20261005-01, D-20261005-03, D-20261005-40 |
| What does this move do: montage, timing, damage, pressure, flags, impact feedback? | The attack (`UAttackData`) | D-20261004-20, D-20261005-40 |
| How does this slot differ from its attack? | The slot's overrides | D-20261005-01 |
| Which counter answers a parried attack? | The combo set's counter pool | D-20261005-24 |
| Which paired finisher plays? | Q19 | — |
| How does this character react, resist and defend? | Its reaction profile | D-20261004-20, D-20261005-40 |
| What are the global rules and the per-type defaults? | The combat settings | D-20261003-08, D-20261004-20 |

### 4.2 Precedence

Every layered field resolves on its own, field by field. A layer that sets the field wins over the layers above it.

1. **Combat settings type default**, per attack type. The tables have Light, Heavy, Charged and Special columns
   [D-20261004-20, D-20261004-38].
2. **Set-level type default**, the combo set's own per-type value. It is in the adopted glossary's "Combo set" entry
   [D-20261005-40], and this spec reads it as the per-weapon layer noted for later in D-20261004-38.
3. **Attack override**, on the attack asset [D-20261004-20].
4. **Slot override**, on the combo-set slot. **It wins** [D-20261005-01].

Notes:
- Each override is a toggle plus a value, the pattern `FKnockbackOverride` already uses (`CombatTypes.h:498-533`).
- **The Staggered time has one default**, not one per type [D-20261005-08]. Layer 1 holds that single value, and layers 3
  and 4 override it.
- **Heavy attacks carry values for both releases.** Where a type default distinguishes a charged release (pressure,
  hit-stun, guard breaker), a heavy attack's and a slot's override sets have one value for an uncharged release and one
  for a charged release. This is proposed (Q24 defines "charged").
- **Characters resolve the same way:** the base profile, then the character profile's overrides [D-20261004-20].
- **The editor always shows which layer an effective value came from** (5.3).

### 4.3 The combo set

**Proposed class:** `UComboSet`. **Proposed asset name:** `DA_ComboSet_<Moveset>`, for example `DA_ComboSet_Katana`.
It is named for its purpose, following the project's naming rule [D-20261004-17].

| Field | Holds | Decision |
|---|---|---|
| Slots | An array of slots (below). | D-20261005-01 |
| Openers | The slot each input starts from when no combo is running: a Light opener and a Heavy opener (today `DefaultLightAttack` and `DefaultHeavyAttack`). | glossary "entry rules" [D-20261005-40] |
| Entry rules | Further openers under conditions (running, airborne), added when those moves are designed. None ships now (7.2). | D-20261005-40 |
| Set-level type defaults | Per-type values for the layered fields (4.2). | D-20261005-40 |
| Counter pool | Counter entries (below). | D-20261005-24, D-20261005-38 |
| Turn-deflects | The 90° left, 90° right and 180° behind turn-deflects that flow into front entries. | D-20261004-30 |
| Single-character strike | The fallback strike, used when no paired entry fits. | D-20261005-23 |
| Finisher pool | Q19. | — |

**Slot** (proposed struct `FComboSlot`):

| Field | Holds |
|---|---|
| Id | A stable, hidden identifier. Links and the editor use it, so renaming or reordering never breaks a link. |
| Attack | A reference to an attack asset [D-20261005-01]. |
| Lane | Light, Heavy or Directional, for the grid [D-20261005-02]. |
| Links | One per input: Light, Heavy, and hold-and-release Light toward Forward, Back, Left or Right. Holding Heavy is the charge, so it has no directional links. |
| Overrides | Toggles and values for the slot-overridable fields (4.4) [D-20261005-01]. |

**Link** (proposed struct `FComboLink`) has one of three kinds:
- **To a slot:** the combo continues into that slot.
- **Back to opener:** the combo starts over at the opener for the link's own input. It is shown with a loop icon. It is
  the only allowed kind of cycle [D-20261005-03, D-20261005-40].
- **None:** the slot has no follow-up for that input. What a press then does is Q41.

**Validation.** The set implements `IsDataValid`, and every result also shows in place as a badge (5.5).

| Severity | Rule |
|---|---|
| Error | A cycle that does not pass through a back-to-opener link [D-20261005-03]. |
| Error | A link to a missing slot; a slot with no attack; a slot whose attack's montage section does not exist; two openers for one input. |
| Warning | A slot no link reaches and that is not an opener (unreachable). `LightAttack_11` is one today. |
| Warning | Counter coverage gaps, by direction and swing [D-20261005-30]. |
| Warning | Finisher coverage gaps (Q19). |
| Warning | An attack with no authored parry window (Q4). |
| Warning | An unparryable attack that has an authored parry window. |
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

**Counter entry** (proposed struct `FCounterEntry`):

| Field | Holds | Decision |
|---|---|---|
| Direction | Front, left, right or behind. | D-20261004-30 |
| Swing | The swing key, Q12. The proposal is the parried attack's lane and swing shape. | D-20261005-24 |
| Content | A paired-content reference (4.9) holding one deflect, shared by a non-lethal counter and a paired finisher. An off-front entry may instead name its turn-deflect and a front entry. | D-20261005-38, D-20261004-30 |
| Damage | Q11. | — |

### 4.4 The attack

The attack is "the move itself: montage, timing, damage, pressure, flags, impact feedback" [D-20261005-40].

**It keeps:**
- `AttackType`;
- the montage and section fields;
- `BaseDamage`;
- `MaxHitCount`;
- the editor timing-generation fields (with PR #139's action that reads timing from the montage, [D-20261004-33]);
- the hold and charge fields;
- `WarpConfig`;
- the defense profile: height, lane, swing shape, contact socket and bone;
- the attack tags `Attack.Property.Unblockable` and `Attack.Defense.BlockInterruptible`;
- the combo timing fields `ComboInputWindow` and the blend times.

**The impact block.** One block per attack holds every layered field, each a toggle plus a value
[D-20261003-08, confirmed by D-20261004-20]:

| Field | Default comes from | Can a slot override it? | Decision |
|---|---|---|---|
| Damage | `BaseDamage` (not layered by type) | yes | D-20261005-01 (option text: "knockback, damage, feedback") |
| Pressure (uncharged; charged for heavies) | type defaults | yes | D-20261005-17 |
| Hit-stun (uncharged; charged for heavies) | type defaults. It replaces `HitStunDuration`. | yes | D-20261005-34, D-20261004-41 |
| Staggered time | the one default | yes | D-20261005-08 |
| Guard breaker (uncharged; charged for heavies) | type defaults | yes | D-20261005-29 |
| Knockback | type defaults (`FKnockbackOverride`, which exists) | yes | D-20261004-38 |
| Impact feedback per outcome (hit, block, parry): sound, effect, hitstop, camera shake | type defaults | yes | D-20261004-20, D-20261004-06 |
| Unparryable | off | yes | D-20261005-07, D-20261005-40 |

"Unparryable" is a plain on/off flag. The project's semantics rule gives booleans "direct authored gates", and the
glossary calls it "an attack flagged".

**Deprecated, then removed after migration (7.2):**
- **The combo links** `NextComboAttack`, `HeavyComboAttack`, `DirectionalFollowUps` and `HeavyDirectionalFollowUps`
  (`AttackData.h:157-170`). They move to the combo set. `HeavyDirectionalFollowUps` is not read at runtime today; only
  validation and one check read it (`CombatComponent.cpp:3642-3643`).
- **`CounterData` and `FinisherData`** (`AttackData.h:322-331`). They move to the counter pool and the finisher pool.
- **Fields nothing reads:** `CounterDamageMultiplier` (`:149-150`), `bCanTriggerFinisher` (`:353-354`),
  `bHasCounterVariant` (`:360-361`), and the two deprecated meter-damage fields (`:145-146`, `:219-221`, named in 7.7).
- **The opt-in `Attack.Defense.Parryable` tag**, replaced by the unparryable flag [D-20261005-07].
- **`HitStunDuration`**, which becomes the hit-stun override.
- **The separate feedback fields** `ImpactAudioConfig`, `ImpactVFXConfig`, `HitstopConfig` and the defense profile's
  blocked-impact overrides. They fold into the impact feedback per outcome.
- **The `Attack.Capability.Terminal` tag**, replaced by links of kind "None". This is proposed, and part of Q42.

### 4.5 The reaction profile

**Proposed class:** `UReactionProfile`. **Proposed assets:** `DA_ReactionProfile_Base`, and one `DA_ReactionProfile_<Character>`
per character [D-20261004-20].

**Inheritance.** A profile names a parent. Each section has override toggles, and anything unset comes from the parent.
Parents may be nested, but never in a cycle. This is proposed.

**It holds** "a character's hit and defense reactions, armor base, guard-break threshold and window, the 'can be finished'
/ 'can finish the player' markers, and parry assists" [D-20261005-40]:

| Section | Contents | Decision |
|---|---|---|
| Hit reactions | Directional light and heavy reactions (front, back, left, right), each an array of variants. Death reactions, also an array. The additive flinch for each direction. I-frames on each entry, as today. **No stun field.** | D-20261002-07, D-20261004-41, D-20261004-42 |
| Defense reactions, keyed by direction | Block reactions for front, left, right and behind; only front is used while the arc is ±75°. The Staggered reaction, a character's "parried reaction", also used with the single-character strike. Recoil. Guard enter and exit. The parry presentation: Q13. | D-20261004-23, D-20261005-23, D-20260929-07 |
| Armor | Base armor, default 0. | D-20261005-17 |
| Guard break | Counting on or off (Q22). Threshold N, default 4. Window W, default 3 s. | D-20261005-15, D-20261005-26 |
| Markers | Can finish the player, default off. Can be finished (Q31). | D-20261005-20, D-20261005-40 |
| Parry assists | Window scale, input buffer, contact grace, slow motion on window, telegraph, always perfect parry (values Q8). | D-20261004-07, D-20261005-09 |
| Body type | Used to choose a paired finisher (Q19). Proposed. | — |
| Other per-character values | `KnockbackScale`, damage resistance, and the block turn (Q38). They move here; proposed. | D-20260930-01 |

**Where it is referenced.**
- The combat settings' `HitReactionSettings` and `DefenseConfiguration` slots (`CombatSettings.h:82, 86`) become one
  reaction-profile slot.
- The component's per-instance override stays.
- **A missing profile falls back to the base profile and logs once**, naming the owner and what is lost. This carries over
  the approved, never-started fix for an empty defense slot [D-20261002-10].

### 4.6 The combat settings

**Global rules** [D-20261003-08, D-20261004-20]:

| Rule | Value | Decision |
|---|---|---|
| Guard arc | ±75° | D-20261005-11 |
| Minimum parry window | 0.35 s before contact | D-20261005-09 |
| Parry lockout | 0.25 s | D-20261005-32 |
| Attacker slowdown | no lower than 50% speed, at most 0.3 s | D-20261005-33 |
| Staggered time default | 1.5 s | D-20261005-08 |
| Hit-stun floor | 0.12 s | D-20261005-34 |
| Follow-up strike multiplier | ×1.5 | D-20261005-39 |
| Aim cone | 60° either side of the camera direction | D-20261005-27 |
| Finisher range | 350 cm | D-20261005-28 |
| Most enemies attacking at once | 2 | D-20261005-35 |
| Per-enemy attack cooldown | Q36 | D-20261005-35 |
| Guard-facing threat selection | today's values, unchanged (3.11) | — |
| Fastest natural turn | if global, Q6 | — |

**Per-type defaults:**

| Field | Light | Heavy | Charged | Special | Decision |
|---|---|---|---|---|---|
| Pressure | 1 | 2 | 3 | Q25 | D-20261005-17 |
| Hit-stun | 0.25 s | 0.45 s | 0.6 s | Q25 | D-20261005-34 |
| Guard breaker | no (yes for directional lights) | no | yes | Q25 | D-20261005-29 |
| Knockback | 25 cm / 0.2 s (code seed) | 20 cm / 0.25 s (code seed) | the Heavy value, scaled by charge, as today | as today | D-20261004-38 |
| Impact feedback (hit, block, parry) | seeded from today's values; the parry values are Q8 | same | same | same | D-20261004-20, D-20261004-06 |

The block and parry defaults come from today's defense configuration: the sound `Impact_Sword_Parry_01_Cue` and the effect
`NS_GFXI_Yellow_Parry`, the same for both **[reported]**. They seed the block defaults. The parry's distinct feedback is
Q8.

### 4.7 Retiring the defense configuration

`UDefenseConfiguration` is removed [D-20261003-08, confirmed by D-20261004-20]. Its contents split three ways:
- **The defending character's capability** goes to the reaction profile: the turn rate and budget, and the block turn.
- **Global rules** go to the combat settings: threat selection and the slow-motion watchdog. The timers of today's
  post-parry stages are removed.
- **Presentation** goes to the reaction profile, keyed by direction: the block reactions, the parry presentation (Q13) and
  the attacker's response. The default sound and effect become impact-feedback type defaults.

The field-by-field map is in 7.4, where today's names can be given.

### 4.8 The resolved attack spec

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
  - knockback;
  - impact feedback per outcome;
  - the unparryable, unblockable and block-interruptible flags;
  - swing (lane and shape) and height.
- **Charge:** whether the release was charged (Q24), latched at release. Today a latched charge field exists on the hit
  record but nothing writes it (`CombatTypes.h:867-869`).
- **Before a press executes.** The lethal checks of 3.3 and 3.8 resolve the spec of the strike the press would start.

**Why a snapshot:**
- an edit or an override that changes mid-attack never changes a hit in flight;
- a test can build a spec without any assets.

### 4.9 The paired-content interface

This follows the plugin review's "narrow seam" idea (proposed).

**Purpose.** Counter entries and the finisher pool refer to two-character content through one narrow interface. Today's
`UPairedAnimationData` and a later Duet asset can then both sit behind it, and the choice of Duet stays open.

**Proposed shape:** an interface, `ICombatPairedContent`, that answers:
- its roles: who starts it and who receives it, with their montages and sections;
- its warp targets and warp limits;
- its sync moments: the deflect and the damage;
- its direction, swing and body-type tags;
- whether it is a paired finisher or a non-lethal counter;
- a quiet "can it start here?" check that returns refusal reasons. The editor and the "why didn't it play" debug readout
  use those reasons.

`UPairedAnimationData` implements it first.

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
- **Counter pool tab:** a direction × swing grid, coloured by coverage, with the fallback that each empty cell would use.
- **Finisher pool tab** (Q19).

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

### 5.4 The timing strip

Each slot has a read-only strip, derived from its montage's notifies. This is the analysis that PR #139's timing action
uses [D-20261004-33]. It shows:
- the Windup, Active and Recovery phases;
- the authored parry window **and** the effective window after the minimum and the scale, with the extension hatched;
- the hold start;
- the armor windows;
- the contact moment.

A warning appears when the animation has changed under its notifies.

### 5.5 Validation badges

- Each badge sits on the row or cell it concerns. It states the problem and the fix, and it navigates there in one click.
- The rules are those of 4.3.
- Loops through a back-to-opener link are shown, not flagged.

### 5.6 Who uses this set

- The set lists its referencers: weapons, characters and AI.
- Each attack lists the sets it appears in.
- A project-wide report lists attacks that no set references.

### 5.7 Pickers

- Slots, montage sections, tags and enum values are chosen from pickers, never typed as names.
- A picker always shows the current value, even an invalid one.
- An empty list shows a hint.

### 5.8 Labels

- **Every field's tooltip** says what the field does in play, which side reads it (attacker or victim), its units, and
  how it combines with its defaults.
- **Fields not yet wired** are hidden under Advanced.
- **Terms come from the glossary** [D-20261005-40], and there are no internal planning labels [D-20261004-17].

### 5.9 What this improves on

- **Over OnSight's three flat card lists**, and ComboCreator's design (which has no UI yet): one grid plus a map,
  per-field overrides that name their source, coverage grids for counters, and the timing strip inside the set view.
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

- the current combo set, from the weapon or the character;
- the current slot id;
- the combo state machine (`FAttackStateMachine`, `CombatTypes.h:1736`);
- the resolved attack spec of the running attack;
- the parry lockout timer;
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
1. The player is in hit-stun or Staggered: the press is buffered (3.7, Q20).
2. Pick the target by the aim cone (3.8, T2).
3. The target is Staggered: a follow-up strike. If it is lethal, a paired finisher (3.3).
4. Otherwise, the lethal check on the attack the press resolves to: a paired finisher (3.8, Q28).
5. Otherwise, select the transition, start the attack, and build its resolved attack spec.

**A Block press:**
1. The guard rules (3.1).
2. The parry rules (3.2).
3. The parry plays its counter (3.3).

**A weapon contact:**
1. Block resolution (3.1).
2. Guard break (3.5).
3. Interruption (3.6).
4. Hit-stun (3.7).
5. Knockback (3.10).

### 6.4 Components that change

| Component | What changes |
|---|---|
| **Combat** (`UCombatComponent`) | The combo set and current slot; the selection function; the resolved spec at attack start; guard dropping when an attack starts, and Block buffered until Recovery; the parry lockout; hit-stun holding attacks and movement; aim-cone targets for follow-up strikes and paired finishers (with Targeting). |
| **Targeting** | The camera-direction aim cone (60°, 350 cm, preferring Staggered or eligible enemies); the code-driven turn for block and parry (the adaptive rate and its cap), through the alignment arbiter. Motion warping stays for attacks and paired content [D-20261004-21]. |
| **HitReaction** | Pressure against armor, armor windows and the flinch; hit-stun from the resolved spec, with its floor; one Staggered state using the causing attack's time; the guard-break count; the reaction profile in place of the hit-reaction settings; death reading the variants array. |
| **PairedAnimation** | The parry sequence: the deflect, plus a non-lethal counter or a paired finisher, decided at the press; the counter pool and its fallback; the follow-up strike and the finisher pool; the paired-content interface. The post-parry timers of today go. |
| **Defense resolver** (`DefenseResolver.cpp`) | The guard arc at 75°; parry candidates (any direction, unparryable, effective window); lands first. The cone and reachability gates go. |
| **Enemy AI and attack tokens** (`EnemyCombatAIComponent`, `CombatTokenSubsystem`) | 2 attackers and a per-enemy cooldown; others hold instead of aborting; hit-stun and Staggered holding attacks and movement; enemy paired finishers only for marked types. |
| **Cinematic effects** (`CombatEffectsWorldSubsystem`) | The attacker slowdown, as an actor time lease (no lower than 50%, at most 0.3 s), on the existing overlap-safe lease system. |
| **Editor module** | The combo set editor: the grid, the map, overrides, the timing strip and validation; the migration operation (7.1). |

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

**Clean-ups, decided by the answer to Q42:**
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
  the only path (stage 9.9).
- **Slot references.** `UAttackConfiguration` is replaced by the combo set. The weapon's and the settings' slots
  (`WeaponData.h:185`, `CombatSettings.h:112`) point at a combo set instead. This is proposed.
- **Enemies:** Q40.

### 7.3 Attack fields

| Today | After |
|---|---|
| `HitStunDuration` | No shipped attack saves it **[reported]**, so there is nothing to move. It becomes the hit-stun override. |
| `Attack.Defense.Parryable` (only on `LightAttack_1`) | Removed: every attack is parryable unless flagged [D-20261005-07]. |
| `LightAttack_1`'s 0.10 s window | Unchanged in the asset. The 0.35 s minimum makes it effective. |
| Other attacks' parry windows | None exist **[reported]**: Q4. |
| `CounterData` | The counter pool takes `DA_Counter_GateA`, the only counter the shipped flow reaches. `Counter_LightAttack_1`–`11` are never reached and use placeholder editor sounds **[reported]**, so they are migrated only if the owner says so. |
| `FinisherData` (`DA_Finisher_GateA`, `Finisher_A`) | The finisher pool (Q19). |
| `ImpactAudioConfig`, `ImpactVFXConfig`, `HitstopConfig` | The impact feedback for the hit outcome. |
| `DefenseProfile` blocked-impact overrides | The impact feedback for the block outcome. |

### 7.4 Defense configuration and hit reactions into reaction profiles

- **`DA_DefenseConfiguration_GateA`** becomes `DA_ReactionProfile_Base`, plus the player's and the enemy's profiles.
- **`DA_DefenseConfiguration_GateB`** becomes the contact-matrix test level's profile.
- **`DA_HitReaction`** becomes the base profile's hit reactions:
  - the stun fields are dropped (all 0 on shipped data **[reported]**) [D-20261004-41];
  - `HeavyDamageHealthPercent` is dropped [D-20261004-42];
  - the single variant slots become arrays [D-20261002-07];
  - the stale saved `KnockbackForce` is dropped.

**Field by field, `UDefenseConfiguration` and its rows (section 4.7):**

| Today's field or group | What it does today | Where it goes | Why |
|---|---|---|---|
| `HardGuardConeHalfAngle` | Gates a parry at 70° | Removed | Any direction can be parried, and turning is never limited [D-20261004-23, D-20261005-05]. |
| `PerfectParryFinalTolerance` | Leftover angle allowed for a parry, 10° | Removed from the parry. Motion warping inside the deflect hides the leftover angle. | D-20261004-30 |
| `MaximumAutomaticTurn`, `DefenseTurnRate` | Guard-facing and block turn; parry reach; the turn budget for each paired step | Reaction profile: guard facing (as today) and the block turn (Q38). They no longer limit a parry. | D-20261005-05, D-20261004-21 |
| `NormalBlockFinalTolerance` | The 35° block gate | The combat settings' guard arc, 75° | D-20261005-11 |
| `CenterLaneHalfAngle`, `BoneHeightRows` | Lane and height classification for matching rows | Kept as global classification in the combat settings if direction or swing keys need them, otherwise removed. Proposed. | — |
| `ThreatLockMinSeconds`, `ThreatSwitchLeadSeconds`, `GuardedThreatRefreshSeconds`, `DefenseThreatRange`, `GuardManualOverrideThreshold`, `GuardAutoFacingResumeSeconds` | Guard facing | Combat settings, values unchanged | Unchanged (3.11). |
| `MaximumHighConfidencePredictionAge` | How fresh a prediction must be for a parry | Removed, with the prediction gate | A reading (Appendix B). |
| `InteractionTombstoneSeconds`, `TerminalInteractionCacheCap`, `NormalBlockTranslationDriftTolerance` | Nothing at runtime | Removed | Unread **[reported]**. |
| `GuardEnterMontage`, `GuardExitMontage` | Nothing; never played | Reaction profile, and wired | D-20260929-07 ("wire it") |
| `NoMontageParryBridgeSeconds`, `CounterWindowSeconds`, `FinisherReadySeconds` | Timers between the parry and later presses | Removed. There is no separate press window; the Staggered time replaces them. | D-20261005-24, D-20261005-22 |
| `ParryStaggerDuration` (the **attacker's** configuration) | Staggered length after a parry | The Staggered default in the combat settings, plus attack overrides | D-20261005-08 |
| `TimeDilationLeaseWatchdogSeconds` | Emergency restore for slow motion | Combat settings | Unchanged. |
| `NormalBlockTranslationAllowance`, `PerfectParryTranslationAllowancePerRole` | Warp translation caps | Block: removed, because the code-driven turn does not translate. Parry: the paired content's own warp limits (4.9). | D-20261004-21 |
| `DefaultBlockImpactAudio` and `VFX`, `DefaultParryImpactAudio` and `VFX` | Defense sound and effect defaults | Combat settings type defaults for the block and parry outcomes | D-20261004-20 |
| `DefenderPresentationRows` | Block and parry presentation, keyed by outcome, height, lane, swing and tags | Reaction profile: block reactions by direction. The parry presentation is Q13. | D-20261004-20, D-20261004-23 |
| `AttackerResponseRows` (`ParryStagger`, `Recoil`) | The attacker's reaction to being parried, and its recoil | Reaction profile: the Staggered reaction by direction, and recoil | D-20261004-20 |
| Row payload: `PairedBridgeData`, `ReviewedDeflectionMarker`, `bRequiresBridgePreflight` | Today's paired parry selection, and the dead re-pick | Removed. The counter entry carries the paired content. | D-20261004-22 |
| Row payload: `SourceSocketOverride`, `TargetBoneOverride` | Nothing | Removed | Unread **[reported]**. |
| Row payload: `Hitstop`, the audio and VFX overrides | Block and parry feedback | The attack's impact feedback per outcome | D-20261004-20 |
| Row payload: blend times, `bEnableRotationWarp`, `MaximumTranslation` | Montage blending and warping | Fields on the reaction-profile entries where still meaningful. The rotation warp is replaced by the code-driven turn. | D-20261004-21 |

### 7.5 Renaming the GateA and GateB assets

The decisions: [D-20261004-19, D-20261004-17]. Assets the redesign retires are removed rather than renamed. **The names
below are proposed** (Q43). They describe each asset, and they avoid the retired words [D-20261005-40].

| Today | Proposed |
|---|---|
| Folder `Data/PDA/Defense/GateA/` | `Data/PDA/Defense/Paired/` |
| Folder `Animation/Montages/Defense/GateA/` | `Animation/Montages/Defense/ParryAndBlock/` |
| Folder `Data/PDA/Defense/GateB/` (and `GateB/Attacks/`) | `Data/PDA/Defense/DefenseMatrix/` (and `DefenseMatrix/Attacks/`) |
| Folder `Animation/Montages/Defense/GateB/` | `Animation/Montages/Defense/DefenseMatrix/` |
| `DA_DefenseConfiguration_GateA` | removed: its contents go to `DA_ReactionProfile_Base` |
| `DA_DefenseConfiguration_GateB` | removed: its contents go to `DA_ReactionProfile_DefenseMatrix` |
| `DA_ParryBridge_GateA` | `DA_Paired_ParryDeflect_Front` |
| `DA_Counter_GateA` | `DA_Paired_Counter_Front` |
| `DA_Finisher_GateA` | `DA_Paired_Finisher_Front` |
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
| The GhostSamurai block, parry, broken, rebound and guard enter and exit montages (untracked) | The base profile's block reactions (front), parry presentation (Q13), Staggered reaction and guard enter and exit. They must be committed together with the profile that references them. |
| `ABP_SamuraiCharacter` and blend-space edits | Land with the inertialization fix below |
| `DA_HitReaction` with `HeavyDamageHealthPercent` = 1 | The field is removed [D-20261004-42] |
| The `DA_DefenseConfiguration_GateA` edit (GhostSamurai rows, guard enter and exit) | The base profile, as above |
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
  finisher-ready window. Retire them only after PR #137's fate is settled (Q44).
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
| Guard during your own attack | `…Guard.AttackStartDropsGuard`; `…Guard.BlockPressDuringAttackRaisesGuardAtRecovery` (Q9). | — |
| No chip damage | `…Guard.BlockDealsNoDamageOnEveryAttackType`. | — |
| Block turn | `…Guard.BlockTurnIsCodeDrivenAndRateLimited`: the fixture sets the rate. | The turn reads as natural (Q38). |
| Parry window | `…Parry.EffectiveWindowNeverBelowMinimum`; `…Parry.ScaleCombinesWithMinimum` (Q5); `…Parry.WindowNeverOpensBeforeAttackStarts`. | — |
| Parry lockout | `…Parry.MissedPressLocksOutParries` (Q1); `…Parry.BlockingWorksDuringLockout`. | — |
| Unparryable | `…Parry.UnparryableAttackIsNotACandidate`; `…Parry.AttacksAreParryableByDefault`. | — |
| Lands first | `…Parry.AnswersEarliestContact`; `…Parry.EqualContactFallsBackToAngle`. | — |
| Adaptive turn and slowdown | `…Parry.TurnRateIsAngleOverTimeLeft`; `…Parry.SlowdownNeverBelowFloorSpeedOrBeyondMaxTime`; `…Parry.SucceedsWhenTurnCannotFinish`. | Side and rear parries pressed late, midway and early, measured for foot slide, pelvis jump and yaw rate, as in the earlier prototype. |
| Parry outcome | `…Counter.LethalParryPlaysPairedFinisher`; `…Counter.NonLethalParryLeavesStaggered`; `…Counter.OutcomeFixedAtPress`, in which the health changes between the press and contact and the clip does not. | Front, side and rear parries, lethal and non-lethal. |
| Counter fallback | `…Counter.FallbackOrderDirectionSwingThenDirectionThenAnyThenStrike`. | — |
| Follow-up strike | `…FollowUp.AppliesMultiplier`; `…FollowUp.NonLethalEndsStaggered`; `…FollowUp.LethalPlaysPairedFinisher`; `…FollowUp.PressElsewhereReleases` (Q18). | The follow-up strike reads clearly. |
| Staggered | `…Staggered.OneStateOneDuration`; `…Staggered.CausingAttackOverrideWins`; `…Staggered.EnemyHeldWhileStaggered`. | — |
| Guard break | `…GuardBreak.GuardBreakerBreaksAtOnce`; `…GuardBreak.PressureCountBreaksWithinWindow` (the fixture sets N and W); `…GuardBreak.PlayerOnlyBreaksToGuardBreakers`; `…GuardBreak.BreakingHitDealsNoDamage`. | The guard-break reaction reads clearly. |
| Interruption | `…Interrupt.OnlyWhenPressureExceedsArmor`, using strict inequality; `…Interrupt.ArmorWindowRaisesArmor` (Q23); `…Interrupt.FlinchKeepsCurrentAction`. | A flinch and an interrupt are told apart at a glance. |
| Hit-stun | `…HitStun.NeverBelowFloor`; `…HitStun.ActionsWaitGuardAllowed`; `…HitStun.InputBufferedFiresAfter`. | Hit-stun lengths read well. |
| Paired finishers | `…Finisher.OnlyWhenLethal`; `…Finisher.NoHealthPercentRoute`; `…Finisher.StaggeredAloneDoesNotFinish`; `…Finisher.AimConeAndRange`; `…Finisher.EnemyOnlyWhenMarked`; `…Finisher.NeverBecauseOfGuardBreak` (Q33). | Alignment from up to 350 cm. |
| Several attackers | `…Attackers.AtMostConfiguredCount`; `…Attackers.CooldownPerEnemy`; `…Attackers.OthersHoldDuringParryAndPairedFinisher`; `…Attackers.InProgressSwingsFinish`. | A one-against-four crowd fight. |
| Precedence | `…Data.SlotOverrideWins`; `…Data.AttackOverrideBeatsSetDefault`; `…Data.SetDefaultBeatsSettings`; `…Data.ResolvedSpecUnchangedByMidAttackEdit`. | — |
| Selection function | `…ComboSet.ReturnsChosenAndRejectionReasons`; `…ComboSet.BackToOpenerIsTheOnlyLegalCycle`. | — |
| Migration | `…Migration.ComboSetReproducesSourceGraph`, run on a fixture graph; `…Migration.NeverOverwritesDesignerEdits`; `…Migration.RenamesLeaveRedirectors`. | — |
| Shipped content | `…Content.ShippedSettingsFiniteAndInClamp`; `…Content.ShippedComboSetsValidate`; `…Content.CounterCoverageWarnedNotFailed`. | — |
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

**The order below is proposed** (Q46). Each stage lists the decisions it implements and the questions it needs answered
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
- **Needs:** Q41, Q42; PR #139 merged.

### 9.2 The moveset grid

- **What:**
  - the grid and the node map;
  - the timing strip, the badges and "who uses this set";
  - pickers, edit-through and make unique;
  - inline overrides for the fields that are already layered (knockback, damage).
- **Ships:** the owner's chosen editor.
- **Decisions:** D-20261005-02.
- **Needs:** approval of section 5.

### 9.3 Layered attack data and reaction profiles

- **What:**
  - the impact block on attacks, as data, with set-level defaults and slot overrides;
  - the resolved attack spec carried on hits;
  - `UReactionProfile` with a base profile and inheritance;
  - retiring `UDefenseConfiguration`;
  - the owner's held content;
  - the asset renames;
  - the charge-loop snap fix.
- **Ships:** the same rules, with one place per question and the owner's tuning live.
- **Decisions:** D-20261004-20, D-20261004-22 (as Q13 settles it), D-20261004-32, D-20261004-38, D-20261004-41 (data),
  D-20261004-19, D-20261002-07.
- **Needs:** Q13, Q24, Q40, Q43.

### 9.4 Hit rules

- **What:**
  - pressure against armor, with armor windows and the flinch;
  - hit-stun with its floor, holding attacks and movement while input buffers;
  - one Staggered state, still caused only by a parry at this stage;
  - the damage ÷ health rule removed.
- **Decisions:** D-20261004-42, D-20261005-17, D-20261005-34, D-20261005-37.
- **Needs:** Q23, Q25, Q26, Q27, Q39.

### 9.5 Guard rules

- **What:**
  - the ±75° arc and the code-driven block turn;
  - guard dropping when an attack starts, and Block buffered until Recovery;
  - guard break, by count and by guard breakers.
- **Decisions:** D-20261005-11 to D-20261005-15, D-20261005-25, D-20261005-26, D-20261005-29, D-20261005-31,
  D-20261004-21.
- **Needs:** Q9, Q20, Q21, Q22, Q38; PR #136 merged.

### 9.6 Parry rules

- **What:**
  - the effective window, with its minimum and scale;
  - the lockout and the unparryable flag;
  - lands first;
  - the adaptive turn and the attacker slowdown;
  - parry feedback and parry assists.
- **Meanwhile:** after a successful parry, today's post-parry sequence still plays.
- **Decisions:** D-20261005-05 to D-20261005-07, D-20261005-09, D-20261005-10, D-20261005-32, D-20261005-33,
  D-20261005-36, D-20261004-06, D-20261004-07.
- **Needs:** Q1–Q8, Q10, Q39.

### 9.7 The parry is the counter, and paired finishers by lethal damage

- **What:**
  - the counter pool and its fallback;
  - the decision at the press: a paired finisher or a non-lethal counter;
  - the follow-up strike;
  - paired finishers only on lethal damage, with the aim cone and 350 cm;
  - the finisher pool;
  - enemy paired finishers for marked types only;
  - today's post-parry stages and the 25% threshold removed.
- **Decisions:** D-20261005-18 to D-20261005-24, D-20261005-27, D-20261005-28, D-20261005-30, D-20261005-38,
  D-20261005-39.
- **Needs:** Q11–Q19, Q28–Q34, Q44; counter content (3.3).

### 9.8 Several attackers

- **What:** two attack tokens, a per-enemy cooldown, and others hold instead of aborting.
- **Decisions:** D-20261005-21, D-20261005-35, D-20261005-36.
- **Needs:** Q35, Q36, Q37.

### 9.9 Removing the deprecated fields

- **What:**
  - the old link fields, `CounterData` and `FinisherData`, and the unread fields;
  - the remaining identifiers that use retired words.
- **Decisions:** D-20261005-40.

---

## 10. Open questions for the owner

Each question is an A–D form, and the recommended option is marked. The recommendations are this spec's. **None is a
decision until the owner picks it.**

### Parry

**Q1. What counts as a missed parry, which starts the 0.25 s lockout?** Block is both guard and parry, so as written every
guard raise could count [D-20261005-06].
- A. A Block press while a parryable attack aimed at you is in progress but outside its window, too early or too late.
  Raising guard when nothing parryable is coming never locks you out, and a press during the lockout does not extend it.
  **(Recommended.)**
- B. Any Block press that does not parry.
- C. As A, plus presses against unparryable attacks.
- D. Only a second press made within the lockout of the first.

**Q2. Is there also a lockout after a successful parry?** D-20261005-36 says the other attack "can be parried by a second
press after the lockout".
- A. No: only a missed press locks out, and a second parry can follow at once.
- B. Yes: the same 0.25 s follows every parry press, successful or not. **(Recommended: it is the text the owner
  picked.)**
- C. A separate, shorter setting after a success.
- D. No second parry while your own parry plays (see Q35).

**Q3. What is "contact", the moment the parry window is measured back from?**
- A. The end of the authored parry window, which authors place at the strike. Today's prediction already treats it as
  contact. **(Recommended.)**
- B. The start of the attack's Active phase.
- C. The first weapon contact, predicted from the swing.
- D. A separate authored contact marker.

**Q4. What about attacks with no authored parry window?** Every attack is parryable [D-20261005-07], and windows are
authored on the attacker's animation [D-20261005-09].
- A. The game gives them the minimum window: 0.35 s, closing at Active start.
- B. A validation error until a window is authored.
- C. They are not parryable until a window is authored.
- D. A at runtime, plus a validation warning so that designers see it. **(Recommended.)**

**Q5. How do the 0.35 s minimum and the parry-assist scale combine?**
- A. L = max(0.35 s, authored × scale): the minimum always holds. **(Recommended: "never shorter than 0.35 s".)**
- B. L = max(0.35 s, authored) × scale: a scale below 1 can go under 0.35 s.
- C. The scale applies only to windows longer than 0.35 s.
- D. The scale adds a fixed time instead of multiplying.

**Q6. What is the fastest natural turn, the cap on the adaptive parry turn?**
- A. A per-character maximum turn rate for the code-driven turn and maximum play rate for turn-deflect clips, in the
  reaction profile, chosen from a rendered sweep. **(Recommended.)** The earlier sweep found ×2.0 the best-looking rate
  for the owned DSAS turns; it measured, it did not decide.
- B. One global rate, in the combat settings.
- C. Separate caps for the sides and for behind.
- D. Each clip's authored speed times one global factor.

**Q7. Within its bounds (no lower than 50% speed, at most 0.3 s), how is the attacker slowdown shaped?**
- A. As light as possible, spread over the full 0.3 s: speed = 1 − shortfall ÷ 0.3, never below 0.5. **(Recommended:
  the least visible.)**
- B. As short as possible, at 50%: time = shortfall ÷ 0.5.
- C. Ease down and back up within 0.3 s.
- D. Always 50% for 0.3 s whenever there is any shortfall.

**Q8. How are the parry readability and assist values set?** These are the parry hitstop and camera kick, its sound and
effect, the input buffer, the contact grace, slow motion while a window is open, the telegraph, always perfect parry, and
the window scale.
- A. Chosen from a rendered sweep in the parry stage. Until then the assists start neutral (scale 1, buffer and grace 0,
  slow motion off, always perfect parry off), and the parry uses the block's feedback. **(Recommended.)**
- B. Start from the proposal of 4 October: a 0.12–0.15 s parry hitstop and a camera kick, with buffer and grace at 0
  until tuned.
- C. Match a reference game's values.
- D. Leave each off, or at 0, unless a designer sets it.

**Q9. A Block press during your own attack is buffered until Recovery [D-20261005-13]. What happens then?**
- A. Guard rises at Recovery only if Block is still held, and the buffered press does not parry. **(Recommended.)**
- B. Guard rises if Block is still held, and the buffered press counts as a parry press at that moment.
- C. Guard rises briefly even if Block was released.
- D. Guard rises only when the attack ends.

**Q10. Can a Block press made during hit-stun parry?**
- A. Yes: it raises guard and is evaluated as a parry. **(Recommended: one rule for every Block press.)**
- B. No: guard only. Parries wait until hit-stun ends.
- C. Only after half the hit-stun has passed.
- D. Yes, but a press with no open window always counts as a missed parry.

### The parry is the counter

**Q11. How much damage does the counter deal, and so when is a parry lethal?** This was never decided. The "grunt counter
damage" question was answered with a question, and today's `CounterDamageMultiplier` (1.5) is never read.
- A. Each counter entry's own damage, with a combat-settings default, like today's paired data. **(Recommended: what a
  counter does lives with the counter.)**
- B. The defender's light opener's damage times a counter multiplier.
- C. The parried attack's damage times a multiplier.
- D. ×1.5 of the defender's light opener, matching the follow-up strike.

**Q12. How are counter entries keyed?**
- A. Direction by today's four-way classifier, each direction spanning 90°. Swing by the parried attack's lane (left,
  centre, right) and swing shape (horizontal, vertical, thrust, sweep, grab). **(Recommended: these fields exist, and the
  plugin review found that lane and swing are needed to tell left from right.)**
- B. Direction plus swing shape.
- C. Direction plus lane.
- D. Designer-set direction sectors plus swing shape.

**Q13. Where does the parry's presentation live?** D-20261004-22 put "each parry entry", with its optional two-character
animation and its solo fallback, in the reaction profile. D-20261005-24 chooses the parry's counter from the combo set.
- A. The counter entry holds the paired parts and their single-character fallback, so D-20261004-22's explicit fallback
  lives there. The reaction profile holds only the receiving character's reactions: Staggered and death.
  **(Recommended.)**
- B. The reaction profile holds parry entries by direction, as D-20261004-22 says, and the combo set holds only the
  counter.
- C. Both: the profile holds the defender's solo deflect by direction, and the counter pool holds the counters.
- D. A shared library asset that both refer to.

**Q14. When does the Staggered time start after a non-lethal parry?**
- A. When the non-lethal counter finishes, so that the whole time is available for the follow-up. **(Recommended.)**
- B. At the parry press.
- C. At contact, when the deflect lands.
- D. When the counter's damage lands.

**Q15. What does the follow-up strike's ×1.5 multiply?**
- A. The resolved damage of the attack the press resolves to, the slot's attack. **(Recommended: it uses existing data,
  and a Heavy press hits harder.)**
- B. The defender's light opener's damage, whatever was pressed.
- C. A dedicated follow-up damage in the combo set.
- D. The counter's damage.

**Q16. Is the non-lethal follow-up strike paired?** The owner-picked text of D-20261005-39 says "×1.5, paired when
content exists". The table confirmed at 04:10 says "×1.5 single-character strike".
- A. Always the single-character strike, as the table says.
- B. A paired non-lethal strike when content exists, otherwise the single-character strike, as D-20261005-39's text says.
  **(Recommended: it is the text the owner picked.)**
- C. Always paired, falling back to the strike.
- D. The slot's own attack animation, at ×1.5.

**Q17. How far can a non-lethal follow-up strike reach?** 350 cm is decided for paired finishers only.
- A. 350 cm, closing the gap the way a paired finisher does, so that both results of one press reach equally far.
  **(Recommended.)**
- B. The attack's own reach.
- C. 500 cm, today's soft-aim range.
- D. Authored per strike.

**Q18. Does a press aimed elsewhere still end the Staggered state?** D-20261004-35, as the owner picked it: it "ends the
window and becomes a normal attack, releasing the held enemy". Since D-20261005-22, that window is the Staggered time.
- A. Yes: an attack aimed elsewhere ends that enemy's Staggered state. **(Recommended: it carries the owner's pick
  over.)**
- B. No: the enemy stays Staggered until its time ends.
- C. Only if the other attack lands.
- D. It pauses the Staggered time while the other attack plays.

**Q19. Where is the finisher pool, and how is a paired finisher chosen?** At 04:10 the session stated "by direction and
body type", with the counter fallback order of D-20261005-30; the owner did not pick it as a separate question.
- A. In the attacker's combo set, keyed by direction (relative to the victim) and the victim's body type (a marker on its
  reaction profile). The fallback is: the same direction and body type; then the same body type, any direction, warped
  to fit; then the single-character strike with the death reaction. **(Recommended.)**
- B. On the victim's reaction profile, because the body owns the finishers that fit it.
- C. A global finisher library in the combat settings.
- D. On each attack, as today.

### Staggered and guard break

**Q20. What can the player do while Staggered after a guard break?**
- A. Nothing but wait: no attack, movement or guard, and input buffers and fires afterwards. **(Recommended: it matches
  enemies being held.)**
- B. As in hit-stun: attacks and movement wait, and guard can be raised.
- C. Move only.
- D. Everything except guard.

**Q21. How does the guard-break count measure "within 3 s", and when does it reset?**
- A. A sliding window: the sum of the pressure blocked in the last 3 s. It resets to 0 on a guard break.
  **(Recommended.)**
- B. A fixed window that starts at the first blocked hit and resets 3 s later.
- C. As A, and it also resets when guard is lowered.
- D. Points drain steadily instead of expiring.

**Q22. Which characters count blocked hits?** D-20261005-14 says the player's guard breaks only to guard breakers.
D-20261005-15 counts per character; the owner's words were "per player".
- A. A reaction-profile setting: on in the base profile, off in the player's profile. **(Recommended: it matches
  D-20261005-14.)**
- B. Off in the base profile, and enabled per enemy type later.
- C. On for everyone, the player included, which contradicts D-20261005-14.
- D. Only characters marked as guarders.

### Hits

**Q23. How do armor windows combine with base armor?** D-20261005-17 says windows "raise it". The glossary says "a
per-character base … plus authored armor windows".
- A. A window sets a level. Current armor is the highest of the base and any open window. **(Recommended: it matches
  D-20261005-17's example, a brute's swing at 2.)**
- B. A window adds to the base.
- C. A window replaces the base, even when lower.
- D. Overlapping windows add to each other and to the base.

**Q24. What makes a heavy attack "charged"?**
- A. Its hold check found Heavy still held, so it entered its charge loop. This is today's yes-or-no charge state.
  **(Recommended.)**
- B. Only a full charge, when `MaxChargeTime` is reached.
- C. A charge level above a threshold.
- D. A separate attack asset marked as charged.

**Q25. What defaults do Special-type attacks use?** No Special attack ships, and today Special looks Heavy.
- A. The Heavy column, until special attacks are designed. **(Recommended: it matches today's look.)**
- B. Explicit values required, with a validation error without them.
- C. The Light column.
- D. Their own column, with values to choose.

**Q26. A second interrupting hit lands during hit-stun. What happens to the hit-stun?** Today the new duration replaces
the old one, even when it is shorter.
- A. Restart with the new hit's duration, as today.
- B. Keep the longer of the time left and the new duration. **(Recommended: a light hit cannot cut a heavy's hit-stun
  short.)**
- C. Add them.
- D. Ignore new hits until it ends.

**Q27. Does a flinch push?**
- A. No: only interrupting hits push, which is today's rule because only a full reaction starts knockback.
  **(Recommended.)**
- B. Yes, with the full knockback.
- C. With a reduced push, set in the settings.
- D. Set per attack.

### Paired finishers

**Q28. Can any lethal attack play a paired finisher, or only counters and follow-up strikes?** The adopted glossary says
a paired finisher "plays whenever a counter or follow-up strike is lethal. Never plays otherwise". D-20261005-18 and
D-20261005-19 let an aimed attack press finish an enemy whose next attack would be lethal, and D-20261005-20 lets enemies
finish the player that way.
- A. Any aimed attack press whose resolved damage is lethal plays a paired finisher, for the player and for marked
  enemies, and the glossary entry is amended to say so. **(Recommended: it keeps D-20261005-18 to D-20261005-20 and the
  "frequent and flowing" finisher pillar.)**
- B. Only counters and follow-up strikes, as the glossary is written. D-20261005-18's second route and D-20261005-20's
  enemy paired finishers are retired.
- C. The player: only counters and follow-up strikes. Enemies: any lethal attack, per D-20261005-20.
- D. The player: any lethal aimed press. Enemies: none.

**Q29. For a Heavy press the charge is unknown at the press. When is "lethal" decided?**
- A. At the press, from the uncharged damage. A charge that later makes the hit lethal kills with the ordinary heavy hit.
  **(Recommended: the decision stays at the press.)**
- B. At release, from the latched charge.
- C. At the press and again at release.
- D. A Heavy press never starts a paired finisher.

**Q30. For a buffered press, is "the press" the moment it is pressed or the moment it executes?**
- A. When it executes, against the target and its health at that moment. **(Recommended.)**
- B. When it is pressed; the result is kept until it executes.
- C. When it is pressed, and cancelled if the target has changed.
- D. A buffered press never starts a paired finisher.

**Q31. What does the glossary's "can be finished" marker do, given "no per-character exemptions" for a lethal parry
(D-20261005-38)?**
- A. It says nothing about whether a character can die. It says only that paired finisher content exists for its body;
  without it, a lethal strike plays the single-character strike and the death reaction. **(Recommended: it keeps
  D-20261005-38.)**
- B. Unmarked characters never get a paired finisher, and a lethal strike kills them as an ordinary hit.
- C. Unmarked characters cannot be killed by a parry: an exemption, which contradicts D-20261005-38.
- D. Remove the marker from the glossary.

**Q32. In the aim cone, what does "Staggered or eligible enemies win ties" mean?**
- A. Any Staggered or eligible enemy in the cone beats every other one. Among them, the one nearest the camera direction
  wins. **(Recommended: it matches "prefers" in D-20261004-35.)**
- B. Candidates are scored by angle and distance, and Staggered or eligible enemies win only an exact tie.
- C. Staggered or eligible enemies get a score bonus.
- D. They win only within half the cone.

**Q33. Enemy paired finishers are "never because of a guard break" (D-20261005-20). Which reading?**
- A. A guard break never grants one by itself, but a lethal attack on a player Staggered by a guard break still plays one.
  **(Recommended: health decides, as everywhere else.)**
- B. A player Staggered by a guard break cannot be finished until that ends.
- C. A player who is Staggered for any reason cannot be finished.
- D. The phrase only notes that the old Staggered route is gone.

**Q34. Does the ×1.5 follow-up strike also apply to an enemy Staggered by a guard break?**
- A. Yes: it applies to any Staggered enemy, as D-20261005-39 is worded. **(Recommended.)**
- B. Only after a parry.
- C. A guard break gets its own multiplier.
- D. No follow-up after a guard break; ordinary attacks only.

### Several attackers

**Q35. During your parry or paired finisher, can a second enemy's swing that was already in progress touch you?**
D-20261005-21 lets in-progress swings finish. D-20261005-36 says the other attack can be parried after the lockout, or
blocked.
- A. No: you cannot be touched during your own parry or paired finisher, which is today's rule for whoever starts a
  paired animation. D-20261005-36's second press then applies only before your parry's sequence begins.
  **(Recommended.)**
- B. Yes, and it interrupts your sequence.
- C. Yes, and a Block press during your sequence can parry it, queuing a second parry.
- D. In-progress swings are cancelled when your sequence starts, which is today's abort.

**Q36. What is the per-enemy attack cooldown, and when does it start?**
- A. 1.0 s, starting when the enemy's attack ends. **(Recommended: today's value, counted so that it actually spaces
  attacks out.)**
- B. 1.0 s, starting when the token is granted, as today.
- C. 0.5 s, from the end of the attack.
- D. Set per enemy type.

**Q37. Do other enemies hold during a non-lethal follow-up strike?**
- A. No: D-20261005-21 covers parries and paired finishers only. **(Recommended.)**
- B. Yes, as for a parry.
- C. Only when the strike is paired.
- D. Only against the struck enemy.

### Guard, clocks and data

**Q38. How is the guard arc measured, and how is the block turn set?**
- A. The arc is measured as today: the defender's facing against the line to the attacker, at contact. The block turns
  by up to the full angle, at a rate chosen from rendered captures; today's 180°/s and 70° are the starting points.
  **(Recommended.)**
- B. The arc is measured from the guard-aim direction once guard aim exists, and from facing until then.
- C. The arc is measured from the blade's impact point.
- D. A fixed partial turn at a fixed rate, the owner's "a bit".

**Q39. Which clock do combat durations use?**
- A. Game time for all of them (Staggered, hit-stun, the lockout, the guard-break window and the windows), so that slow
  motion and hitstop stretch them alike. The slowdown's 0.3 s cap counts real time. **(Recommended.)**
- B. Real time for all of them.
- C. Game time for character states (Staggered, hit-stun), and real time for input timers (lockout, buffer, grace,
  guard-break window).
- D. Chosen per setting.

**Q40. Do enemies get combo sets?** Today enemy AI uses its own attack list, which bypasses the attack configuration.
- A. Yes: each enemy type has a combo set, its AI picks slots, and its counter and finisher pools come from it.
  **(Recommended: one model.)**
- B. No: the AI lists stay, and enemy finisher content goes on the reaction profile.
- C. The AI lists stay, but refer to slots in a shared enemy combo set.
- D. Decide when enemy types are designed.

**Q41. What does a press do when the slot has no link for that input?**
- A. The combo ends: the press is buffered and starts the opener once the attack finishes, without a combo blend.
  **(Recommended: it stays distinct from an explicit back-to-opener link, which continues the combo.)**
- B. The same as a back-to-opener link.
- C. The press is dropped.
- D. Whatever today's code does, once verified.

### Migration and delivery

**Q42. Migration clean-ups.** What happens to `LightAttack_11`, the unreachable `None` direction key, the unused Sprint,
Jump and Plunging slots, the directional context tag, and the Terminal tag?
- A. **(Recommended.)**
  - `LightAttack_11` becomes an unlinked slot, flagged unreachable, for the owner to place.
  - The `None` key is dropped.
  - The three unused slots are removed, and come back as entry rules when those moves are designed.
  - The directional attacks lose their `Context.DirectionalFollowUp` requirement, because the hold link is the gate.
  - Terminal is replaced by empty links.
- B. As A, but `LightAttack_11` is linked after `LightAttack_10` as the light combo's last hit.
- C. As A, but `LightAttack_11` is deleted.
- D. Migrate everything exactly, and only flag the issues.

**Q43. Do you approve the proposed asset names (7.5)?**
- A. Approve as proposed. **(Recommended.)**
- B. Approve, but keep test-only assets under one `Test` folder.
- C. Rename the data assets only; montages keep their names.
- D. Propose different names.

**Q44. What happens to PR #137, the rework of today's post-parry press window?**
- A. Merge it now as an interim fix for the live bug, a window of about 50 ms, and retire it in the parry-counter stage,
  keeping its failure-path tests. **(Recommended.)**
- B. Close it, because the new flow replaces it.
- C. Merge only its failure-path fixes.
- D. Leave it open until the parry-counter stage.

**Q45. What remains of "defense role per enemy type" (D-20261004-27), now that there are no per-enemy-type modes
(D-20261005-24)?** The register's D-20261004-27 still says "Evade becomes a universal escape". The 2026-10-04 log's
correction note says that was not part of the option the owner picked.
- A. Content direction only: which attacks each enemy type has (dangerous unparryable attacks, guard breakers, guarding).
  Every rule is the same for all enemies. **(Recommended.)**
- B. It is superseded entirely.
- C. Per-type parry rules remain.
- D. Revisit it when enemy types are designed.

**Q46. Do you approve the stage order (section 9)?**
- A. As proposed: the authoring foundation, the editor and the data, then hits, guard, parry, the parry-counter and
  attackers. **(Recommended: attack data changes once, as D-20261005-04 intended.)**
- B. Rules first, on today's attack data, with authoring after.
- C. Data and rules first, and the editor last.
- D. A different order.

---

## Appendix A. Decisions used

| Decision | What it decided | Where |
|---|---|---|
| D-20261002-07 | Reaction variants become arrays; death reads the array | 4.5, 7.4 |
| D-20261002-10 | Log when the defense slot is empty (carried to the profile slot) | 4.5 |
| D-20261002-11 | Tests never pin configurable values | 8.1 |
| D-20261003-01, -03 | The hit lock in the current reaction mode; inputs buffered | 3.7 |
| D-20261003-08 | Consolidation into attack, profile and settings (confirmed by D-20261004-20) | 4 |
| D-20261004-02 | Guard aim with magnetism (queued) | 3.11, Q38 |
| D-20261004-03 | Player free, attacker held | 3.3, 3.4 |
| D-20261004-06, -07 | Parry readability; parry assists as settings | 3.2, 4.5 |
| D-20261004-09 | Hold the owner's content for this work | 7.6 |
| D-20261004-17, -19 | No planning labels; Gate assets renamed here | 7.5, 8.1, 9 |
| D-20261004-20 | Data model with defaults and overrides, and a base profile | 1.3, 4 |
| D-20261004-21 | Code-driven block and parry turn | 3.1, 3.2 |
| D-20261004-22 | Explicit parry fallback (location: Q13) | 7.4, Q13 |
| D-20261004-23 | Parry any direction; block front; data keyed by direction | 3.1, 3.2 |
| D-20261004-24 | One block rule | 3.1 |
| D-20261004-26 | Design pillars | 1.2 |
| D-20261004-27 | Defense role per enemy type (Q45) | 3.5, Q45 |
| D-20261004-30 | Turn-deflect into front pairs | 3.2, 3.3 |
| D-20261004-32 | Charge-loop snap fix with the redesign | 7.6, 9.3 |
| D-20261004-33 | Timing read from the montage | 5.4, 7.2 |
| D-20261004-34 | Keep Force Root Lock | 7.6 |
| D-20261004-35 | Aim decides; the state is called Staggered | 3.3, Q18 |
| D-20261004-38 | Knockback layering | 3.10, 4.2 |
| D-20261004-41, -42, -43 | Hit-stun on the attack; type looks, pressure interrupts; guard break added | 3.5–3.7 |
| D-20261004-45 | Exact mechanics before code | header |
| D-20261005-01 to -04 | Combo set with edit-through; grid and node map; back-to-opener links; one spec | 4.3, 5 |
| D-20261005-05 to -10 | The parry window, lockout, parryable by default, Staggered default, minimum and scale, slowdown | 3.2, 3.4 |
| D-20261005-11 to -15 | Guard arc and turn, no chip, block in recovery, the player's guard, counted guard break | 3.1, 3.5 |
| D-20261005-16, -17 | Interruption is pressure against armor | 3.6 |
| D-20261005-18 to -21 | Finisher eligibility, trigger, enemy finishers, others hold | 3.8, 3.9 |
| D-20261005-22 to -24 | Opportunity equals Staggered time; strike fallback; the parry is the counter | 3.3 |
| D-20261005-25, -26 | Count by pressure; 4 within 3 s | 3.5 |
| D-20261005-27, -28 | 60° aim cone; 350 cm | 3.3, 3.8 |
| D-20261005-29 to -31 | Guard breakers; counter fallback order; guard-break Staggered | 3.3–3.5 |
| D-20261005-32, -33 | Lockout 0.25 s; slowdown bounds | 3.2 |
| D-20261005-34 to -37 | Hit-stun values; 2 attackers; lands first; what hit-stun blocks | 3.7, 3.9 |
| D-20261005-38, -39 | A lethal parry plays a paired finisher; the follow-up strike | 3.3, 3.8 |
| D-20261005-40 | The glossary | 2, throughout |

**Superseded, and not used as rules:**
- D-20261004-28, the two modes by enemy type, superseded by D-20261005-24;
- D-20261004-36's window lead, superseded by D-20261005-05;
- D-20261004-11 and D-20261004-13, superseded by D-20261004-20 and D-20261004-21.

## Appendix B. Ambiguities, conflicts and readings

Each item is either an open question, or a reading this spec chose that the owner should see.

**Conflicts between active decisions:**
1. The glossary's "paired finisher … Never plays otherwise" against D-20261005-18, D-20261005-19 and D-20261005-20 (Q28).
2. The glossary's "can be finished" marker against D-20261005-38's "no per-character exemptions" (Q31).
3. The glossary's armor "plus" windows against D-20261005-17's windows that "raise it" (Q23).
4. The table confirmed at 04:10 ("×1.5 single-character strike") against D-20261005-39's picked text ("paired when
   content exists") (Q16). The table's wording for the non-lethal case is written here as "non-lethal counter", per
   the glossary.
5. D-20261004-22 puts parry entries in the reaction profile; D-20261005-24 takes the counter from the combo set (Q13).
6. D-20261004-35's "a press aimed elsewhere … releasing the held enemy", now that the window is the Staggered time
   (D-20261005-22) (Q18).
7. D-20261005-15 in the owner's words, "per player", against D-20261005-14, under which the player breaks only to guard
   breakers (Q22).
8. D-20261003-02's "phase resistance" against D-20261005-17's armor windows (the state-based option was not picked).
   Phase resistance is treated as replaced. PR #138's pillars page still lists it.
9. D-20261004-27's per-type defense role against D-20261005-24's "no per-enemy-type modes" (Q45). The register's
   D-20261004-27 entry still carries "Evade becomes a universal escape", which the 10-04 log's correction note
   withdrew. **The register needs that fix.**
10. D-20260929-07 says to wire `FFinisherTriggerConfig`. D-20261005-18 removes its thresholds, so it is retired.
11. `CLAUDE.md`'s "Stateful Block input is immediate" against D-20261005-13's buffered Block press.
12. PR #138's design pillars page predates the 10-05 decisions: the two modes, the window lead, phase resistance, and the
    post-parry window.

**Wording that leaves the rule open:**

13. "Missed parry" is undefined, because Block is both guard and parry (Q1).
14. D-20261005-36's "after the lockout" implies a lockout after a successful parry; D-20261005-06 names only missed
    presses (Q2).
15. "Contact" in D-20261005-09 (Q3); attacks with no authored window under D-20261005-07 (Q4); the minimum and the scale
    (Q5).
16. "Fastest natural speed" in D-20261005-33 (Q6), and the slowdown's shape (Q7).
17. Counter damage was never decided (Q11), nor what the ×1.5 multiplies (Q15).
18. "Swing" in D-20261005-24 and D-20261005-30 (Q12).
19. When Staggered starts (Q14).
20. The finisher pool's home and keys were stated in the session, not picked (Q19).
21. "Briefly Staggered" for the player in D-20261005-14: what the player may do (Q20).
22. "Within 3 s" (Q21).
23. "Charged" is not an attack type, and the latched charge is never written today (Q24).
24. "Not on guard break" in D-20261005-20 (Q33).
25. The ×1.5 follow-up strike on a guard-break Staggered state (Q34).
26. The interplay of D-20261005-21, -35 and -36 (Q35).
27. "A short cooldown" in D-20261005-35 (Q36).
28. "Rotate a bit" in D-20261005-11, and the arc's reference direction while guard aim is queued (Q38).
29. No decision names a clock (Q39).

**Readings this spec chose, for the owner to see:**

30. D-20261005-24's "tough ones two" is narrowed by D-20261005-39 to "several exchanges".
31. "The enemy recovers" (D-20261005-39) is read as: Staggered ends when the non-lethal follow-up lands.
32. The counter is read as a strike that drops guard under D-20261005-13.
33. D-20261004-03 is carried over as "after a non-lethal counter the player is free, and the Staggered enemy is held".
34. The breaking hit of a guard break deals no damage (D-20261005-12 and the glossary's "negates all damage"; the "full
    damage when the guard breaks" option was not picked).
35. A guard break's Staggered time comes from the breaking attack, and starts at the breaking contact (D-20261005-31).
36. The directional-light guard-breaker default applies to slots reached through a hold link.
37. Hit-stun applies to enemies too: "the victim can't act" (D-20261005-34's option). The D-20261005-37 question spoke of
    the player.
38. A lethal counter that reaches the last fallback step kills with the single-character strike and the death reaction.
39. Today's prediction-confidence and path tests are dropped as separate parry gates, because "window open and aimed at
    you" covers them.
40. A window cannot open before its attack starts. This is arithmetic, not a decision.
41. The limit of two attackers counts the attackers of the player; it is set in the combat settings.
42. The set-level type default came from the authoring comparison's recommendation. It entered the decisions only through
    the glossary's "Combo set" entry (D-20261005-40), and reading it as D-20261004-38's per-weapon layer is this spec's.
43. The 0.12 s floor came from a proposed, unmerged hit-reaction spec. The owner adopted the number by picking
    D-20261005-34, whose option text calls it "the 0.12 s floor decided earlier".
44. The glossary's back-to-opener entry uses a word that its own last table retires, for the parry sequence. This spec
    says "combo" for a run of linked attacks.
45. The owner's held knockback values (60 cm and 150 cm) come from a condensed summary. Check them on the asset.
