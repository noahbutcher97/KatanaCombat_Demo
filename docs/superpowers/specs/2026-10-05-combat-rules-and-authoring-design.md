# Combat rules, data model and attack authoring: design

- **Date:** 2026-10-05.
- **Revised:** 2026-10-10, for D-20261007-53 to -59 and D-20261010-01 to -04: the kit as the root asset, the combat
  rules split from the per-character settings, the player by data, the marker's home, table-shaped selection with one
  trace, the timeline as the attack editor, and paired content found by key.
- **Status:** draft for the owner's review, section by section. It authorizes no code change and no asset save.
- **Why this exists.** The owner's standing rule [D-20261004-45]: "i want to get more specific with the rules for guard
  breaks, finishers, parries, and counters, and what causes what and how and why and exact mechanics i dont want to find
  out how something is being calculated or arbitrated later once youve already generated code". The owner also chose one
  combined spec for the combo set, attack data, impact layering and the reaction profile, built in stages
  [D-20261005-04].
- **Baseline:** `origin/main` at `a7693bb4`; every file and line cited is from that commit. While this spec was being
  written, four pull requests merged into `main` (now `e47da15e`): #136, one block rule (the legacy 70° cone is gone);
  #139, attack timing read from the montage; #138, the October findings, design pillars and decision log; and #137, the
  rework of today's post-parry press window, which stays as the interim behaviour until the parry-counter stage (9.8)
  retires it. Their effect on the rules is noted where it matters. No pull request that touches these rules is open.
- **Sources.**
  - The decisions register, `docs/sessions/registers/decisions.md` (local and git-excluded), through D-20261010-04, plus the ruling R-20261005-01. Only
    active decisions are used, and "superseded by" is respected. Where the register records the owner's words, those words
    are quoted; where it records a reading of them ("my reading", "the owner may veto"), the hedge is carried into Appendix
    B unchanged.
  - The owner's own answers, read from the session transcript, where the register condenses them.
  - The adopted combat glossary [D-20261005-40], reproduced as section 2 with the amendments of D-20261005-42,
    D-20261005-47 and D-20261005-48.
  - `.superpowers/design/combat-rules-as-is.md`, the current-rules reference. It is model-written. Every code fact this
    spec relies on was re-read at `a7693bb4`, unless it is marked **[reported]**.
  - The authoring comparison, the ComboCreator and Duet review, the defense-role feasibility report, the owned-deflect
    check, the directional counter prototype, and the defense data traces (all under `.superpowers/`).
  - The feature set and the authoring model (`.superpowers/design/combat-feature-set.md`, `combat-authoring-model.md`),
    the montage-notify ownership research (`research-montage-notify-ownership.md`) and the editor design pass
    (`authoring-design-pass-slot-montage.md`, with `moveset-editor-mockup.html`). All four are model-written: pointers
    to the decisions and to engine facts, never the evidence. Each engine or code fact taken from them is marked
    **[reported]**.
  - The data-model research of 7 October, ordered by the owner (the 2026-10-07 log, E26): the stress test of eight
    storage models against twelve scenarios (`.superpowers/design/research-data-model-stress-test.md`) and the survey of
    fifteen sources (`research-data-model-survey.md`). Both are model-written and say so; their asset counts, scores and
    "reusable" judgments are marked **[reported]**, and the decisions they led to (D-20261007-56 to -59, D-20261010-01
    to -04) are the authority, not the reports (4.17).

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
before content), D-20261007-07 (how a lethal parry-counter ends), D-20261007-11 (the words parry, counter and
parry-counter), D-20261007-35 (every attack field overridable per slot) and D-20261007-38 (windows are data).

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
- block and parry reactions on the reaction profile, now the kit's defense set [D-20261005-48, D-20261010-02].

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
   [D-20261004-20, D-20261004-38, D-20261005-01, D-20261005-61], and every attack field can be overridden per slot
   [D-20261007-35]. Each character's reaction profile inherits from a base profile and overrides it [D-20261004-20].
   Global rules are not layered: they come from the one combat rules asset [D-20261007-53].
4. **One place per question.**
   - What does this attack do? The attack.
   - What does this character fight with? Its kit: the weapon plus a combo set, a defense set and a paired set.
   - What follows what? The kit's combo set.
   - How does this kit block and parry, and does a parry seek a paired counter? The kit's defense set.
   - Which two-character sequences does this kit play, and can it finish the player? The kit's paired set.
   - How does this character's body react, resist and break, and is it the player? Its reaction profile.
   - What are the global rules? The combat rules, one asset for the project.
   - What are this character's type defaults and its default kit? Its combat settings.

   Sources: [D-20261004-20, D-20261005-01, D-20261005-48, D-20261005-57, D-20261007-53, D-20261007-54, D-20261007-55,
   D-20261010-01, D-20261010-02].
5. **Input is never lost.** Presses made during your own attack or during hit-stun are buffered
   [D-20261005-13, D-20261005-37, D-20261003-03].
6. **Feel is judged on rendered captures**, not on code or automation alone. This is project practice, recorded in the
   project memory and in [D-20261004-40].
7. **Rules before content.** No combat rule is derived from what content exists; content is created, bought or
   procedurally fitted to the rules, and an asset limitation is met with technical or procedural means, not a weaker
   rule [D-20261006-04].
8. **Windows are data.** The attack owns every timing window, anchored to its animation, and the runtime reads them from
   the resolved attack spec. Montage notifies are an export of that data for the animation editor, never the thing
   played against [D-20261007-36, D-20261007-38, D-20261007-39].
9. **"Will it hit" is predicted, never guessed.** The contact predictor answers it with a confidence, for the parry
   window, the enemy's finisher choice and the player's lethal strike (3.15) [D-20261007-22, D-20261007-24].
10. **Selection is a table with a trace.** Which counter answers a parry, which finisher plays, which slot an enemy picks
    and which opener a press starts from are rows of one table shape, judged by one evaluator that records the rows it
    evaluated; the simulate-press panel shows that trace (4.16, 5.12) [D-20261007-57]. Paired content is found by key
    through those tables, never by a reference from another set [D-20261010-01].
11. **The player is data, not identity.** Every rule that treats the player differently reads a field on the player's
    reaction profile; no rule asks "is this the player" (4.8) [D-20261007-54].

---

## 2. Glossary

**Status:** approved by the owner as written at `a34ecd69` [D-20261005-65]; the reaction-profile entry was then narrowed by
D-20261005-68, and the decisions of 6, 7 and 10 October, through D-20261010-04, amended the entries marked below.

### 2.1 The adopted glossary

The owner adopted this glossary as written [D-20261005-40]. It is reproduced from
`.superpowers/design/combat-glossary.md` with the heading levels adjusted. Later decisions changed it, and each
changed entry is marked **[amended]** with its decision: the paired finisher's scope [D-20261005-42], the reaction
profile's contents [D-20261005-47, D-20261005-48], and three new authoring entries, defense set, paired set and weapon
data [D-20261005-48, D-20261005-49, D-20261005-57]; then the words parry, counter and parry-counter [D-20261007-11],
the aim cone's reference [D-20261007-12], the counter's shape [D-20261007-07], the defense set's damage [D-20261006-03]
and the lenient lockout [D-20261007-18]; then the parry window's home [D-20261007-38], the slot [D-20261007-35], the
paired set's finisher lists [D-20261007-31] and the second attacker under others hold [D-20261007-44]; then the
reaction profile's marker and player fields [D-20261007-54, D-20261007-55], the defense set's and paired set's
references, now lookups by key [D-20261010-01], and the root asset, weapon data renamed kit [D-20261010-02]. Every other
line is verbatim.

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
  the parry-assist setting. Turning is never limited to it. The window is the attacker's and is held as data on the
  attack, a lead time before predicted contact, anchored to the animation; the montage notify is a derived mirror of it
  (3.2, 4.13). The project's working rule "ParryWindow is on the attacker's montage" becomes "the parry window is the
  attacker's, held as data". **[amended by D-20261007-38]**
- **Parry lockout** — 0.25 s after a missed parry press during which no parry can trigger. Blocking still works. A press
  made during the lockout is remembered and counts as a parry when the lockout ends with the window still open; the
  length is a tunable to be felt first and may go to zero. **[amended by D-20261007-18]**
- **Aim cone** — 60° either side of the stick direction, or of the character's facing when no direction is held;
  Staggered or eligible enemies win ties, nearest otherwise; the camera never decides. Used for every attack press:
  follow-up strikes, paired finishers and ordinary attacks (3.13). **[amended by D-20261007-12]**

#### Authoring
- **Combo set** — the asset holding slots, links and entry rules for one moveset, plus set-level type defaults.
- **Slot** — one placement of an attack in a combo set, with optional per-field overrides. A slot is an attack plus a
  diff, and every attack field can be overridden per slot (4.4). **[amended by D-20261007-35]**
- **Attack** (`UAttackData`) — the move itself: montage, timing, damage, pressure, flags, impact feedback. "Timing" is
  every window of the move, held as data: the phases, the parry window, the hold start, the armor windows, the contact
  marker and the cancel point (4.5). **[amended by D-20261007-38]**
- **Reaction profile** — a character's reactions to hits only: hit reactions, armor base, guard-break threshold, window
  and counting toggle, the Staggered reaction and recoil, hit-stun responses, the character's own Staggered time, whether
  a dodge may escape its locked states, and parry assists. The player is whichever character's profile says so: counting
  off, its own Staggered time, the dodge escape allowed; no rule asks for an identity. The "can finish the player" marker
  is no longer here: it is the kit's paired set holding player-finisher content. It never arbitrates movement or camera;
  those get their own profiles when needed.
  **[amended by D-20261005-46, D-20261005-47, D-20261005-48, D-20261005-55, D-20261005-68, D-20261005-88,
  D-20261007-54, D-20261007-55]**
- **Defense set** — the weapon's block and parry responses: block reactions, the guard enter and exit clips, and for
  each attacker direction and swing a counter row with the deflect, the fallback strike, the counter's damage, and
  whether a paired counter is sought; the counter itself is found by key in the kit's paired set, never referenced.
  Referenced by the kit. **[added by D-20261005-48; amended by D-20261005-55, D-20261005-57, D-20261006-03,
  D-20261007-57, D-20261010-01, D-20261010-02]**
- **Paired set** — the kit's two-character sequences, keyed by direction and role: counters (each able to kill; an
  authored kill sequence is optional) and finishers, a list per attacker direction chosen by the variety rule. Nothing
  references into it: a defense row asks it for counters by direction and swing, a lethal strike asks it for finishers by
  direction. Each entry is one content asset; the content's own variants are alternates of that entry, never the
  direction choice. A paired set that holds player-finisher content is one that can finish the player. **[added by
  D-20261005-57; amended by D-20261007-07, D-20261007-31, D-20261007-55, D-20261010-01, D-20261010-04]**
- **Kit** — the root asset a character fights with: the weapon (its mesh, sockets, traces and multiplier, the physical
  data) plus one combo set (attacks), one defense set (defenses) and one paired set (two-character sequences). Two kits
  may share the weapon and the defense and paired sets and differ in combo set. A character with no kit uses its combat
  settings' default kit. This entry was "Weapon data" until D-20261010-02 renamed the root. **[added by D-20261005-48,
  D-20261005-49; amended by D-20261005-57; renamed and reshaped by D-20261010-02]**
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
| **Contact** (predicted arrival) | The moment the attacking blade is predicted to reach the defender, computed at runtime from the swing's trajectory [D-20261005-73]: the "when" of the contact predictor (3.15) [D-20261007-24]. The parry window is measured back from it, the turn finishes at it, and "lands first" compares it. |
| **Contact predictor** | The runtime function that, for an attack against a target, predicts whether and when the blade path reaches the target's capsule, returning a confidence (3.15). Its users are the parry window, the enemy's finisher choice and the player's lethal strike [D-20261007-24]. |
| **Effective parry window** | The authored lead time before predicted arrival, after the 0.35 s minimum and the parry-assist scale are applied (section 3.2). |
| **Missed parry** | A Block press made while a parryable attack aimed at the defender is in progress, outside that attack's window. It starts the parry lockout. A press with nothing incoming is just a guard [D-20261005-71]. |
| **Deflect** | The parry as content: the defender's clip that meets the incoming blade. "Deflect" names the clip, "parry" the action [D-20261007-11]. |
| **Turn-deflect** | An authored 90° turn (for the sides) or 180° turn (for behind) that flows into a front counter pair. Motion warping hides the leftover angle [D-20261004-30]. |
| **Swing** | The parried attack's swing, one of four: slash left, slash right, overhead, thrust [D-20261005-45]. It is a field on the attack, filled by the "Auto-detect swing" editor action or by hand [D-20261005-84]. |
| **Counter pool** | The defense set's directional counter entries, keyed by the attacker's direction and the parried attack's swing [D-20261005-24, D-20261005-45, D-20261005-48]. Each is a row of the defense set's counter table (4.16); a row that seeks a paired counter finds it by key in the kit's paired set [D-20261005-57, D-20261007-57, D-20261010-01]. |
| **Counter entry** (counter row) | One row of the counter pool: the deflect, the fallback strike, the counter's damage, and whether a paired counter is sought. The paired counter is found by key in the kit's paired set (direction and swing, lane preferred) and shares the row's deflect. The counter can kill at its own contact; an authored kill sequence on it is optional and plays instead when the press is lethal, never as a separate clip after it [D-20261005-38, D-20261005-48, D-20261005-57, D-20261006-03, D-20261007-07, D-20261010-01]. |
| **Kill sequence** | An optional authored two-character ending on a paired-set counter, played instead of the counter's own contact when its damage is lethal [D-20261007-07]. |
| **Non-lethal counter** | What a parry-counter plays when the counter's damage is not lethal: a paired counter, or the single-character strike when none fits. It leaves the enemy Staggered [D-20261005-23, D-20261005-24, D-20261007-07]. |
| **Cancel notify** | The one "free from here" notify on any non-attack montage (3.0) [D-20261006-02]. |
| **Single-character strike** | A strong cinematic strike played by the defender alone, together with the enemy's stagger reaction. It is the last fallback when no paired animation fits [D-20261005-23, D-20261005-30]. |
| **Finisher pool** | The paired set's finishers: rows of clips per attacker direction, keyed by direction only [D-20261005-47, D-20261007-31], asked for by key from the attacker's kit's paired set; the combo set holds no reference to them [D-20261005-57, D-20261010-01]. One is chosen by the variety rule, and there is no throttle [D-20261007-31]. |
| **Fit to clip** | Bringing a lethal strike whose direction has no finisher clip to the nearest authored direction: the attacker's approach warp moves the pair, the victim turns within a small budget, and the first part of the clip is blended procedurally to hide the approach (3.8) [D-20261007-22, D-20261007-23]. |
| **Fitting budgets** | The turn, distance and room limits a fit may use, tunables on the paired set with combat-rules defaults; beyond them fitting fails and an ordinary attack plays [D-20261007-23, D-20261007-53]. |
| **Scripted hit** | A hit applied at a clip's contact marker without a weapon trace, because the predictor already confirmed the target: the single-character counter and the follow-up strike land this way (3.3) [D-20261007-41]. |
| **Super armor** | An armor window above any pressure: the character is never interrupted, and the flinch still plays (3.6) [D-20261007-26]. |
| **Block shove** | The short push a blocked hit gives the blocker, with its own per-type default, not derived from the hit push [D-20261005-59, D-20261007-27]. |
| **Guard-break push** | The push the hit that breaks a guard gives the defender, with its own per-type default, not derived from the hit push [D-20261005-59, D-20261007-27]. |
| **Shove-back** | The short settings-distance push that moves a second attacker away from a paired sequence when its swing is aborted (3.9) [D-20261007-44]. |
| **Finisher range** | 350 cm: the maximum distance at which a paired finisher can start [D-20261005-28]. |
| **Attack token** | An attacker's permission to attack a target. At most 2 are held against one target at once (configurable); the tokens count the attackers of whoever is targeted, not of the player as such, and each enemy has a short cooldown [D-20261005-35, D-20261007-54]. |
| **Free mode** | Targeting with no lock: every attack press aims by the aim cone around the stick direction, or the character's facing when no direction is held (3.13) [D-20261007-12]. |
| **Lock mode** | Targeting after a lock press: every press aims at the locked target, the player strafes facing it, and a flick of the look stick (or a scroll) switches targets (3.13) [D-20261007-13]. The lock follows what the player does: acting toward another enemy transfers it [D-20261007-46]. |
| **Dodge** | The player's committed evasive move from the Evade press (3.14): aimed by the stick, camera-relative, with an authored invulnerability window, no resource and no cooldown [D-20261007-49 to D-20261007-52]. |
| **I-frame window** | The dodge's authored invulnerability window, held as data on the dodge like every other window [D-20261007-50]. |
| **Stick direction** | The direction the movement stick is held, read in the frame movement uses (a reading, Appendix B). |
| **Others hold** | While the player is in any paired sequence, a parry-counter, a paired finisher or an enemy's finisher, other enemies start no new attack; an attack is in progress once its montage has started, in any phase, and a combo continuation is a new attack [D-20261005-21, D-20261007-29]. A second enemy whose swing is already in progress when the sequence starts aborts and is shoved back [D-20261007-44]. The enemies resume when the sequence ends, each after its resume delay (3.9). Always written as "others hold", to keep it apart from holding a button. |
| **Resume delay** | The per-enemy wait before a holding enemy re-enters the attack queue when the sequence ends: a combat-rules default with a per-profile override [D-20261007-29, D-20261007-53]. |
| **Re-grant delay** | The short settings wait before a freed attack token is granted again [D-20261007-30]. |
| **Lane** | A group of slots shown together in the moveset grid: light, heavy and directional [D-20261005-02]. |
| **Link** | What a slot does next for one input: Light, Heavy, or hold-and-release Light toward Forward, Back, Left or Right. A link points at a slot, back to the opener, or nowhere. |
| **Opener** | The slot a combo starts from when an input is pressed and no combo is running. Today: `DefaultLightAttack` and `DefaultHeavyAttack` on the attack configuration. |
| **Type default** | A value a character's combat settings hold for each attack type [D-20261004-20, D-20261004-38, D-20261007-53]. |
| **Set-level type default** | A per-type default held by a combo set: the per-weapon layer, second of the four in section 4.2 [D-20261005-40, D-20261005-61]. |
| **Attack override** | A field the attack asset sets for itself, replacing the default [D-20261004-20]. |
| **Slot override** | A field a slot sets, replacing the attack's value for that slot only [D-20261005-01]. Every attack field can be overridden [D-20261007-35]. |
| **Slot diff** | The set of a slot's overrides: what the slot changes about its attack. The row shows only the diff by default [D-20261007-35]. |
| **Save to asset** | The per-field action beside an overridden value that writes it into the attack asset and clears the override, with the used-by notice [D-20261007-35]. |
| **Edit-through** | Editing the referenced attack's own fields inline from the combo set [D-20261005-01]. |
| **Extract to new attack** | Turning a slot's diff into a new attack asset that the slot then references. It replaces "make unique" [D-20261005-01, D-20261007-35]. |
| **Data window** | A timing window held on the attack as data: a phase boundary, the parry window, the hold start, an armor window, the contact marker or the cancel point, anchored to sequence time plus segment and resolved to montage time on load or cook [D-20261007-36, D-20261007-38]. |
| **Derived notify** (mirror) | The montage notify that "export to montage" draws from a data window, in a distinct colour with "(from attack data)" in its name. The runtime never reads it [D-20261007-38, D-20261007-40]. |
| **Export to montage** | The editor action that writes an attack's data windows into its montage as derived notifies; today's notify generator [D-20261007-38]. |
| **Adopt from montage** | The editor action that reads a hand-authored montage's notifies into the attack's data windows once, for migration; today's timing action [D-20261007-38]. |
| **Telegraph** | The automatic cue (trail tint, cue sound, flash) that fires at attack start from the resolved attack spec for each readable flag, from a telegraph set keyed by flag [D-20261007-37]. |
| **Base profile** | The reaction profile that character profiles inherit from and override [D-20261004-20]. |
| **Combat settings** | `UCombatSettings`: a character's per-type defaults and its default kit; the global rules left it for the combat rules [D-20261003-08, D-20261004-20, D-20261007-53]. |
| **Combat rules** | `UCombatRules` (proposed name): the one asset for the project holding every value that must agree for everyone: the window minimum, the lockout, the slowdown, the Staggered default, the aim cone, the ranges, the attacker cap and cooldowns, the predictor settings, the telegraph set and the counter damage default (4.15) [D-20261007-53]. |
| **Weapon** (physical data) | `UWeaponData`: the weapon's mesh, sockets, trace settings, damage multiplier, equip and holster montages and effects pool, referenced by the kit (4.3). "Weapon" alone names this physical data; the root is the kit [D-20261010-02]. |
| **Selection table** | A set of rows judged by one evaluator with one column vocabulary: input, combo step, stance or ground state, target state, direction, swing, lane, lethal, distance range and weight. The counter grid, the finisher lists, the enemy's slot choice and the opener and entry rules are all selection tables (4.16) [D-20261007-57]. |
| **Row trace** | The evaluator's record of every row it judged for one press, with each column's pass or fail and the row chosen. The simulate-press panel shows it (5.12) [D-20261007-57]. |
| **Simulate-press panel** | The editor panel that takes a pretend press and its context and shows the row trace, so that "why didn't it play" is answered before play (5.12) [D-20261007-57]. |
| **Timeline** (attack editor) | The editor opened from an attack or a slot: lanes of data windows, phases, hold start, contact, armor, hits, telegraph and FX moments drawn over the montage, with a combo lane showing the links out at the combo point (5.11) [D-20261007-58]. |
| **Alternate** | One of a paired content asset's own internal variants (an angle sector, a distance, a weight). The content picks among its alternates inside the direction, swing and variety choice our tables have made; an alternate never decides the direction (4.12) [D-20261010-04]. |
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
- attacks never carry it: their cancel point stays the phase transition from Active to Recovery, held as attack data like
  every other window [D-20261007-38];
- the dodge has a cancel point of its own (3.14) [D-20261007-51]; it is held on the dodge's data beside its i-frame
  window, "like every other window" [D-20261007-50] (a reading, Appendix B);
- a montage with no cancel notify is read as ending at montage end, and validation warns (4.13).

Inside a parry-counter, neither character can be touched until its own cancel notify fires [D-20261007-08]. The
paired-content interface (4.12) exposes each role's notify. The notify's class and its validation are in 4.13.

**What D-20261007-38 leaves alone.** That decision gives the attack's data ownership of the attack's windows. The cancel
notify on non-attack montages (deflects, reactions, paired halves) stays a notify placed by hand, because no attack data
owns those clips; so do armor windows on a character's non-attack animations (3.6). That is a reading (Appendix B).

**"Will it hit."** Wherever a rule needs to know whether a strike reaches its target, the contact predictor answers,
with a confidence against that user's threshold (3.15) [D-20261007-22, D-20261007-24]. No rule guesses it.

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
- D-20261005-70: the guard arc's default is global (the combat rules since D-20261007-53, 4.15) and the defense set may
  override it;
- D-20261007-25: an interrupting hit on a guarding character, from outside the arc or unblockable, ends the guard; because
  hit-stun allows guard, it returns at once while Block is still held, never counting as a parry press; a flinch never
  ends guard;
- D-20261007-51: a dodge press drops guard;
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
- a guard break (3.5);
- **an interrupting hit that reaches the guarding character**: a hit from outside the arc (Contact-6) or an unblockable
  hit (Contact-4) [D-20261007-25];
- a dodge press (3.14) [D-20261007-51].

**Guard on an interrupting hit** [D-20261007-25]. The hit ends the guard. Because hit-stun allows guard (Guard-4), the
guard returns at once while Block is still held, and that return never counts as a parry press; the character is in
hit-stun, so it blocks but does not parry until a new press. A flinch, a hit that does not interrupt, never ends guard.

The counter of a parry-counter is a strike. This spec reads it as an attack for this rule, so guard ends when the counter
starts (Appendix B). Guard comes back only through a new press, or through the buffer rule Guard-3.

**Block at contact.** The defense resolver evaluates each weapon contact in today's order, with the arc changed
(`DefenseResolver.cpp:289-375`):

| # | Condition | Outcome |
|---|---|---|
| Contact-1 | The contact is invalid, friendly, already consumed or stale. | Ignored (today). |
| Contact-2 | The defender is invulnerable or in i-frames. | Ignored (today). |
| Contact-3 | The defender started the paired animation now playing. | Ignored (today). |
| Contact-4 | The attack carries `Attack.Property.Unblockable`. | A hit at full damage through guard (today, `:337-341`). If it interrupts, guard ends and returns at once while Block is still held (below) [D-20261007-25]; a flinch leaves guard up. This holds even when the attack is also a guard breaker: unblockable wins, the guard does not break, and validation warns on the pair of flags (4.4) [D-20261007-16]. |
| Contact-5 | The defender is not guarding. | A hit (3.6). |
| Contact-6 | \|yaw\| > 75°. | A hit. If it interrupts, guard ends and returns at once while Block is still held (below) [D-20261007-25]. |
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
  with a combat-rules default of **360°/s**, tuned from captures [D-20261005-69, D-20261007-53].
- **Presentation.** The block reaction for the incoming direction, from the defender's kit's defense set
  [D-20261005-48]. While the arc is ±75° only the front entry is used. The data is keyed by direction now, so that full-circle blocks later need only
  content plus one setting, the guard arc [D-20261004-23].
- **The attacker** recoils if the attack carries `Attack.Defense.BlockInterruptible`. Otherwise its swing continues but
  cannot touch this defender again (today, `DefenseResolver.cpp:366-374`).
- **The block uses one of the swing's hit slots** (today).

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Guard arc (half-angle) | 75° | combat rules default; the defense set may override it | D-20261005-11, D-20261005-70 | 35°, `NormalBlockFinalTolerance` on the defense configuration |
| Block turn: amount | the full angle to the attacker | — | D-20261005-69 | at most min(70°, measured yaw), through motion warping |
| Block turn: rate | 360°/s, tuned from captures | combat rules default; the defense set may override it | D-20261005-69 | 180°/s |

**Changes from today:**
- The arc widens from 35° to 75°.
- Starting an attack drops guard. Today guard stays up through your own attack, and contacts within 35° are blocked
  mid-swing **[reported]**.
- A Block press during your own attack is buffered. Today it is refused and dropped (`CombatComponent.cpp:3317-3348`).
- The turn is a code-driven rotation, not a motion-warp window that each block clip must author.
- The legacy 70° rule is gone (merged in #136).
- A guard can now break (3.5).
- An interrupting hit from outside the arc, or an unblockable one, ends the guard for that hit and brings it back at once
  while Block is held [D-20261007-25]. Today guard stays up through such a hit (`DefenseResolver.cpp:337-341`).

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
- D-20261007-38: the window is the attacker's, held as data on the attack with a slot diff; the montage notify is a
  derived mirror. This amends the "on the attacker's animation" of D-20261005-09 and the project's working rule
  "ParryWindow is on the attacker's montage", which becomes "the parry window is the attacker's, held as data";
- D-20261007-36: every window is anchored to sequence time plus segment;
- D-20261007-24: the predicted arrival is the contact predictor's "when" (3.15);
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
"lands first" compares. It is the "when" of the contact predictor (3.15) [D-20261007-24]. **The method below is that
predictor's; what the decision does not fix is proposed:**
- The attack's data gives the swing's timeline: the Active phase start and, when the attack carries one, the contact
  marker, both data windows (4.5) [D-20261007-38]. Today the parry window's end stands in for that frame; the contact
  marker takes over, with the Active start as the fallback.
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
- **Home.** The window is the attacker's, held as data on the attack with a slot diff, anchored to sequence time plus
  segment and resolved to montage time on load or cook (4.5, 4.13) [D-20261007-36, D-20261007-38]. The montage's
  parry-window notify is a derived mirror drawn by "export to montage"; the runtime never reads it (6.5). D-20261005-09's
  "on the attacker's animation" is read through D-20261007-38: the window belongs to the attacker's attack, and the
  animation shows it.
- **Form (R-20261005-01, proposed; the owner may veto the form, not the home).** The window is authored as a **lead
  time** L_a before predicted arrival: a length only, never a start and an end, so an inverted window cannot be
  authored. The timeline (5.11) drags the lead; the end stays pinned to the contact marker.
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
  floor raises it to L = 0.35 s, so the window covers the attack from its first frame to its arrival. "Adopt from
  montage" (7.3) reads that notify into the attack as the 0.10 s lead, once.

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
   - ω is capped at the fastest natural turn, ω_max: a per-weapon cap on the defense set with a combat-rules default,
     **720°/s** to start, chosen from a rendered sweep [D-20261005-75].
   - With turn-deflect clips [D-20261004-30], the clip's play rate adapts the same way, under the same cap. Until those
     clips exist, the code-driven turn covers off-front parries [D-20261004-23].
   - **When θ ÷ ω_max > T**, so that even the fastest natural turn cannot face the attacker in time, the attacker's
     animation is slowed [D-20261005-10], as the safety valve [D-20261005-50]:
     - the slowdown **eases in and out**: a curve from 100% down to the needed speed and back [D-20261005-76];
     - its lowest speed s is no less than 0.5, and its whole duration d is no more than 0.3 s [D-20261005-33];
     - the time it buys is the area under the curve: ∫(1 − s(t)) dt over d. With the eases it is less than the
       0.3 × (1 − 0.5) = **0.15 s** a flat slowdown would buy; how much less depends on the ease shape;
     - the ease shape (the ease-in and ease-out lengths within d) is a combat-rules tunable, tuned on captures
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
| Minimum parry window | 0.35 s before arrival | combat rules | D-20261005-09 | none; the one shipped window is 0.10 s |
| Window lead time | per attack, as data, with a slot diff; the lead-time form is proposed | the attack; the montage notify is a derived mirror | D-20261005-09, D-20261007-36, D-20261007-38, R-20261005-01 | a start and end on the montage, read by the runtime |
| Window scale (parry assist) | 1.0 | reaction profile, parry assists | D-20261005-09, D-20261005-78 | none |
| Input buffer, contact grace | 0, 0 | reaction profile, parry assists | D-20261004-06, D-20261005-78 | none |
| Slow motion on window, telegraph, always perfect parry | off, on, off | reaction profile, parry assists | D-20261004-07, D-20261005-78 | none |
| Parry lockout | 0.25 s, after a miss only; lenient: a press during it is honoured if the window is still open when it ends; to be felt first, and it may go to zero | combat rules | D-20261005-06, D-20261005-32, D-20261005-72, D-20261007-18 | none: a failed press is a free guard |
| Attacker slowdown, lowest speed | 50% | combat rules | D-20261005-33 | none |
| Attacker slowdown, longest time | 0.3 s | combat rules | D-20261005-33 | none |
| Attacker slowdown, ease shape | tuned on captures (proposed) | combat rules | D-20261005-76 | none |
| Fastest natural turn ω_max | 720°/s, from a sweep | combat rules default; defense set override | D-20261005-75, D-20261005-68 | guard facing turns at 180°/s |
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
- **Home:** the window is attack data, anchored to the animation, and the runtime reads it from the resolved attack spec;
  today it is a notify state on the montage and the runtime reads the notify. The project rule "ParryWindow is on the
  attacker's montage" becomes "the parry window is the attacker's, held as data" [D-20261007-38]; `CLAUDE.md` follows
  with the pull request.
- **Arrival:** the contact predictor's "when", with a confidence (3.15) [D-20261007-24].

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
- D-20261007-33: a Heavy press with a Staggered target is the follow-up with the heavy opener at ×1.5; the charge is
  ignored and the hold does not open; lethal becomes a paired finisher;
- D-20261007-41: the single-character counter and the follow-up strike land as scripted hits at the clip's contact
  marker; the counter applies no hit-stun or push, and a follow-up carries its attack's;
- D-20261007-43: the readings on the kill sequence's home and on D-20261005-54's middle step stand;
- D-20261007-31: a list of finishers per direction, with the variety rule;
- D-20261007-22 and D-20261007-23: finisher coverage is authoring; an uncovered direction is fitted to a clip (3.8);
- D-20261007-46: a parry-counter of one enemy, or a follow-up or finisher on one, transfers the lock to it (3.13);
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
| Follow-up strike (optional press 2, while the enemy is Staggered) | The ×1.5 strike: a paired counter clip when one matches the direction, otherwise the fallback strike as a scripted hit; the enemy recovers. | A paired finisher by 3.8: a clip from the direction's list, else a fit to the nearest authored direction; an ordinary attack only when fitting fails, and then it kills only if it lands. |

**The sequence, in order:**

| # | Step | Rule |
|---|---|---|
| Seq-1 | **The press.** A Block press passes 3.2, and the parried attack is consumed for every target. In lock mode the lock transfers to the parried enemy (3.13). | D-20261005-24; D-20261007-46; consumption as today **[reported: `CombatComponent.cpp:1997-2069`]** |
| Seq-2 | **Content.** The attacker's direction (front, left, right or behind: today's four-way classifier, `CombatMath.h:53-56`, the dominant axis winning, so each direction spans 90°; keeping those boundaries is proposed), the parried attack's swing (its `Swing` field, 4.5), the variety rule and the room check pick one counter entry and the content it will play (below). | D-20261005-45, D-20261005-84, D-20261005-30, D-20261005-64, D-20261006-03 |
| Seq-3 | **Damage.** The counter's damage is the chosen clip's override when it has one, else the entry's value, else the combat rules' default of **50** (the heavy attack's damage). Resolved damage is that value times the weapon multiplier and the attacker's resistance (2.2). | D-20261006-03, D-20261005-80 |
| Seq-4 | **Outcome.** Lethal if the resolved damage is ≥ the attacker's current health. Decided now, at the press, so the right content starts at once. | D-20261005-38, D-20261005-40, D-20261006-03 |
| Seq-5 | **The turn and the parry.** The defender turns (3.2, "On success", item 4) and the deflect meets the blade at the predicted contact. | D-20261005-05, D-20261005-73 |
| Seq-6 | **The counter's contact.** The damage applies here, in full. It is never clamped: a counter is not chosen on a lethal press and then held back. A paired counter lands it at its sync moment (4.12). The single-character fallback strike lands it as a **scripted hit** at the clip's contact marker, with no weapon trace: the predictor already confirmed the target. The counter applies no hit-stun and no push; Staggered follows at the cancel notify. | D-20261006-05, D-20261007-41 |
| Seq-7a | **Lethal.** The enemy dies at that contact; its death outcome takes over from its victim half of the clip (3.11), with today's death-handling flag (**[reported]** `HitReactionComponent.cpp:1518-1533`). When the entry's counter carries an authored kill sequence, that plays instead, from the press. No lethal variant is required. | D-20261007-07 |
| Seq-7b | **Not lethal.** The enemy becomes Staggered at its own cancel notify, for the parried attack's Staggered time (3.4); the rest of its clip is its Staggered reaction. | D-20261005-81, D-20261006-02, D-20261007-07 |
| Seq-8 | **Protection.** Each character is untouchable until its own cancel notify (3.0): the player is free and hittable from theirs; the enemy is open from its own. | D-20261007-08 |
| Seq-9 | **The player's cancel notify.** From it the player is fully free: move, guard, attack anywhere, be hit. Attack presses made inside the sequence are buffered and fire now, last input wins. | D-20261005-62, D-20261004-03, D-20261007-09 |

Others hold throughout the sequence [D-20261005-21].

**Choosing the content (Seq-2).** From the defender's kit's defense set, its counter table [D-20261005-24,
D-20261005-48, D-20261007-57]. The evaluator (4.16) judges the rows against the attacker's direction, the parried
attack's swing and lane, and records the rows it evaluated. The fallback order is D-20261005-30's, which D-20261005-45
confirms:
1. a row for this direction and this swing. A row that also names the parried attack's lane beats one that does not:
   "lane preferred" [D-20261005-45, D-20261010-01];
2. a row for this direction and any swing;
3. a row for any direction, warped to fit;
4. the fallback strike of the row chosen by steps 1 to 3, with the enemy's Staggered reaction [D-20261005-23]. Each
   row carries its own fallback strike, per direction and swing [D-20261005-48]; an empty counter table fails
   validation (4.6).

When the chosen row seeks a paired counter, the counter is **found by key** in the same kit's paired set: its counter
rows matching the row's direction and swing, lane preferred, with the variety rule among several; nothing references
it [D-20261010-01]. A row that opts out of a paired counter plays its fallback strike at once. The lookup is one more
pass of the same evaluator, and its rows appear in the same trace (5.12).

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
3. the combat rules' default, **50** [D-20261007-53].

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
| FollowUp-4 | **The strike.** A light press plays the light opener as the follow-up, from the initial press: the hold window does not open, and a directional light never follows up [D-20261007-17]. A Heavy press plays the heavy opener as the follow-up, from the initial press: the charge is ignored and the hold does not open [D-20261007-33]. One rule for both buttons. | — |
| FollowUp-5 | **Damage:** ×1.5 the resolved damage of the attack the press resolves to [D-20261005-39, D-20261005-82]. Lethal is decided at the press [D-20261005-38, D-20261005-39, D-20261005-40]. | — |
| FollowUp-6 | **Lethal:** a paired finisher by 3.8: a clip from the attacker's direction's list, chosen by the variety rule; a direction with no clip is fitted to the nearest authored one; only when fitting fails does the strike play as an ordinary attack, and then it kills only if it lands [D-20261005-47, D-20261007-22, D-20261007-23, D-20261007-31]. **Not lethal:** a counter row of the attacker's kit's paired set for the target's direction (any swing), found by key, when one exists; otherwise the fallback strike of the attacker's defense set's row for that direction, at ×1.5 [D-20261005-43, D-20261005-48, D-20261005-57, D-20261010-01]. The unpaired strike is a scripted hit at the clip's contact marker, and it applies the hit-stun and push of the attack the press resolved to [D-20261007-41]. Staggered ends immediately, and there is one follow-up per parry-counter [D-20261005-51]. | — |

- **A press inside the counter sequence** is buffered and fires as the follow-up the moment the player's cancel notify
  fires; last input wins [D-20261007-09]. The hold is checked at the fired attack's hold-window start as usual, so a
  button held through the sequence and released after it yields a directional light or a charged heavy [D-20261007-45];
  against a Staggered target the hold does not open [D-20261007-17, D-20261007-33].
- **Lock mode.** A follow-up or finisher on one enemy transfers the lock to it, and the strafe re-centres (3.13)
  [D-20261007-46].
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
  thrust), one defense-set row and the paired-set counter row its key reaches: one deflect flowing into a counter that
  can kill. An authored kill sequence is optional, chosen at the press when the counter has one; it is never a separate
  finisher clip played after a non-lethal counter [D-20261005-38, D-20261005-45, D-20261005-48, D-20261005-57,
  D-20261007-07].
- **Turn-deflects.** 90° left and right, and 180° behind, each flowing into a front pair [D-20261004-30]. The decision
  says the 90° turns are mirrorable, but mirroring is not possible in the project today: there is no mirror data table
  and no Mirror node **[reported, from the turn speed sweep]**. Under rules before content, that is met with tooling,
  not a narrower rule [D-20261006-04].
- **The single-character strike**, and the enemy's Staggered reaction [D-20261005-23].
- **The finisher pool**, a list of paired finishers per attacker direction in the paired set [D-20261005-47,
  D-20261005-57, D-20261007-31]. Coverage of every direction is an authoring requirement, not a rule; an uncovered
  direction is fitted to a clip (3.8) [D-20261007-22]. There are no body types [D-20261005-47].
- **A contact marker** on the fallback strike and on every follow-up strike's clip, where the scripted hit lands
  [D-20261007-41].
- **A cancel notify** on every clip above (3.0, 4.13) [D-20261006-02].

What is owned today is a content finding and sits with the stage that builds this (9.8).

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
- D-20261007-26: while Staggered a character's armor is 0, whatever its base;
- D-20261007-45: a buffered press honours the hold when it fires;
- D-20261007-48: a lock ends when the player is Staggered;
- D-20261007-51: the dodge escapes it after the minimum (3.14);
- D-20261007-27: the guard-break push has its own default;
- D-20261005-40: the glossary.

**Causes.** Only these two [D-20261005-40]:
1. a non-lethal parry-counter (3.3);
2. a guard break (3.5).

**Duration.** The Staggered time of the attack that caused it:
- one default for every attack, **1.5 s**, in the combat rules [D-20261007-53];
- overridable per attack, and per slot (4.2) [D-20261005-08, D-20261005-31].

**The player's length is the player's own:** **0.6 s** by default, on the player's reaction profile (4.8)
[D-20261005-88]. It always applies to the player: an attack's or a slot's Staggered-time override never changes it
[D-20261006-06]. Enemies keep the 1.5 s default and take the causing attack's override. The rule reads the profile
field, not the character's identity: any character whose profile sets its own Staggered time keeps it, and the player is
the one that does [D-20261007-54].

For a parry-counter, the causing attack is the parried attack. For a guard break, it is the blocked attack that broke the
guard; that is a reading (Appendix B).

**Start.** For a parry-counter, at the enemy's own cancel notify on its half of the counter: that is where "the counter
sequence ends" [D-20261005-81, D-20261006-02, D-20261007-07]. For a guard break, at the breaking contact; that is a
reading.

**Clock.** Q8.

**While an enemy is Staggered:**
- it starts no attack and does not move: it is held [D-20261004-03]. Today the AI cannot start an attack while staggered
  (`EnemyCombatAIComponent.cpp:1140-1152`); its attack token returns (3.9) [D-20261007-30];
- its armor is 0, whatever its base, so every hit interrupts [D-20261007-26];
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
- the guard-break reaction plays, and the guard-break push carries the player (3.5, 3.10) [D-20261005-59,
  D-20261007-27];
- there is no attack, no guard and no stick movement until it ends;
- the player's armor is 0 [D-20261007-26], and a lock ends (3.13) [D-20261007-48];
- **a dodge press ends it early**, when the character's profile allows the dodge escape, as the player's does
  [D-20261007-54], by the same rule as the hit-stun escape (3.7): never before the 0.12 s floor and never
  within the first half, so the escape opens at max(0.12 s, S ÷ 2) after the breaking contact, where S is the player's
  Staggered length; that is 0.3 s at the default. The escape dodge is itself committed. A dodge press made before the
  escape opens is buffered and fires when it opens;
- **dodging out ends the Staggered state itself**, so the enemy's opening closes with it: the dodge reads as ending the
  stagger, and nothing of the state outlives it;
- otherwise it ends when the player's Staggered length runs out;
- attack and Block presses made meanwhile are buffered and fire afterwards. That carries over from D-20261005-67 and is a
  reading (Appendix B). A buffered Light or Heavy press starts its attack when the state ends, and at that attack's
  hold-window start the button state is checked as usual: holding through and releasing after yields a directional light
  or a charged heavy [D-20261007-45];
- a marked enemy may choose a finisher as its next attack while the player is Staggered, when the predictor says it lands
  (3.8) [D-20261007-20].

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
| Staggered time | 1.5 s, the same for every attack | combat rules; per-attack and per-slot overrides | D-20261005-08, D-20261005-31, D-20261007-53 | 1.5 s on the **attacker's** defense configuration, plus 1.5 s on the AI component, plus a 1.5 s constant |
| The player's Staggered time | 0.6 s; always the player's own, never an attack's or a slot's override | the player's reaction profile, as a field any profile may set | D-20261005-88, D-20261006-06, D-20261007-54 | none: nothing Staggers the player today |
| Staggered dodge escape (player) | opens at max(0.12 s, half the player's Staggered time) | rule, no tunable | D-20261005-87, D-20261005-88 | none |
| Dodge escape allowed | on for the player; off in the base profile (a reading, Appendix B) | reaction profile | D-20261005-87, D-20261005-88, D-20261007-54 | none |

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
- D-20261005-59: the breaking hit delivers a push, not the block shove; D-20261007-27 gives that push its own default;
- D-20261007-27: the guard-break push has its own per-type default, to be tuned, with the hit push's overrides;
- D-20261007-25: a hit from outside the arc, or an unblockable one, is not a block; it ends guard for the hit (3.1);
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
| Break-2 | The defender's reaction profile has guard-break counting switched on [D-20261005-46, D-20261007-54]. | No guard break: a profile with counting off breaks only to guard breakers. The player's profile has it off, so the player's guard breaks only to guard breakers [D-20261005-14, D-20261005-40]; the rule reads the toggle, never the identity [D-20261007-54]. |
| Break-3 | Counting is on. The attack's resolved pressure is added to the guard-break count at the contact time t [D-20261005-25]. The window is sliding: at each blocked hit the count is the sum of the pressure of blocked hits in the last W = 3 s [D-20261005-83]. | If the count ≥ N: **guard break** [D-20261005-26]. |

- **N** is 4 pressure points and **W** is 3 s by default. Both are set per character, through the base profile and
  per-character overrides [D-20261005-15, D-20261005-26].
- **Counting is a per-character toggle** in the reaction profile: on in the base profile, off in the player's, so the
  player's guard breaks only to guard breakers [D-20261005-14, D-20261005-46, D-20261005-56]. There is no is-player
  branch: the player is the character whose profile has counting off, and a shipped-content check asserts that the
  player's profile does, as a decided rule, not a pinned tunable (8.1) [D-20261007-54].
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
- **The breaking hit delivers the guard-break push**, not the block shove: its own per-type default in the combat
  settings, to be tuned on captures, with the hit push's per-attack and per-slot overrides (3.10) [D-20261005-59,
  D-20261007-27].
- **The guard-break count clears** [D-20261005-83].
- **No enemy paired finisher** comes from it by itself; if a marked enemy's next hit on the Staggered player would be
  lethal, the normal lethal rule still applies [D-20261005-20, D-20261005-86] (3.8).

**What does not count:**
- unblockable hits, hits outside the arc and hits while not guarding, none of which is a block;
- parries.

**Who it applies to.** Any character whose guard is up. Enemies do not guard today **[reported]**. Enemy guarding is not
designed, and guard-breaker enemy types come later [D-20261004-27].

**Built dormant.** The counted guard break (Break-3) is built with the guard-break stage (9.6) and proven on fixtures
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
| Guard-break push | its own per-type default, to be tuned on captures | combat settings → set → attack → slot | D-20261005-59, D-20261007-27 | none |

### 3.6 Interruption and hit look

**Decisions:**
- D-20261004-42: the attack type decides the look, and pressure decides interruption;
- D-20261005-16 and D-20261005-17: pressure against armor;
- D-20261005-44: current armor is the higher of the base and any active window;
- D-20261006-02: the reaction's cancel notify;
- D-20261007-19: the contact order within a frame;
- D-20261007-25: an interrupting hit on a guarding character ends the guard, which returns at once while Block is held;
- D-20261007-26: armor in the locked states, and super armor;
- D-20261007-38: an attack's own armor windows are attack data;
- D-20261007-41: scripted hits;
- D-20261005-40: the glossary.

**Scope.** Hits that land. Not blocks, not parries, and not damage applied by a paired animation. A scripted hit (3.3)
has its own rule: the counter applies no hit-stun and no push, and a follow-up strike applies those of the attack the
press resolved to [D-20261007-41].

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
    0 while the victim is Staggered, whatever its base [D-20261007-26];
    otherwise max(base armor from the reaction profile (default 0),
                  the level of any active armor window on the victim's animation) [D-20261005-44]
```

**Armor in the locked states** [D-20261007-26]:
- **Staggered:** armor is 0, whatever the base, so every hit interrupts.
- **Hit-stun:** the base applies, and any active window with it.
- **Super armor** is an armor window whose level is above any pressure: the character is never interrupted, and the
  flinch still plays.

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
- applies knockback (3.10);
- ends a guard it reached from outside the arc or through an unblockable flag; the guard returns at once while Block is
  still held, never as a parry press (3.1) [D-20261007-25].

**Contact order within a frame** [D-20261007-19]. Several weapon contacts can land in one frame: a trade, or two
attackers. In that frame:
1. **All contacts are judged against the state at the frame's start.** A trade lands both hits: neither hit's reaction
   cancels the other's attack before its contact is judged.
2. **Damage applies first**, for every contact.
3. **A killing hit replaces the reaction:** no hit-stun, no push; the death plays.
4. **Among the surviving interrupting hits on one victim, the highest pressure, then the latest, supplies the reaction,
   the hit-stun and the push.** "Latest" is the later contact time within the frame (a reading, Appendix B). The other
   hits still deal their damage and count their pressure.

**A hit that does not interrupt:**
- plays the additive flinch over the current action, which continues [D-20261004-42];
- applies its damage;
- applies **no** hit-stun, which comes only "when a hit interrupts" [D-20261005-34];
- applies **no** knockback [D-20261005-58].

**Removed:**
- the damage ÷ health rule, `HeavyDamageHealthPercent`. Today, at shipped values, every weapon hit plays a heavy reaction
  **[reported]** [D-20261004-42].
- per-phase resistance from D-20261003-02, which armor windows replace (Appendix B).

**Armor windows** are authored on a character's animations and carry a level [D-20261005-17]. On an attack's own
section they are attack data, anchored like every other window and drawn on the montage as derived notifies (4.5, 4.13)
[D-20261007-38]. On a character's other animations, the reactions and the dodge's clip, the proposal stays a notify
state carrying the level, because no attack data owns those clips (a reading, Appendix B). The dodge's i-frame window is
invulnerability, not armor (3.14).

**Unchanged:** the invulnerability and i-frame flags (3.11). Super armor is now defined above [D-20261007-26].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Pressure | light 1, heavy 2, charged 3 | combat settings type defaults; attack and slot overrides | D-20261005-17 | none: every hit that plays a reaction can interrupt |
| Base armor | 0 | reaction profile | D-20261005-17 | none |
| Armor window level | authored on each window | attack data for an attack's own windows; the animation otherwise | D-20261005-17, D-20261007-38 | none |
| Armor while Staggered | 0 | rule, no tunable | D-20261007-26 | none |

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
- D-20261006-02: the hit reaction's cancel notify;
- D-20261007-25: a guard the hit ended returns at once while Block is held, never as a parry press;
- D-20261007-26: the base armor applies in hit-stun;
- D-20261007-32: a push in flight runs under the character; only a dodge cancels it;
- D-20261007-45: a buffered press honours the hold when it fires;
- D-20261007-51: the dodge escapes hit-stun after its minimum (3.14);
- D-20261007-20: a marked enemy may choose a finisher against a player in hit-stun.

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
- guard can be raised (3.1), and that press can parry [D-20261005-77]. A guard that the interrupting hit ended returns at
  once while Block is still held, and that return is not a parry press [D-20261007-25];
- the base armor applies, with any active window [D-20261007-26];
- a push in flight continues under the character; attack, guard and movement input run on top of it, and only a dodge
  cancels it (3.10) [D-20261007-32];
- input is buffered and fires afterwards [D-20261005-37, D-20261003-03]. The buffer is today's last-input-wins queue. A
  buffered Light or Heavy press starts its attack when hit-stun ends, and at that attack's hold-window start the button
  state is checked as usual, so holding through hit-stun and releasing after it yields a directional light or a charged
  heavy [D-20261007-45];
- **a dodge press ends the player's hit-stun early** [D-20261005-87], when the character's profile allows the dodge
  escape, as the player's does [D-20261007-54]: never before the 0.12 s floor, and never within the
  first half of the stun, so the escape opens at max(0.12 s, H ÷ 2) after the hit. The escape dodge is itself committed:
  it plays through. A dodge press made before the escape opens is buffered and fires when it opens. The dodge is 3.14
  [D-20261007-51]; today the Evade input does nothing (`CombatComponent.cpp:4147-4149` **[reported]**);
- a marked enemy may choose a finisher as its next attack against a player in hit-stun, when the predictor says it lands
  (3.8) [D-20261007-20].

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
| Hit-stun floor | 0.12 s | combat rules (Q23) | D-20261005-34 | none |
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
- D-20261005-47: keyed by direction only, no body types; its "normal death when no clip matches" is amended by
  D-20261007-22 and D-20261007-23: an uncovered direction is fitted first;
- D-20261005-57: the finishers live in the kit's paired set; the combo set's reference to them is dropped by
  D-20261010-01;
- D-20261007-55: the marker: a paired set that holds player-finisher content can finish the player;
- D-20261010-01: finishers are found by key from the attacker's kit's paired set;
- D-20261010-04: one finisher row is one content asset; its alternates never decide the direction;
- D-20261007-20: an enemy's finisher is chosen at its attack's start, against a player who is Staggered or in hit-stun,
  when the predictor says the hit lands; never a mid-swing promotion;
- D-20261007-22: coverage of every direction is authoring; "will it hit" is predicted; fitting before an ordinary attack;
- D-20261007-23: fit to clip, with budgets on the paired set and procedural blending at the seam;
- D-20261007-24: the contact predictor (3.15);
- D-20261007-31: a list of clips per direction, chosen by the variety rule, with no throttle;
- D-20261007-42: per-clip ground state and height requirements; the victim is never invulnerable;
- D-20261007-46: a finisher on one enemy transfers the lock to it (3.13);
- D-20261005-40: the glossary.

**Rule.** A paired finisher plays whenever an aimed strike's resolved damage against its target is lethal and the
strike is predicted to land: a parry-counter, a follow-up strike on a Staggered enemy, or an ordinary attack. That is
decided at the press [D-20261005-38, D-20261005-39, D-20261005-42, D-20261007-22]. Staggered on its own does not grant
one [D-20261005-39]. No health percentage is involved [D-20261005-18]. Coverage of every direction is an authoring
requirement, not a rule: where a direction has no clip the situation is fitted to a clip, and only when fitting fails
does an ordinary attack play, which kills only if it lands [D-20261007-22].

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
| Trigger-4 | **Lethal check:** resolved damage ≥ the target's current health, and the contact predictor says the strike reaches the target, at the lethal-strike threshold (3.15): "will it hit" is predicted, never guessed [D-20261007-22, D-20261007-24]. For a Heavy press, Q4. | The strike plays normally. |
| Trigger-5 | **Content.** The paired finisher comes from the attacker's kit's paired set, asked by key for the finisher rows of the attacker's direction relative to the victim (front, left, right or behind) and nothing else; no set references them [D-20261005-47, D-20261005-57, D-20261010-01]. Each direction's rows are a **list** of clips, one content asset per row [D-20261010-04]; one is chosen by the variety rule (one always plays; two alternate; three or more are random, excluding the last two played), and there is no throttle [D-20261007-31]. A direction with no clip is **fitted** to the nearest authored direction (below) [D-20261007-22, D-20261007-23]; the editor lists each paired set's uncovered directions [D-20261007-23]. | Fitting fails: an ordinary attack plays, and it kills only if it lands [D-20261007-22]. |
| Trigger-6 | **Start checks.** Today's checks stay: a clear path (a 30 cm sweep), and neither character already in a paired animation (**[reported]** `PairedAnimationComponent.cpp:3863-3998`) [D-20261005-64]. The clip's own requirements are checked with them: the ground state it declares for each role, the height-difference limit, and that the victim is not invulnerable [D-20261007-42]. In lock mode the lock transfers to the victim [D-20261007-46]. | A failed check counts as fitting failed: an ordinary attack plays, and it kills only if it lands [D-20261007-42]. |

**Fit to clip** [D-20261007-22, D-20261007-23]. When the attacker's direction has no finisher clip:
1. the attacker's approach warp brings the pair to the **nearest authored direction**, and the victim may turn within a
   small budget;
2. the turn, distance and room the fit may use are **budgets**: tunables on the paired set with combat-rules
   defaults (4.7, Q23). Beyond them fitting fails, and an ordinary attack plays [D-20261007-22];
3. the first part of the clip is **blended procedurally**, the upper body and the blade toward the victim, to hide the
   approach. It is verified on a rendered check;
4. the budgets' values are not in the decision; they are tunables with no decided value, set on the captures that verify
   the blend (3.12).

The owner's words: "we should have finishers for all directions that is an authoring issue. in order to get around our
current content issue of finishers being authored from in front and behind but not the sides, we should use procedural
techniques to make the situation fit the finisher or another technique that could provide better fidelity"
[D-20261007-22].

**What a clip requires** [D-20261007-42]. Paired content declares, per role (attacker and victim), the ground state it
needs: grounded, airborne or any, with both grounded by default; and a height-difference limit with a combat-rules
default (Q23). The victim must never be invulnerable. A mismatch counts as fitting failed, so an ordinary attack plays. Air
content later declares airborne; the defaults keep today's behaviour. These live on the paired content and are read
through the paired-content interface (4.12), never copied into a table row [D-20261010-04].

**During a paired finisher:**
- others hold, and a second attacker whose swing is in progress aborts and is shoved back (3.9) [D-20261005-21,
  D-20261007-29, D-20261007-44];
- the victim's death is in the clip.

**Enemy paired finishers on the player** [D-20261005-20, D-20261007-20]. All of these must hold:
- the enemy's kit's paired set holds player-finisher content: that is the marker, and validation checks it; the reaction
  profile no longer carries one [D-20261007-55];
- the enemy's **next** attack would be lethal to the player [D-20261005-42];
- the player is **Staggered or in hit-stun** at that attack's start, and the contact predictor says the hit lands, at
  the finisher-choice threshold (3.15) [D-20261007-20, D-20261007-24];
- the enemy **chooses the finisher as its next attack, at that attack's start**. An ordinary swing already in progress
  never becomes a finisher: there is no mid-swing promotion, whatever the player's health [D-20261007-20]. The owner:
  "enemies shouldnt be transitioning between normal combo attacks into finishers thats a recipe for chaos";
- a guard break grants nothing by itself [D-20261005-20, D-20261005-86]; a marked enemy's lethal hit on the Staggered
  player finishes only through the choice above, which narrows D-20261005-86's "still finishes" (Appendix B);
- the enemy is within 350 cm [D-20261005-28];
- the enemy's finisher counts as its attack for the tokens (3.9) [D-20261007-30], and others hold during it
  [D-20261007-29].

**There is no "can be finished" marker.** Health alone decides, with no per-character exemption. A victim whose direction
has no clip is fitted to the nearest authored one, and only a failed fit falls back to an ordinary attack
[D-20261005-38, D-20261005-47, D-20261007-22]. There are no body types [D-20261005-47].

**A marked enemy** is one whose kit's paired set holds player-finisher content [D-20261007-55]; the word is kept for
brevity, and nothing on the enemy's profile carries it.

**Removed from today:**
- the 25% low-health threshold (`HitReactionComponent.cpp:1423`);
- hit-stun and Staggered as routes on their own;
- choosing the one best soft-aim target in the character's facing direction, at 500 cm and 120°
  (**[reported]** `PairedAnimationComponent.cpp:2777-2799`). This becomes the aim cone of 3.13, around the stick
  direction or the facing, at 350 cm for a paired finisher;
- finisher data on every attack, replaced by the finisher pool;
- every shipped enemy attack carrying lethal finisher data **[reported]**, replaced by kits whose paired set holds
  player-finisher content [D-20261007-55];
- the normal death as the first answer to a missing clip: fitting comes first [D-20261007-22].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Aim cone (half-angle) | 60°, around the stick direction or the facing (3.13) | combat rules | D-20261005-27, D-20261007-12 | soft aim from facing: 120° rejection, scored on 45° |
| Finisher range | 350 cm | combat rules | D-20261005-28 | 500 cm, `SoftAimRange` |
| Can finish the player | no: the kit's paired set holds no player-finisher content | the kit's paired set, as content presence; validation checks it (Q24) | D-20261005-20, D-20261005-40, D-20261007-55 | every enemy attack can |
| Finisher list, variety | the counters' rule: one plays; two alternate; three or more random, excluding the last two | rule, no tunable; no throttle | D-20261007-31 | one clip, no variety |
| Fitting budgets: turn, distance, room | no decided value; tuned on the captures that verify the seam blend | paired set, with combat-rules defaults (Q23) | D-20261007-23 | none: a missing clip means the normal death |
| Ground state per role | both grounded | the paired content | D-20261007-42 | implicit |
| Height-difference limit | no decided value; "defaults keep today's behaviour" | combat rules default (Q23); the paired content may override | D-20261007-42 | **[reported]** none explicit |
| Lethal-strike predictor threshold | Q20 | combat rules | D-20261007-22, D-20261007-24 | none |

### 3.9 Several attackers

**Decisions:**
- D-20261005-35: up to 2 attackers;
- D-20261005-21: others hold;
- D-20261005-36: lands first;
- D-20261007-08: untouchable until your own cancel notify;
- D-20261007-29: what "in progress" means, combo continuations, unstarted tokens, the resume delay, and the scope: every
  paired sequence involving the player;
- D-20261007-30: when a token is held and returned, the cooldown's start, the re-grant delay, and the enemy's finisher
  as its attack;
- D-20261007-44: a second enemy whose swing is in progress aborts and is shoved back; it supersedes the "swings already
  started finish, contact ignored" part of D-20261007-29, D-20261005-21's "swings already in progress finish" and the
  earlier reading 47;
- D-20261004-01: enemy coordination comes after the player's combat mode.

**Attack tokens** [D-20261005-35, D-20261007-30]:
- At most **N = 2** attackers hold a token against one target at once by default. The tokens count the attackers of
  whoever is targeted, not of the player as such; no rule here asks who the target is [D-20261007-54]. N is
  configurable, in the combat rules [D-20261005-35, D-20261007-53]. Today N is 1, global and C++ only
  (`CombatTokenSubsystem.h:41`).
- **A token is held from its grant until the attack ends for any reason:** finished, interrupted, parried, recoiled,
  the enemy's death, or the enemy Staggered [D-20261007-30]. A token granted to an enemy that has not started its attack
  when others hold begins is returned [D-20261007-29].
- **The cooldown starts when the token returns** [D-20261007-30]. Each enemy has **a short cooldown** between attacks
  [D-20261005-35]; its length is Q6. Today it is 1.0 s, counted from the grant (`CombatTokenSubsystem.h:45`).
- **A freed token is not granted again for a short settings delay**, the re-grant delay [D-20261007-30]. Its value is
  not decided.
- **An enemy's finisher counts as its attack** and holds a token [D-20261007-30].
- Waiting enemies queue first in, first out, as today.

**Others hold** [D-20261005-21, D-20261007-29, D-20261007-44]:
- While the player is in a paired sequence, a parry-counter, a paired finisher or an enemy's finisher on the player, no
  other enemy starts a new attack. The rule covers every paired sequence involving the player [D-20261007-29].
- **An attack is in progress once its montage has started, in any phase.** An enemy's combo continuation is a new attack,
  and it holds [D-20261007-29].
- **A second enemy whose swing is already in progress when the sequence starts aborts and is shoved back**, for spacing
  [D-20261007-44]: its attack is cancelled at that moment, with a recoil or abort reaction; it is pushed a short
  settings distance away from the sequence (the shove-back, default tuned on captures); its token returns; and it
  holds. The owner: "perhaps we should have them a second attacker abort and be pushed back a bit to give spacing for
  the paired sequence". Which clip "recoil or abort reaction" names is a reading (Appendix B): the reaction profile's
  recoil [D-20261005-55].
- **When the sequence ends, holding enemies re-enter the queue**, each after a per-enemy **resume delay**: a
  combat-rules default with a per-profile override [D-20261007-29, D-20261007-53]. Which profile is a reading (Appendix B).
- Today every enemy targeting the player aborts its swing without a push and with no resume delay **[reported]**.
- Protection stands beside this: inside a parry-counter neither character can be touched until its own cancel notify
  [D-20261007-08], and during a paired finisher the character that started it cannot be touched (3.11). With
  D-20261007-44 no in-progress swing reaches either character, because it has aborted. D-20261005-36's fresh press for
  the other attack therefore applies before the sequence starts, or after the player's cancel notify.
- Whether other enemies hold during a non-lethal follow-up strike that is **not** paired is Q7; a paired follow-up is a
  paired sequence involving the player, so others hold during it [D-20261007-29].

**Two parryable attacks** [D-20261005-36]. A parry press answers whichever lands first (3.2, Parry-4). The other can be parried
at once with a fresh press, since a success starts no lockout [D-20261005-72], or blocked.

**Not in this spec.** Wider enemy coordination, such as attack slots and orbits, comes after the player's combat mode
[D-20261004-01].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Most attackers on one target at once | 2; the tokens count the attackers of whoever is targeted | combat rules | D-20261005-35, D-20261007-53, D-20261007-54 | 1, C++ only, counting the player's attackers |
| Per-enemy cooldown | length Q6; it starts when the token returns | combat rules | D-20261005-35, D-20261007-30 | 1.0 s from the grant |
| Re-grant delay | no decided value, "short" | combat rules | D-20261007-30 | none |
| Resume delay | no decided value; per enemy | combat rules default; per-profile override | D-20261007-29 | none: enemies abort and re-queue at once |
| Shove-back distance (second attacker) | no decided value; tuned on captures | combat rules (Q23) | D-20261007-44 | none |

### 3.10 Knockback

**Decisions:**
- D-20261004-38, D-20261001-07, D-20260930-02 and D-20260930-01: the layering, the code seeds, per-attack distance and
  duration, and the victim's scale;
- D-20261005-58: no push on a hit that does not interrupt;
- D-20261005-59: a block shove on a blocked hit, and a push on the hit that breaks a guard;
- D-20261005-60: normal pushes on victims that are Staggered or in hit-stun, and none during paired sequences;
- D-20261007-27: the block shove and the guard-break push each have their own per-type defaults, not derived from the hit
  push, with the hit push's overrides; it supersedes the 40% proposal below D-20261005-59;
- D-20261007-28: the shove's direction and the carried-over push behaviours are decided on rendered captures;
- D-20261007-32: a push in flight: only a dodge cancels it; other input runs on top of it;
- D-20261007-19: within a frame, the strongest interrupting hit supplies the push, and a killing hit applies none;
- D-20261007-41: a scripted counter applies no push; a scripted follow-up applies its attack's;
- D-20261007-44: the shove-back of a second attacker (3.9).

**Push by outcome:**

| Outcome | Push | Decision |
|---|---|---|
| Interrupting hit | The attack's resolved knockback: distance, duration, direction mode, speed profile and blend (3.6). Among several interrupting hits in one frame, the one with the highest pressure, then the latest, supplies it (3.6). | D-20261004-38, D-20261007-19 |
| Killing hit | None: the death replaces the reaction. | D-20261007-19 |
| Flinch (a hit that does not interrupt) | None. | D-20261005-58 |
| Blocked hit | The **block shove**: a short push with its own per-type default, Light 10 cm and Heavy 20 cm to start; its direction is open (Q21). | D-20261005-59, D-20261007-27, D-20261007-28 |
| The hit that breaks a guard | The **guard-break push**: its own per-type default, to be tuned, together with Staggered (3.5). | D-20261005-59, D-20261007-27 |
| A scripted counter (3.3) | None; Staggered follows at the cancel notify. | D-20261007-41 |
| A scripted follow-up strike (3.3) | The push of the attack the press resolved to. | D-20261007-41 |
| A second attacker's aborted swing (3.9) | The **shove-back**: a short settings distance away from the sequence, tuned on captures. | D-20261007-44 |
| A hit on a victim that is Staggered or in hit-stun | As the hit's outcome above: the full push if it interrupts. Being Staggered or in hit-stun changes nothing. | D-20261005-60 |
| Any contact during a paired sequence (a counter or a paired finisher) | None. The sequence's authored motion replaces it. | D-20261005-60 |
| Unblockable hit through guard | As an interrupting hit, if it interrupts (3.11). | today |

**Layering** [D-20261004-38]:
- **Per-type defaults** live in the attacker's combat settings [D-20261007-53]. Each attack overrides them field by field, and each slot overrides
  the attack [D-20261005-01]. The code values only seed a new settings asset. The combo set's type default (4.2) is the
  per-weapon layer noted for later [D-20261005-61].
- **Code seeds for the hit push:** Light 25 cm in 0.2 s; Heavy 20 cm in 0.25 s (`CombatSettings.cpp:10-21`)
  [D-20261001-07]. The owner's held settings asset sets other values (7.6).
- **Scaling.** Distance scales with charge and with the victim's `KnockbackScale` (`KnockbackResolution.cpp:30-38`)
  [D-20260930-01]. The block shove scales the same way (proposed).

**The block shove and the guard-break push** [D-20261005-59, D-20261007-27]:
- each has **its own per-type default** in the combat settings, not derived from the hit push, with the same per-attack
  and per-slot overrides as the hit push [D-20261007-27]. The earlier "40% of the type's hit push" proposal is
  superseded;
- **block shove distance, to start:** Light **10 cm**, Heavy **20 cm** [D-20261007-27]. The decision names distances
  only; the shove's duration has no decided value (Q21);
- **guard-break push:** "to be tuned" [D-20261007-27]; no starting value;
- **direction:** open. Whether the shove goes away from the attacker, along the attack's push direction or otherwise is
  decided on rendered captures, with the knockback scenarios (Q21) [D-20261007-28];
- both use the same displacement executor as the hit push (proposed).

**A push in flight** [D-20261007-32]:
- **only a dodge cancels the remaining push** (3.14);
- **attack, guard and movement input run on top of it**: a press is not refused because a push is running, and the push
  carries the character while the action plays;
- the editor **warns when an attack's hit-stun is shorter than its push** (4.4), since the victim would then act while
  still being carried.

**Carried over, open** [D-20261007-28]. Whether the existing push behaviours carry over unchanged, the stop at walls, the
ledge rule, replacement on a new push and the pause during hitstop, is decided on rendered captures; until then they
keep today's behaviour (3.11). The owner's pick: "D. Decide on captures"; the register marks it **open, evidence
pending**.

**Today:** a push starts only when a full reaction starts (`HitReactionComponent.cpp:491-500`), so interrupting hits
push, and nothing else does. The block shove and the guard-break push are new.

**Slopes** are verified by a rendered capture [D-20261004-40].

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Hit push | Light 25 cm / 0.2 s, Heavy 20 cm / 0.25 s (code seeds) | combat settings → set → attack → slot | D-20261004-38 | the same |
| Block shove | distance Light 10 cm, Heavy 20 cm to start; duration and direction open (Q21) | combat settings → set → attack → slot | D-20261005-59, D-20261007-27, D-20261007-28 | none |
| Guard-break push | own per-type default, to be tuned | combat settings → set → attack → slot | D-20261005-59, D-20261007-27 | none |
| Push on a flinch | none | — | D-20261005-58 | none (no reaction, no push) |
| Push on a killing hit | none | — | D-20261007-19 | a reaction and a push start, then death **[reported]** |
| Push during paired sequences | none | — | D-20261005-60 | none |
| Push cancelled by | a dodge only | rule, no tunable | D-20261007-32 | nothing cancels a push |
| Shove-back distance | no decided value; tuned on captures | combat rules (Q23) | D-20261007-44 | none |
| Walls, ledges, replacement, hitstop pause | today's behaviour until captures decide | — | D-20261007-28 | today's displacement rules |

### 3.11 What stays as today

No decision touches these, so they keep working as they do today. Each one can change only through a later decision.
- **Unblockable attacks** hit through guard at full damage, and guard stays up (`DefenseResolver.cpp:337-341`).
- **Block-interruptible attacks** recoil when blocked (`DefenseResolver.cpp:366-374`).
- **A block uses one of the swing's hit slots**, and the swing cannot touch that defender again.
- **Guard facing while guard is held** keeps today's threat selection and automatic turn. Guard aim is "direction, with
  magnetism" [D-20261004-02], whose design is queued and not part of this spec.
- **Lands-first ties** fall back to today's lower tie-breaks (3.2, Parry-4).
- **Invulnerability, and i-frames on reaction entries** are unchanged **[reported]**. Super armor is now defined by
  D-20261007-26 (3.6); a paired finisher's victim must never be invulnerable [D-20261007-42].
- **Damage resistance** is unchanged, and so are the rules for paired partners.
- **Being Staggered again** replaces the remaining time (`HitReactionComponent.cpp:1439-1466`).
- **Hitstop, sound and effect** are configured per attack, now inside impact feedback (4.5).
- **The death outcome** (ragdoll or frozen pose), and the flag that makes a paired finisher's clip the death animation,
  are unchanged **[reported]**.
- **Input buffering** uses the last-input-wins queue.
- **Direction classification** uses the four-way classifier.
- **The path check before a paired finisher** is a 30 cm sweep. It stays as a start check (Trigger-6); when it fails,
  fitting has failed and an ordinary attack plays [D-20261007-22, D-20261007-42].
- **Push behaviour in flight**: the stop at walls, the ledge rule, replacement on a new push and the pause during
  hitstop keep today's behaviour until the captures of D-20261007-28 decide (3.10).
- **Targeting today.** No lock-on, strafe or target cycling exists in source (the 2026-10-07 log, E9). Today's soft aim
  picks the one best target in the character's facing direction, at 500 cm and 120°, and serves paired finishers only.
  Free mode (3.13) replaces it, and lock mode is new [D-20261007-12, D-20261007-13].

### 3.12 Tunables at a glance

| Tunable | Default | Home | Per character? | Decision |
|---|---|---|---|---|
| Guard arc | ±75° | combat rules (Q23); defense set override | per weapon | D-20261005-11, D-20261005-70 |
| Block turn rate (the turn is always the full angle) | 360°/s | combat rules (Q23); defense set override | per weapon | D-20261005-69 |
| Minimum parry window | 0.35 s | combat rules | no | D-20261005-09, D-20261007-53 |
| Parry window scale | 1.0 | reaction profile, parry assists | yes | D-20261005-09, D-20261005-78 |
| Parry lockout | 0.25 s, lenient; to be felt first, and it may go to zero | combat rules | no | D-20261005-32, D-20261007-18 |
| Attacker slowdown | ≥ 50% speed, ≤ 0.3 s | combat rules | no | D-20261005-33 |
| Fastest natural turn | 720°/s | combat rules (Q23); defense set override | per weapon | D-20261005-75, D-20261005-68 |
| Other parry assists (input buffer, contact grace, slow motion on window, telegraph, always perfect parry) | 0, 0, off, on, off | reaction profile | yes | D-20261004-07, D-20261005-78 |
| Staggered time | 1.5 s (one default) | combat rules → attack → slot | per attack | D-20261005-08, D-20261005-31 |
| Follow-up strike multiplier | ×1.5 of the resolved attack's damage | combat rules (Q23) | no | D-20261005-39, D-20261005-82 |
| Counter damage | 50 (default); each defense-set row sets its own; a paired clip may override | combat rules → defense-set row → paired clip | per row | D-20261005-80, D-20261006-03 |
| Hit-stun dodge escape | opens at max(0.12 s, half the stun) | rule | no | D-20261005-87 |
| The player's Staggered time | 0.6 s, always the player's own | the player's reaction profile | yes | D-20261005-88, D-20261006-06 |
| Staggered dodge escape (player) | opens at max(0.12 s, half the player's Staggered time) | rule | no | D-20261005-88 |
| Guard-break threshold and window | 4 points within 3 s | reaction profile | yes | D-20261005-15, D-20261005-26 |
| Guard breaker | charged heavy and directional light: yes; others: no | combat settings → attack → slot | per attack | D-20261005-29 |
| Pressure | light 1, heavy 2, charged 3 | combat settings → set → attack → slot | per attack | D-20261005-17 |
| Base armor | 0 | reaction profile | yes | D-20261005-17 |
| Hit-stun | 0.25 / 0.45 / 0.6 s | combat settings → set → attack → slot | per attack | D-20261005-34 |
| Hit-stun floor | 0.12 s | combat rules (Q23) | no | D-20261005-34 |
| Aim cone | 60° either side of the stick direction, or of the facing when no direction is held | combat rules | no | D-20261005-27, D-20261007-12 |
| Finisher range | 350 cm | combat rules | no | D-20261005-28 |
| Lock range, and "near" for the automatic re-pick | Q19 | combat rules (proposed) | no | D-20261007-13, D-20261007-48 |
| Flick threshold (lock switch) | no decided value; tuned on captures (proposed) | combat rules (proposed, Q23) | no | D-20261007-13 |
| Can finish the player | content presence: the kit's paired set holds player-finisher content | the kit's paired set | per kit | D-20261005-20, D-20261007-55 |
| Guard-break counting toggle | base on; player off; no is-player branch | reaction profile | yes | D-20261005-46, D-20261005-56, D-20261007-54 |
| Dodge escape allowed | on for the player; off in the base profile (a reading) | reaction profile | yes | D-20261005-87, D-20261005-88, D-20261007-54 |
| Most attackers on one target at once | 2; the tokens count the attackers of whoever is targeted | combat rules | no | D-20261005-35, D-20261007-54 |
| Per-enemy attack cooldown | Q6 | combat rules | no | D-20261005-35 |
| Knockback (hit push) | Light 25 cm / 0.2 s, Heavy 20 cm / 0.25 s (code seeds) | combat settings → set → attack → slot | per attack | D-20261004-38 |
| Block shove | Light 10 cm, Heavy 20 cm to start; duration and direction open (Q21) | combat settings → set → attack → slot | per attack | D-20261005-59, D-20261007-27, D-20261007-28 |
| Guard-break push | own per-type default, to be tuned | combat settings → set → attack → slot | per attack | D-20261005-59, D-20261007-27 |
| Armor while Staggered | 0 | rule | no | D-20261007-26 |
| Token held until | the attack ends for any reason; cooldown from the return | rule | no | D-20261007-30 |
| Re-grant delay | no decided value, "short" | combat rules | no | D-20261007-30 |
| Resume delay (others hold) | no decided value | combat rules; per-profile override | yes | D-20261007-29 |
| Shove-back distance (second attacker) | no decided value; tuned on captures | combat rules (Q23) | no | D-20261007-44 |
| Finisher list, variety | the counters' rule; no throttle | rule | no | D-20261007-31 |
| Fitting budgets: turn, distance, room | no decided value; tuned on captures | paired set; combat-rules defaults (Q23) | per weapon | D-20261007-23 |
| Ground state per role; height-difference limit | both grounded; no decided value | the paired content; combat-rules default (Q23) | per clip | D-20261007-42 |
| Predictor tolerance and horizon | no decided value | combat rules; per-attack overrides | per attack | D-20261007-24 |
| Predictor thresholds (parry window lenient, finisher choice strict, lethal strike) and confidence scale | Q20 | combat rules | no | D-20261007-24 |
| Lock grace (out of range or out of sight) | no decided value, "short" | combat rules (proposed, Q23) | no | D-20261007-48 |
| Dodge distance, duration, i-frame window | no decided value; tuned on captures (Q22) | the dodge's own data; combat-rules defaults (Q23) | no | D-20261007-21, D-20261007-50 |
| Dodge direction mode (eight-way or free angle) | a content choice | the dodge's clips | — | D-20261007-49 |

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
- D-20261007-45: a buffered Light or Heavy press honours the hold at its attack's hold-window start when it fires;
- D-20261007-46: lock follows action: acting toward a different enemy transfers the lock to it, and the strafe re-centres;
- D-20261007-47: while locked the stick aims a directional light as in free mode; with no enemy that way the lock breaks
  and the attack goes in that direction and misses;
- D-20261007-48: what ends or moves a lock;
- D-20261005-27: the cone's 60° half-angle; its camera reference is replaced by D-20261007-12;
- D-20261005-28 and D-20261005-85: 350 cm for a paired finisher and for a follow-up strike;
- D-20261004-35: aim decides;
- D-20261004-26: the pillars: a free camera, with an optional lock.

The owner's description (the 2026-10-07 log, E9): "the camera will use input direction to let you turn and attack in
the direction you are pressing but then clicking lockon locks to the best target and enters strafe mode letting you
scroll through targets by direction and location".

**Two modes.** Free mode is the default. Lock mode starts with a lock press and ends as below. Neither a lock-on nor a
target cycle exists today, and today's soft aim (the facing, 500 cm, 120°) is what free mode replaces (3.11).

**The lock input.** No lock action exists: the player binds Light, Heavy, Block and Evade (`PlayerCharacter.cpp:112-140`),
with move and look beside them. Lock mode needs a **new input-action asset**, bound in the combat mapping context
(`IMC_Combat` **[reported]**) with a key on each device. Which key is a content choice.

**The guiding principle** [D-20261007-46, D-20261007-48]: **the lock follows what you actually do.** The owner: "A big
part of this game was meant to be bouncing between enemies and attacking … in … continuous movement … if you lock onto a
target and then hold light for a directional attack, we need to ensure you arent still locked to that target". The lock
never holds the player to a target they are no longer fighting.

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
| Lock-2 | While locked, a Light or Heavy press aims at the locked target; the cone is not consulted [D-20261007-13]. **A directional light is the exception:** the stick aims it as in free mode, toward the nearest enemy in the stick direction (camera-relative), and the lock transfers to that enemy; with no enemy that way **the lock breaks and the attack goes in that direction and misses** [D-20261007-47]. The owner: "you should always go where you are trying to go. going to the right place relies on picking the right direction for the directional attack." |
| Lock-3 | The player strafes, facing the locked target. When the lock transfers, the strafe re-centres on the new target [D-20261007-46]. |
| Lock-4 | A flick of the look stick, or a scroll, switches the lock to the nearest enemy in that direction. |
| Lock-5 | **The lock follows action** [D-20261007-46]: acting toward a different enemy while locked transfers the lock to that enemy: a directional light toward one (Lock-2), a parry-counter of one (3.3), a follow-up strike or a paired finisher on one (3.3, 3.8). |
| Lock-6 | **A buffered press honours the hold** [D-20261007-45]: a Light or Heavy press buffered through hit-stun, the Staggered state or a sequence starts its attack when that state ends, and at the attack's hold-window start the button state is checked as usual, so holding through and releasing after yields a directional light or a charged heavy. A directional light fired this way transfers or breaks the lock by Lock-2. |

**What ends or moves a lock** [D-20261007-48]:

| # | Event | Result |
|---|---|---|
| End-1 | The target dies. | Re-pick a near enemy; else free mode. What "near" means is Q19 [D-20261007-13, D-20261007-48]. |
| End-2 | The target is out of range, or out of sight, for a short settings grace. | Free mode. The range is Q19; the grace has no decided value (3.12). |
| End-3 | A second lock press. | Free mode. |
| End-4 | The player is Staggered (3.4), or is the victim of an enemy's paired finisher (3.8). | Free mode. |
| End-5 | A Light held for a directional attack. | By Lock-2: the lock transfers to the enemy in the stick direction, or breaks when there is none [D-20261007-47]. |
| End-6 | A Block that blocks a different attacker. | **My reading:** the lock transfers to that attacker, consistent with D-46 and the block turn; owner may veto. (Carried verbatim from the register, Appendix B.) |
| End-7 | "Anything else that keeps combat consistent." | The owner's words. The implementer applies the principle "the lock follows what you actually do". Under 1.1 any further event is listed in the implementation brief for the owner to see, not chosen quietly (a reading, Appendix B). |

The dodge is not in the list, so a dodge leaves the lock in place (a reading, Appendix B).

Nothing here designs the camera's framing while locked, the strafe speed or the lock indicator; they belong to the camera
and movement designs the pillars queued, with guard aim (3.11), and come later. Guard facing keeps today's threat
selection (3.11); the lock does not change what guard faces except through End-6 (a reading, Appendix B).

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Aim cone (half-angle) | 60° | combat rules | D-20261005-27, D-20261007-12 | soft aim from facing: 120° rejection, scored on 45° |
| Cone centre | the stick direction, else the facing | rule, no tunable | D-20261007-12 | the facing |
| Tie rule | Staggered or eligible first, then nearest | rule, no tunable | D-20261007-12 | scored on angle |
| Finisher and follow-up range | 350 cm | combat rules | D-20261005-28, D-20261005-85 | 500 cm, `SoftAimRange` |
| Lock range (how far a lock press looks, and the out-of-range end) | Q19 | combat rules (proposed) | D-20261007-13, D-20261007-48 | none |
| "Near", for the automatic re-pick on death | Q19 | combat rules (proposed) | D-20261007-13, D-20261007-48 | none |
| Lock grace (out of range or out of sight before the lock ends) | no decided value, "short" | combat rules (proposed, Q23) | D-20261007-48 | none |
| Flick threshold (look-stick deflection that switches) | no decided value; tuned on captures | combat rules (proposed, Q23) | D-20261007-13 | none |
| Lock input | a new input-action asset, key per device a content choice | the combat mapping context | — | none exists |

**Changes from today:**
- The cone's centre is the stick direction, else the facing. The earlier draft of this spec keyed it to where the
  camera looked [D-20261005-27, replaced by D-20261007-12].
- One rule for every press; today's soft aim serves paired finishers only.
- Lock mode is new: today there is no lock-on, strafe or target cycling, and no lock input.
- The lock follows action, and a directional light goes where the stick points even while locked [D-20261007-46,
  D-20261007-47].

### 3.14 The dodge

**Decisions:**
- D-20261007-21: the dodge is designed here and delivered as a small stage before hit-stun ships (9.4);
- D-20261007-49: the direction: the stick, camera-relative; a neutral stick dodges away from the current threat, or
  straight back;
- D-20261007-50: an authored invulnerability window, held as data on the dodge, with a default tuned on captures;
- D-20261007-51: what it cancels, and its own cancel point;
- D-20261007-52: no resource, no cooldown, consecutive dodges, and a press during a dodge buffered to its cancel point;
- D-20261005-87 and D-20261005-88: the escapes from hit-stun and from the player's Staggered state (3.7, 3.4);
- D-20261006-01: an escape from both needs both minimums;
- D-20261007-32: a dodge cancels a push in flight (3.10);
- D-20261007-48: the dodge is not among the events that end a lock;
- D-20261007-54: the dodge escape from hit-stun and Staggered is a profile field, not a check on who the player is.

**Input.** The Evade press. Today it does nothing (`CombatComponent.cpp:4147-4149` **[reported]**); the input action
exists (`IA_Dash` **[reported]**).

**Direction** [D-20261007-49]:
- the stick direction, camera-relative;
- with a neutral stick, **away from the current threat**: the enemy faced, or the locked target; straight back if there is
  none;
- eight-way or free-angle is a content choice, not a rule. (D-20261007-47 and D-20261007-49 both say "camera-relative";
  that supports reading 46 on the stick's frame, Appendix B.)

**Evaluated on a dodge press, in order:**

| # | Condition | Result |
|---|---|---|
| Dodge-1 | The player is in hit-stun or Staggered, and the profile allows the dodge escape, as the player's does [D-20261007-54]. | The press waits for the escape to open, max(0.12 s, half the state's length) after the hit, both minimums when both run; then it ends the state, closes the enemy's opening when Staggered, and the committed dodge plays (3.4, 3.7) [D-20261005-87, D-20261005-88, D-20261006-01, D-20261007-51]. |
| Dodge-2 | The player is inside their own parry-counter or paired finisher, before their cancel notify. | Buffered; it fires at the cancel notify like any press (3.3) [D-20261007-09]. That is a reading: D-20261007-51 names no sequence (Appendix B). |
| Dodge-3 | The player is in an attack's Windup or Active phase. | **Never cancelled** [D-20261007-51]. The press is buffered and fires at Recovery; a reading under "input is never lost" (Appendix B). |
| Dodge-4 | The player is in an attack's Recovery. | The dodge **ends the Recovery** and plays [D-20261007-51]. |
| Dodge-5 | Guard is up. | **Guard drops**, and the dodge plays [D-20261007-51]. |
| Dodge-6 | A push is in flight. | **The remaining push is cancelled**, and the dodge plays [D-20261007-32]. |
| Dodge-7 | A dodge is playing. | The press is **buffered to the dodge's cancel point**, then a second dodge plays: consecutive dodges are allowed [D-20261007-52]. |
| Dodge-8 | Otherwise. | The dodge plays, in the direction above. |

**The dodge itself:**
- it is **committed**: it plays through [D-20261005-87];
- it carries an **authored invulnerability window** (the i-frame window), held as data on the dodge and anchored to its
  animation like every other window; during it every contact on the dodger is ignored (Contact-2) [D-20261007-50];
- it has **its own cancel point**: after it, an attack press starts the opener from the dodge, aimed as usual (3.13);
  before it, the press is buffered [D-20261007-51]. The cancel point is held on the dodge's data beside the i-frame
  window (a reading, Appendix B);
- it has **no resource and no cooldown** [D-20261007-52];
- it does not end a lock [D-20261007-48, by omission; a reading, Appendix B];
- guard does not come back on its own after a dodge: a new press raises it (3.1).

**Tunables** (the homes are the owner's; the values are tuned on captures and none is decided, Q22):

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Dodge distance | no decided value | the dodge's own data; combat-rules default (Q23) | D-20261007-21 | none |
| Dodge duration | no decided value | the dodge's own data; combat-rules default (Q23) | D-20261007-21 | none |
| I-frame window (start and length) | no decided value; tuned on captures | the dodge's own data, as a data window; combat-rules default (Q23) | D-20261007-50 | none |
| Cancel point | authored on the dodge's data | the dodge's own data | D-20261007-51 | none |
| Direction mode | a content choice | the dodge's clips | D-20261007-49 | none |
| Cooldown, resource | none | rule, no tunable | D-20261007-52 | none |

The dodge's data asset, and where a character references it, are proposed in 4.14.

**Changes from today:** the Evade press does nothing today. Everything above is new.

### 3.15 The contact predictor

**Decisions:**
- D-20261007-24: the predictor, its confidence, its users and their thresholds, and the homes of its tolerance and horizon;
- D-20261007-22: "will it hit" is answered by the predictor, never guessed;
- D-20261007-20: the enemy's finisher choice uses it;
- D-20261005-73: contact is the predicted blade arrival;
- D-20261005-64 and D-20261005-79: the parry succeeds on timing alone; the prediction is a reference, not a gate on the
  parry's success.

**What it answers.** For an attack against a target, the predictor says **whether and when** the blade path reaches the
target's capsule, with a **confidence** [D-20261007-24]. The owner: "we should also have prediction analytics for
contacts so we dont have to guess whether attacks are going to land" [D-20261007-20].

**Inputs** [D-20261007-24]:
- the attack's authored trace path: the weapon's `WeaponStart`/`WeaponEnd` socket path over the attack's section (the
  path "Auto-detect swing" also reads, 4.5);
- the attack's warp, which moves that path as the attacker closes;
- the target's motion;
- the attacker's montage position and effective play rate, including hitstop and the attacker slowdown (3.2), so arrival
  moves when the attacker is slowed (proposed, from 3.2);
- the contact marker, as the authored moment the path is expected to arrive, with the Active start as the fallback
  (4.5).

**Outputs:**
- **when:** the predicted arrival C (2.2), re-evaluated each animation tick while the attack is in progress; the latest
  value is used at a press (3.2);
- **whether:** a confidence that the path reaches the capsule within the horizon, judged with a tolerance.

**Users and thresholds** [D-20261007-24]. Each user has its own threshold:

| User | Threshold | What it decides |
|---|---|---|
| The parry window (3.2) | lenient | the arrival C that the window is measured back from; and, at the lenient threshold, that the attack is reaching this defender at all (Parry-3's "aimed at" and "in progress"). It never decides whether the parry succeeds [D-20261005-64, D-20261005-79]; that is a reading beside the removed confidence gate (Appendix B). |
| The enemy's finisher choice (3.8) | strict | whether a marked enemy's next attack would land on the player while Staggered or in hit-stun, at that attack's start [D-20261007-20]. |
| The player's lethal strike (3.8) | its own; not named in the decision (Q20) | whether a lethal press reaches its target, so that a paired finisher is chosen only for a strike that lands [D-20261007-22]. |

**Tunables:**

| Tunable | Default | Home | Decision | Today |
|---|---|---|---|---|
| Tolerance (how near the path must come) | no decided value | combat rules; per-attack override | D-20261007-24, D-20261007-53 | none |
| Horizon (how far ahead it looks) | no decided value | combat rules; per-attack override | D-20261007-24, D-20261007-53 | none |
| Thresholds per user, and the confidence scale | Q20 | combat rules | D-20261007-24, D-20261007-53 | none |

**Changes from today:** today's prediction publishes a source socket, a target bone and a predicted contact time every
animation tick (**[reported]** `CombatComponent.cpp:2260-2374`), with the parry window's end as the contact time, and
its confidence once gated the parry; that gate is removed [D-20261005-64]. The predictor replaces the time with the
trajectory's arrival and gives the confidence to three users, none of which is the parry's success.

---

## 4. Data model

### 4.1 One place per question

The asset layout is the owner's [D-20261005-48, D-20261005-57, D-20261010-02]: the root is the **kit**, which holds the
**weapon** (the physical data: mesh, sockets, traces, multiplier) and references a **combo set** (attacks only), a
**defense set** (block and parry responses) and a **paired set** (every two-character sequence). The per-character
**reaction profile** keeps what belongs to the body. The global rules live in one **combat rules** asset for the
project; each character's **combat settings** keep its type defaults and its default kit [D-20261007-53]. The old
defense configuration is retired. A character with no kit uses its combat settings' default kit, so its sets apply with
no special case [D-20261005-49]. Two kits may share the weapon and the defense and paired sets and differ only in combo
set: an enemy with the same katana and fewer moves is a second kit, not a second weapon [D-20261010-02].

Nothing references across the sets. A defense row that seeks a paired counter, and a lethal strike that needs a
finisher, ask the kit's paired set **by key** through the selection tables (4.16): direction and swing (lane preferred)
for a counter, direction for a finisher, with the variety rule among several rows [D-20261010-01].

```
UCombatRules (one per project; a developer-settings root, proposed)
UCombatSettings (per character) ── DefaultKit ──┐
                                                ▼
BaseCombatCharacter ── Kit ─▶ UCombatKit ──▶ UWeaponData   (mesh, sockets, traces, multiplier, equip/holster, FX pool)
                    │                   ├──▶ UComboSet     (slot rows: attack + diff + links + selection columns;
                    │                   │                   openers and entry rules as rows; set-level defaults)
                    │                   ├──▶ UDefenseSet   (block reactions, guard enter/exit, arc and turn overrides;
                    │                   │                   counter rows: direction × swing [× lane] → deflect, damage,
                    │                   │                   fallback strike, seeks a paired counter yes/no)
                    │                   └──▶ UPairedSet    (counter rows by direction and swing, each able to kill, kill
                    │                                       sequence optional; finisher rows by direction; fit budgets;
                    │                                       holds player-finisher content = can finish the player.
                    │                                       Found by key from the defense set and from a lethal strike.)
                    └── ReactionProfile ──▶ UReactionProfile (hit reactions, Staggered reaction, recoil, armor, guard
                                                              break, own Staggered time, dodge escape, parry assists)
```

| Question | Where the answer lives | Decision |
|---|---|---|
| Which kit does this character fight with? | Its kit, or its combat settings' default kit | D-20261005-48, D-20261005-49, D-20261010-02 |
| What is the weapon physically: mesh, sockets, traces, multiplier? | The weapon (`UWeaponData`), referenced by the kit | D-20261010-02 |
| What follows what, from which input? | The combo set: slots, links, openers, entry rules | D-20261005-01, D-20261005-03, D-20261005-40 |
| What does this move do: montage, timing, damage, pressure, flags, impact feedback? | The attack (`UAttackData`) | D-20261004-20, D-20261005-40 |
| How does this slot differ from its attack? | The slot's overrides | D-20261005-01 |
| Which slot does an enemy pick, from where, against what? | The slot row's selection columns | D-20261005-63, D-20261007-57 |
| How does this kit block, deflect, and answer a parry? | The defense set: block reactions, guard enter and exit, the counter rows | D-20261005-48, D-20261005-45, D-20261005-55, D-20261010-03 |
| Which two-character counters and finishers does this kit play? | The paired set, keyed by direction and role, found by key | D-20261005-57, D-20261005-47, D-20261010-01 |
| Can this kit finish the player? | Its paired set holds player-finisher content | D-20261007-55 |
| How does this character's body react, resist and break? | Its reaction profile | D-20261004-20, D-20261005-48, D-20261010-03 |
| Is this character the player? | No rule asks. The player's profile says: counting off, its own Staggered time, the dodge escape allowed | D-20261007-54 |
| What are the global rules? | The combat rules, one asset | D-20261007-53 |
| What are this character's per-type defaults and its default kit? | Its combat settings | D-20261003-08, D-20261004-20, D-20261007-53 |

**The rule for where a tunable lives** [D-20261005-68, D-20261010-03]: a tunable lives with the thing it describes.
- **The reaction profile covers reactions to hits only:** hit reactions, armor, guard-break counting, the Staggered
  reaction and recoil, hit-stun responses, the character's own Staggered time, the dodge escape and parry assists. It
  does not arbitrate movement or camera; movement and camera get their own profiles when they are needed.
- **What the weapon animates lives on the kit's sets:** block reactions, guard clips, deflects, counter rows, and the
  arc and turn overrides, on the defense set; two-character content on the paired set [D-20261010-03].
- **What the body decides lives on the reaction profile:** guard-break counting, armor, the character's own Staggered
  time [D-20261010-03].
- **Global rules live on the combat rules; per-type defaults and the default kit on each character's combat settings**
  [D-20261007-53].

**The defense split, kept** [D-20261010-03]. The survey's counter-example is Ninja Combat, which owns defense on the
target: a defense data asset per character or group decides every mitigation stage, and the weapon is attacker-side only
(`research-data-model-survey.md`, the Ninja Combat row and "Rule: weapon → combo set / defense set / paired set",
citing https://tech.ninjabear.studio/cbt-concept-damage-and-mitigation.html, **[reported]**). The owner kept the split
because the katana animates the block: what the weapon animates is the weapon's, what the body decides is the body's.

**Which side's asset is read.** The review of sections 6 to 9 asked whose settings a rule reads. D-20261007-53 closes
it: a global rule is read from the one combat rules asset, whoever is involved; a per-type default is read from the
settings of the character whose attack it is, when that attack's resolved spec is built (4.2, 4.11; a reading, Appendix
B). The sets:
- The **defender's** kit supplies its defense set (block reactions, deflects, counter rows) and, by key, its paired set's
  counters for a parry-counter.
- The **attacker's** kit supplies its combo set and, by key, its paired set's finishers when its follow-up strike or
  ordinary attack is lethal (3.8), and whether it can finish the player [D-20261007-55].
- The **victim's** reaction profile supplies its hit reactions, its Staggered reaction, its armor and its guard-break
  settings.

### 4.2 Precedence

Every layered field resolves on its own, field by field. A layer that sets the field wins over the layers above it.
**The four layers are decided** [D-20261005-61].

1. **Combat settings type default**, per attack type, read from the attacking character's combat settings
   [D-20261007-53]. The tables have Light, Heavy, Charged and Special columns [D-20261004-20, D-20261004-38]. Global
   rules are not a layer: they come from the one combat rules asset (4.15), and no set, attack or slot overrides them,
   except where a rule names its own per-weapon override (the guard arc, the turn rates, the fitting budgets).
2. **Combo set type default**, the combo set's own per-type value [D-20261005-40]. It is the per-weapon layer
   [D-20261005-61], the one D-20261004-38 noted for later.
3. **Attack override**, on the attack asset [D-20261004-20].
4. **Slot override**, on the combo-set slot. **It wins** [D-20261005-01]. Every attack field can be overridden here
   [D-20261007-35].

The four layers stay [D-20261007-34]. **The combo-set layer is hidden in the editor** until more than one combo set would
differ from the settings: with one weapon it adds a row that never says anything [D-20261007-34]. The data keeps the
layer, so a second weapon reveals it with no migration.

Notes:
- Each override is a toggle plus a value, the pattern `FKnockbackOverride` already uses (`CombatTypes.h:498-533`).
- **The Staggered time has one default**, not one per type [D-20261005-08]. Layer 1 holds that single value, and layers 3
  and 4 override it.
- **Heavy attacks carry values for both releases.** Where a type default distinguishes a charged release (pressure,
  hit-stun, guard breaker), a heavy attack's and a slot's override sets have one value for an uncharged release and one
  for a charged release. This is proposed (Q1 defines "charged").
- **Characters resolve the same way:** the base profile, then the character profile's overrides [D-20261004-20].
- **The editor always shows which layer an effective value came from** (5.3).
- **Two ambiguities the stress test exposed are open** (**[reported]**; neither is decided): a row drag carries the
  slot's diff with the row (Q25), and the set layer sits below the attack layer, so a weapon cannot re-default an attack
  that sets its own value except slot by slot (Q26).

### 4.3 The kit

The root asset is the **kit** [D-20261010-02]: the weapon's physical data plus the three sets. **Proposed class:**
`UCombatKit`. **Proposed asset name:** `DA_Kit_<Name>`, for example `DA_Kit_Katana`, named for its purpose
[D-20261004-17]. The owner's pick: "Rename the root to a Kit: weapon (mesh, sockets, traces) + combo set + defense set
+ paired set".

| Field | Holds | Decision |
|---|---|---|
| Weapon | The physical data: `UWeaponData` as it exists today (`WeaponData.h`), with its mesh, sockets, equip and holster montages, trace settings, damage multiplier, hit sound and effect, and effects pool. | D-20261010-02 |
| Combo set | A reference to a `UComboSet` (4.4). | D-20261005-48 |
| Defense set | A reference to a `UDefenseSet` (4.6). | D-20261005-48 |
| Paired set | A reference to a `UPairedSet` (4.7). | D-20261005-57 |

**Two kits may share.** The same weapon, defense set and paired set may sit in two kits that differ in combo set: the
player's katana kit and an enemy's katana kit with fewer moves [D-20261010-02]. That replaces a second weapon asset per
enemy variant, and it is how the per-actor AI attack lists retire (7.2) [D-20261005-63].

**The weapon stays a sub-asset (proposed).** `UWeaponData` keeps its class and its physical fields and is referenced by
the kit rather than merged into it. Recommended because the weapon component reads it today
(`WeaponComponent.cpp:87-97`), because the same katana mesh and traces then sit in one place for every kit that carries
it, and because a kit then changes only its references when a set is swapped. Merging would save one asset per kit and
cost a copy of the physical data for every enemy variant. The owner's pick names the two as parts of one kit and does not
choose between them, so this is a proposed shape.

**Changes from today** [D-20261005-48, D-20261005-57, D-20261010-02]:

| Field | Today | After |
|---|---|---|
| `AttackConfiguration` (`WeaponData.h:185`) | Points at `UAttackConfiguration`, which holds only the two openers that are read | Removed from the weapon; the kit's **`ComboSet`** replaces it |
| `BaseCombatCharacter::WeaponData`; `UCombatSettings::DefaultWeaponData` (`CombatSettings.h:78`) | Reference the weapon | Become **`Kit`** and **`DefaultKit`**, references to a `UCombatKit` |
| — | — | The kit's **`DefenseSet`** and **`PairedSet`** |

**Resolution** [D-20261005-49]. Today the weapon component uses its own `WeaponData`, else the combat settings'
`DefaultWeaponData` (`WeaponComponent.cpp:87-97`). That order stays, with the kit in the weapon's place: the character's
kit, else its combat settings' default kit; the weapon and all three sets come with whichever kit wins. There is no
separate fallback for the weapon or for any one set.

**Validation.** A kit missing its weapon or any of the three sets is an error. There is no cross-reference to check:
the defense set and the combo set hold no references into the paired set, so the former rule, a reference into a paired
set other than the weapon's, is gone [D-20261010-01]. What validation checks instead is **coverage**: that the kit's
paired set has a counter row for every direction and swing a defense-set row seeks (4.6), and a finisher row for every
direction (4.7).

### 4.4 The combo set

The combo set holds attacks only [D-20261005-48]: "the asset holding slots, links and entry rules for one moveset, plus
set-level type defaults" [D-20261005-40].

**Proposed class:** `UComboSet`. **Proposed asset name:** `DA_ComboSet_<Moveset>`, for example `DA_ComboSet_Katana`.
It is named for its purpose, following the project's naming rule [D-20261004-17].

| Field | Holds | Decision |
|---|---|---|
| Slots | An array of slots (below). | D-20261005-01 |
| Openers | The slot each input starts from when no combo is running: a Light opener and a Heavy opener (today `DefaultLightAttack` and `DefaultHeavyAttack`). | glossary "entry rules" [D-20261005-40] |
| Entry rules | Rows of a selection table (4.16): input and stance or ground state → the slot a press starts from, so that running or airborne presses can start elsewhere when those moves are designed. The two plain openers are its first rows; no other row ships now (7.2). | D-20261005-40, D-20261007-57 |
| Set-level type defaults | Per-type values for the layered fields (4.2). | D-20261005-40 |

**Slot** (proposed struct `FComboSlot`):

| Field | Holds |
|---|---|
| Id | A stable, hidden identifier. Links and the editor use it, so renaming or reordering never breaks a link. |
| Attack | A reference to an attack asset [D-20261005-01]. |
| Lane | Light, Heavy or Directional, for the grid [D-20261005-02]. |
| Links | One per input: Light, Heavy, and hold-and-release Light toward Forward, Back, Left or Right. Holding Heavy is the charge, so it has no directional links. |
| Diff | The slot's overrides: a toggle and a value for **any** attack field (4.5), the timing windows included [D-20261005-01, D-20261007-35]. A slot is an attack plus this diff. |
| Selection columns | The slot row's columns in the combo set's selection table (4.16): a weight (default 1.0), a distance range (today's minimum and maximum range, `FEnemyAttackConfig`, `EnemyAITypes.h:113-125`) and a target state. An enemy picks among its kit's combo set's slot rows by these, through the one evaluator, and the "rows evaluated" trace says why; the separate AI attack lists are retired [D-20261005-63, D-20261007-57]. |

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
| Warning | A combo set whose slot rows all have a weight of 0, so an enemy using it never attacks. |
| Warning | An attack with no authored parry window; it gets the 0.35 s minimum at runtime [D-20261005-74]. |
| Warning | An unparryable attack that has an authored parry window. |
| Warning | An attack flagged both unblockable and guard breaker: at contact unblockable wins and the guard does not break [D-20261007-16]. |
| Warning | A cancel notify on an attack montage: attacks cancel at the phase transition, and the notify is ignored (4.13) [D-20261006-02]. |
| Warning | An attack whose hit-stun is shorter than its push: the victim would act while still being carried (3.10) [D-20261007-32]. |
| Warning | A stale mirror: the montage's derived notifies differ from the attack's data windows, on every attack that uses that montage (4.13). **Proposed** (the 2026-10-07 log, E23); D-20261007-40 fixes the save prompt, not this warning. |
| Warning | A readable flag (guard breaker, unparryable, unblockable, block-interruptible) overridden on a slot while the same attack is reachable from another slot without the override: players see the same swing with and without its cue (6.5). **Proposed**, from the editor design pass. |
| Info | An override equal to the value it inherits. |
| Not flagged | A loop through a back-to-opener link. It is shown, not flagged [D-20261005-03]. |

**Edit-through** [D-20261005-01]:
- The slot card shows the referenced attack's own fields, and editing them edits the attack asset.
- Before the first edit, the card shows "used by N slots in M sets", so a shared change is never a surprise.

**The slot is an attack plus a diff** [D-20261007-35]. Every attack field can be overridden per slot: the impact block,
the readable flags, the swing, the reach and warp, and every timing window (4.5). The row shows only the diff by default,
in four collapsible groups with counts; "show inherited" reveals the greyed values with their source (5.3). The owner:
"Couldnt we also just go with b but make presentation more intelligent, clean and effective?"

**Save to asset** [D-20261007-35]. Beside each overridden field a button writes the slot's value into the attack asset and
clears the override; the used-by notice appears then, before the write, since every slot that references the attack
changes with it. The owner: "we could add a 'Save To Asset' button next to each overridable property that pops up when you
change the value from the assets value and only then does it write through? this way its explicit but still easy?"

**Extract to new attack** [D-20261007-35]. On the row, it turns the slot's diff into a new attack asset and points the
slot at it. It replaces "make unique" [D-20261005-01]. Because the windows are data [D-20261007-38], a diff that changes
a window needs no montage copy: the new attack holds its own windows and exports its own derived notifies to the shared
montage section if asked. Whether the extracted attack should also be offered a new section in the same montage is
undecided (the design pass left it open); nothing in play depends on it.

**No finisher references.** The combo set once carried one reference per direction into the paired set's finisher
list (this spec's shape under D-20261005-57). It carries none: a lethal strike asks the attacker's kit's paired set by
key (3.8, 4.16) [D-20261010-01]. One link, one validation rule and one way to drift are gone with it.

**A row drag and the diff** (Q25). Dragging a row carries the slot's diff with it, so a diff authored for position 3
travels to position 4 when the row moves (the stress test, **[reported]**). Whether the grid says so on the drop or
offers "keep overrides at position" is open.

### 4.5 The attack

The attack is "the move itself: montage, timing, damage, pressure, flags, impact feedback" [D-20261005-40].

**It keeps:**
- `AttackType`;
- the montage and section fields;
- `BaseDamage`;
- `MaxHitCount`;
- **the timing windows, as data** (below) [D-20261007-38]; today's `ManualTiming` and `bUseAnimNotifyTiming` become
  them. Nothing reads `ManualTiming` at runtime today, and `bUseAnimNotifyTiming` only toggles the field's visibility
  (**[reported]** `CombatTypes.h:650-652`, `AttackData.h:286-300`);
- the hold and charge fields;
- `WarpConfig`;
- the defense profile: height, lane, contact socket and bone, and the swing;
- the attack tags `Attack.Property.Unblockable` and `Attack.Defense.BlockInterruptible`;
- the combo timing fields `ComboInputWindow` and the blend times.

**The timing windows** [D-20261007-36, D-20261007-38]. The attack owns every window of its move, and each is part of
the resolved attack spec (4.11) with a slot diff [D-20261007-35]:

| Window | Form | Read by |
|---|---|---|
| Phases | the Windup→Active and Active→Recovery boundaries | combo timing; the hit window; the attack's cancel point (3.0) |
| Parry window | a lead time before the contact marker (3.2; the lead-time form is R-20261005-01, proposed) | 3.2 |
| Hold start | the moment the hold check runs | the hold (CLAUDE.md rule 4) |
| Armor windows | start, end and a level, on the attack's own section | 3.6 |
| Contact marker | the authored moment the blade is expected to arrive; the scripted hit's moment on strikes | the predictor (3.15); scripted hits (3.3) |
| Cancel point | the Active→Recovery boundary; attacks have no separate cancel notify | 3.0 |

- **Anchoring** [D-20261007-36]: a window is "this many seconds into this animation segment", **sequence time plus
  segment**, resolved to montage time on load or cook. It survives retiming or reordering the montage's segments. This
  is ComboCreator's model, chosen by the owner; its resolver (**[reported]** `FCCAnchoredWindow`, anchors resolved in
  `PreSave`) is the shape to follow, not code to copy.
- **Export and adopt** [D-20261007-38]: the montage's notifies for these windows are derived mirrors. "Export to
  montage" writes them from the data; "adopt from montage" reads a hand-authored montage into the data once (4.13, 7.3).
- **The runtime reads the data**, never the mirror (6.5) [D-20261007-38, D-20261007-39].

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
| Block shove | its own type defaults (3.10) | yes | D-20261005-59, D-20261007-27 |
| Guard-break push | its own type defaults (3.10) | yes | D-20261005-59, D-20261007-27 |
| Impact feedback per outcome (hit, block, parry): sound, effect, hitstop, camera shake | type defaults | yes | D-20261004-20, D-20261004-06 |
| Unparryable | off | yes | D-20261005-07, D-20261005-40 |
| Unblockable, block-interruptible (today the tags `Attack.Property.Unblockable`, `Attack.Defense.BlockInterruptible`) | off | yes | D-20261007-35 |
| Swing, lane, height, reach and warp, the blend times, `ComboInputWindow` | the attack's own values | yes | D-20261007-35 |
| Every timing window (above) | the attack's own values | yes | D-20261007-35, D-20261007-38 |

Every row is slot-overridable [D-20261007-35]; a slot override of a readable flag is real because the defender reads the
resolved spec, and readable because the telegraph fires from it (6.5) [D-20261007-37].

"Unparryable" is a plain on/off flag. The project's semantics rule gives booleans "direct authored gates", and the
glossary calls it "an attack flagged". An attack flagged both unblockable and guard breaker saves, with a warning (4.4):
at contact unblockable wins [D-20261007-16].

**Deprecated, then removed after migration (7.2):**
- **The combo links** `NextComboAttack`, `HeavyComboAttack`, `DirectionalFollowUps` and `HeavyDirectionalFollowUps`
  (`AttackData.h:157-170`). They move to the combo set. `HeavyDirectionalFollowUps` is not read at runtime today; only
  validation and one check read it (`CombatComponent.cpp:3642-3643`).
- **`CounterData` and `FinisherData`** (`AttackData.h:322-331`). Their content becomes paired-set rows, one row per
  content asset, found by key (4.7, 7.3) [D-20261005-48, D-20261005-57, D-20261010-01, D-20261010-04].
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
paired set; the defense set keeps the deflect and the fallback strike. Since D-20261006-03 each entry also holds the
counter's damage, and since D-20261007-07 the paired counter can kill on its own, with an authored kill sequence
optional. Since D-20261007-57 the counter pool is a **selection table** (4.16), and since D-20261010-01 an entry holds
no reference to the paired counter: it says whether one is sought, and the counter is found by key in the kit's paired
set. The split with the reaction profile is kept: what the weapon animates is here; what the body decides is on the
profile [D-20261010-03].

**Proposed class:** `UDefenseSet`. **Proposed asset name:** `DA_DefenseSet_<Weapon>`, for example `DA_DefenseSet_Katana`.

| Field | Holds | Decision |
|---|---|---|
| Defense tunables | Overrides of the combat-rules defaults for this weapon: the guard arc [D-20261005-70], the block turn rate [D-20261005-69] and the fastest natural parry turn [D-20261005-75]. Each is a toggle plus a value. | D-20261005-68, D-20261005-69, D-20261005-70, D-20261005-75, D-20261007-53, D-20261010-03 |
| Block reactions | One block reaction per attacker direction (front, left, right, behind). Only the front entry is used while the guard arc is ±75°; the others wait for full-circle blocks, which then need content plus one setting. | D-20261005-48, D-20261004-23 |
| Guard stance | The guard enter and exit clips, wired at last. | D-20260929-07, D-20261005-55 |
| Counter table | Counter rows (below), a selection table keyed by attacker direction and swing, lane optional. | D-20261005-24, D-20261005-45, D-20261005-48, D-20261007-57 |
| Turn-deflects | The 90° left, 90° right and 180° behind turn-deflects that flow into front entries. | D-20261004-30 |

**Counter row** (proposed struct `FCounterRow`, the former `FCounterEntry`). Its key columns are the shared vocabulary
of 4.16; its outputs are the weapon's content and the counter's damage:

| Column | Kind | Holds | Decision |
|---|---|---|---|
| Direction | key | Front, left, right or behind. | D-20261004-30, D-20261005-45 |
| Swing | key | Slash left, slash right, overhead or thrust, or any. | D-20261005-45 |
| Lane | key, optional | Left, centre or right. A row that names the lane is preferred when it matches: "lane preferred" (3.3). | D-20261005-45, D-20261010-01 |
| Lethal | key, optional | Any by default. The column is in the vocabulary; this spec gives it no use on counter rows beyond what D-20261007-07 decides, since the kill sequence lives on the paired content (a reading, Appendix B). | D-20261007-57 |
| Deflect | output | The defender's deflect that meets the blade. An off-front row may instead name its turn-deflect and a front row. | D-20261005-38, D-20261004-30 |
| Seeks a paired counter | output | Yes or no. Yes: the kit's paired set is asked, by key, for counter rows matching this row's direction and swing, lane preferred, with the variety rule; the one found shares this row's deflect, can kill at its own contact, and may carry an authored kill sequence. No, or none found: this row's fallback strike plays. There is no reference. | D-20261005-48, D-20261005-57, D-20261007-07, D-20261010-01 |
| Damage | output | This row's counter damage: a toggle plus a value over the combat rules' default of 50. The paired clip's own override beats it; the fallback strike always deals this value (3.3). | D-20261006-03, D-20261005-80, D-20261007-53 |
| Fallback strike | output | This row's single-character strike, played when no paired counter is sought or none fits. The glossary keys it by direction and swing like the rest of the row. It carries its own cancel notify (3.0). | D-20261005-23, D-20261005-48, D-20261006-02 |

**Validation** (`IsDataValid`, with badges in the defense set editor, 5.9). Coverage is the whole of the cross-set check:

| Severity | Rule |
|---|---|
| Warning | A direction × swing cell with no row, with the fallback that will play for it [D-20261005-30]. |
| Warning | A row that seeks a paired counter while the kit's paired set has no counter row for its direction and swing, so only its fallback strike can play (coverage) [D-20261010-01]. |
| Warning | A deflect, turn-deflect, fallback strike or block reaction with no cancel notify; it is read as ending at montage end [D-20261006-02]. |
| Error | A row with no fallback strike; an empty counter table; no front block reaction; no guard enter or exit clip. |
| Error | Two rows with the same key columns, which the evaluator could tell apart only by the variety rule; a row whose key names no direction. |

### 4.7 The paired set

The paired set "holds every two-character sequence keyed by direction and role: counters (lethal and non-lethal
variants) and finishers. The defense set references counters from it; the combo set references finishers from it"
[D-20261005-57]. The owner's reason: finishers are offensive, so they do not belong with the weapon's defenses. Since
D-20261007-07 a counter is one sequence that can kill at its own contact; an authored kill sequence is optional. Since
D-20261010-01 **nothing references into it**: a defense row asks for its counters by key, a lethal strike asks for its
finishers by key, and the two "references" in the decision's wording are those lookups. Since D-20261007-57 its counters
and finishers are rows of selection tables (4.16). Since D-20261010-04 **one row is one content asset**: the content's
own internal variants (Duet's angle sector, distance and weight) are alternates of that row's clip, chosen by the content
inside the direction, swing and variety choice our tables have already made; they never decide the direction. Since
D-20261007-55 a paired set that holds player-finisher content is the one that can finish the player (3.8).

**Proposed class:** `UPairedSet`. **Proposed asset name:** `DA_PairedSet_<Weapon>`, for example `DA_PairedSet_Katana`.

| Field | Holds | Decision |
|---|---|---|
| Counter rows | Counter rows (below), a selection table keyed by attacker direction and the parried swing, lane optional. A defense row that seeks a paired counter is answered from here by key. | D-20261005-57, D-20261005-45, D-20261007-57, D-20261010-01 |
| Finisher rows | Finisher rows (below), a selection table keyed by attacker direction (front, left, right, behind). Each direction's rows are a **list**; one is chosen by the variety rule, no throttle. A lethal strike is answered from here by key. | D-20261005-57, D-20261005-47, D-20261007-31, D-20261007-57, D-20261010-01 |
| Player-finisher content | The finisher rows whose victim is the player. Their presence **is** the "can finish the player" marker; validation checks it. How those rows are told apart is Q24. | D-20261007-55 |
| Fitting budgets | The turn, distance and room a fit to the nearest authored direction may use: each a toggle plus a value over the combat-rules default (3.8). | D-20261007-23, D-20261007-53 |

**Counter row** (proposed struct `FPairedCounterRow`, the former `FPairedCounter`):

| Column | Kind | Holds | Decision |
|---|---|---|---|
| Direction | key | Front, left, right or behind. | D-20261005-57 |
| Swing | key | The swing this counter was authored against, or any. | D-20261005-45 |
| Lane | key, optional | Left, centre or right; preferred when a defense row names the same lane. | D-20261005-45, D-20261010-01 |
| Content | output | One paired-content asset (4.12): deflect plus counter. Its damage lands at the counter's contact and can kill; when it does not, the enemy is Staggered from its own cancel notify. Its alternates are the content's own. | D-20261005-24, D-20261005-57, D-20261007-07, D-20261010-04 |
| Kill sequence (optional) | output | One paired-content asset (4.12): the same deflect flowing into an authored kill, played instead of the counter when the press is lethal. Not required. | D-20261005-38, D-20261005-57, D-20261007-07 |
| Damage override (optional) | output | A toggle plus a value that replaces the defense row's damage for this clip (3.3). | D-20261006-03 |

**Finisher row** (proposed struct `FPairedFinisherRow`, the former `FPairedFinisher`): a direction key; the optional
columns of the vocabulary a row may narrow itself by (a distance range; weight is not used, since the variety rule is
fixed [D-20261007-31]); and one paired-content asset (4.12) as its output [D-20261010-04]. The content declares the
ground state each role needs (grounded, airborne or any; both grounded by default), a height-difference limit override,
and is refused for an invulnerable victim [D-20261007-42]; these answers come through the interface and are **never
copied into the row** [D-20261010-04].

**Validation** (`IsDataValid`, with badges in the paired set editor, 5.9):

| Severity | Rule |
|---|---|
| Warning | A direction with no counter row (coverage), named with the defense-set rows of the kits carrying this set that seek it [D-20261010-01]. |
| Warning | A direction with no finisher row, listed as uncovered with the authored direction a fit would use [D-20261005-47, D-20261007-23]. |
| Warning | A counter row that no defense-set row of any kit carrying this paired set can reach: its direction and swing sought by none. |
| Warning | A counter half, kill-sequence half or finisher half with no cancel notify; it is read as ending at montage end [D-20261006-02]. |
| Warning | A row whose content's own alternates span more than one of our directions, so that the content could pick a direction the table did not (**proposed**, from D-20261010-04). |
| Info | A counter with no kill sequence: a lethal press kills at the counter's own contact [D-20261007-07]. |
| Info | Whether this set can finish the player, read from its player-finisher rows [D-20261007-55]. |
| Error | A counter, kill sequence or finisher whose two roles share a montage, or whose "can it start here?" check (4.12) can never pass, for example missing roles. |
| Error | A finisher row whose content is a counter, or a counter row whose content is a kill (4.12). |

### 4.8 The reaction profile

**Proposed class:** `UReactionProfile`. **Proposed assets:** `DA_ReactionProfile_Base`, and one `DA_ReactionProfile_<Character>`
per character [D-20261004-20].

**Inheritance.** A profile names a parent. Each section has override toggles, and anything unset comes from the parent.
Parents may be nested, but never in a cycle. This is proposed.

**It holds** a character's hit reactions, armor base, guard-break settings, its own Staggered time, whether a dodge may
escape its locked states, and parry assists [D-20261005-48, D-20261005-40 as amended, D-20261007-54]. It covers
reactions to hits only, never movement or camera [D-20261005-68, D-20261010-03]. Block and parry responses are the
weapon's (4.6, 4.7); there is no "can be finished" marker and no body type [D-20261005-47]; and the "can finish the
player" marker is no longer here, since it is content presence on the kit's paired set (4.7) [D-20261007-55].

| Section | Contents | Decision |
|---|---|---|
| Hit reactions | Directional light and heavy reactions (front, back, left, right), each an array of variants. Death reactions, also an array. The additive flinch for each direction. I-frames on each entry, as today. **No stun field.** Each reaction clip carries a cancel notify (3.0); one without it ends at montage end, with a warning (4.13). | D-20261002-07, D-20261004-41, D-20261004-42, D-20261006-02 |
| Body-side defense reactions | The Staggered reaction, by direction: what this character does when its attack is parried or its guard breaks, also played with the fallback strike. Recoil, when its attack is blocked by a block-interruptible defender. | D-20261005-23, D-20261005-55 |
| Armor | Base armor, default 0. | D-20261005-17, D-20261005-44 |
| Guard break | Counting toggle: on in the base profile, off for the player. Threshold N, default 4. Window W, default 3 s. | D-20261005-15, D-20261005-26, D-20261005-46, D-20261005-56, D-20261007-54 |
| Staggered | This character's own Staggered time, a toggle plus a value. Unset in the base profile, so enemies take the causing attack's time; set to 0.6 s on the player's profile. When set, it always applies: an attack's or a slot's override never changes it. | D-20261005-88, D-20261006-06 |
| Dodge escape | Whether a dodge press may end this character's hit-stun and Staggered state after their minimums (3.4, 3.7, 3.14): on for the player; off in the base profile, since no rule gives an enemy a dodge (a reading, Appendix B). | D-20261005-87, D-20261005-88, D-20261007-54 |
| Parry assists | Window scale 1.0, input buffer 0, contact grace 0, slow motion on window off, telegraph on, always perfect parry off. | D-20261004-07, D-20261005-09, D-20261005-78 |
| Resume delay | This enemy's wait before re-entering the attack queue when others hold ends: a toggle plus a value over the combat-rules default [D-20261007-53]. D-20261007-29 says "per-profile override"; this is the only per-character profile, although D-20261005-68 keeps it to reactions to hits (a reading, Appendix B). | D-20261007-29 |
| Other per-character hit values | `KnockbackScale` and damage resistance: how this body takes a hit. They move here; proposed. The guard-facing turn and the block turn do **not** live here [D-20261005-68]. | D-20260930-01 |

**The player by data** [D-20261007-54]. No rule asks whether a character is the player. The three player-specific
rules each read a field above: the guard-break counting toggle (off for the player, 3.5), the character's own Staggered
time (0.6 s for the player, 3.4) and the dodge escape (allowed for the player, 3.4, 3.7, 3.14). The attack tokens count
the attackers of whoever is targeted (3.9). A second character given the player's fields would be treated exactly as the
player is; the owner's pick: "By data, not identity". One shipped-content check asserts that the player's profile has
counting off: a decided rule, not a pinned tunable (8.1).

**Where it is referenced.**
- The combat settings' `HitReactionSettings` and `DefenseConfiguration` slots (`CombatSettings.h:82, 86`) become one
  reaction-profile slot.
- The component's per-instance override stays.
- **A missing profile falls back to the base profile and logs once**, naming the owner and what is lost. This carries over
  the approved, never-started fix for an empty defense slot [D-20261002-10].

### 4.9 The combat settings

`UCombatSettings` is **per character** [D-20261007-53]. It keeps two things: the per-type defaults, and the default kit.
Everything that must agree for everyone left it for the combat rules (4.15). The owner's pick: "Split a global rules
asset from per-character settings".

| Field | Holds | Decision |
|---|---|---|
| Per-type defaults | The table below: layer 1 of the attack's precedence (4.2), read from the attacking character's settings when its resolved attack spec is built (4.11). | D-20261003-08, D-20261004-20, D-20261007-53 |
| Default kit | The kit a character with none fights with (`DefaultKit`, today `DefaultWeaponData`, `CombatSettings.h:78`). | D-20261005-49, D-20261010-02 |
| Reaction profile | The character's reaction profile (4.8), in the place of today's `HitReactionSettings` and `DefenseConfiguration` slots (`CombatSettings.h:82, 86`). | D-20261004-20 |

**Per-type defaults:**

| Field | Light | Heavy | Charged | Special | Decision |
|---|---|---|---|---|---|
| Pressure | 1 | 2 | 3 | Q2 | D-20261005-17 |
| Hit-stun | 0.25 s | 0.45 s | 0.6 s | Q2 | D-20261005-34 |
| Guard breaker | no (yes for directional lights) | no | yes | Q2 | D-20261005-29 |
| Knockback | 25 cm / 0.2 s (code seed) | 20 cm / 0.25 s (code seed) | the Heavy value, scaled by charge, as today | as today | D-20261004-38 |
| Block shove | 10 cm to start; duration open (Q21) | 20 cm to start; duration open (Q21) | the Heavy value (proposed) | Q2 | D-20261005-59, D-20261007-27 |
| Guard-break push | to be tuned | to be tuned | the Heavy value (proposed) | Q2 | D-20261005-59, D-20261007-27 |
| Impact feedback (hit, block, parry) | seeded from today's values; the parry values come from a rendered sweep [D-20261005-78] | same | same | same | D-20261004-20, D-20261004-06 |

The block and parry defaults come from today's defense configuration: the sound `Impact_Sword_Parry_01_Cue` and the effect
`NS_GFXI_Yellow_Parry`, the same for both **[reported]**. They seed the block defaults. The parry's distinct feedback is
chosen from a rendered sweep before the parry stage ships [D-20261005-78].

**Why per character.** The stress test's eighth scenario **[reported]**: a global rule change must reach everyone, and a
per-character type default must not fork the globals. Today one settings asset is both, so a per-character default would
fork every rule with it. With the split, a value changed on the rules asset reaches everyone at once, and a type default
changed on one character's settings reaches that character only [D-20261007-53]. The global values that D-20261007-53
does not name are placed on the rules asset by this spec's reading, and Q23 asks the owner to confirm (4.15).

### 4.10 Retiring the defense configuration

`UDefenseConfiguration` is removed [D-20261003-08, confirmed by D-20261004-20 and D-20261005-48]. Its contents split
three ways:
- **The weapon's responses** go to the defense set (the block reactions, the parry deflects, the guard enter and exit
  clips [D-20261005-55]) and the paired set (the two-character counters and finishers [D-20261005-57]).
- **The body's reactions** go to the reaction profile: the Staggered reaction and recoil [D-20261005-55].
- **The defense tunables** go to the combat rules as defaults, overridable on the defense set: the guard arc, the
  block turn rate, and the guard-facing turn rate and budget [D-20261005-68, D-20261005-69, D-20261005-70,
  D-20261007-53].
- **Global rules** go to the combat rules [D-20261007-53]: threat selection and the slow-motion watchdog. The timers of today's
  post-parry stages are removed. The default sound and effect become impact-feedback type defaults.

The field-by-field map is in 7.4, where today's names can be given.

### 4.11 The resolved attack spec

**Today.** The hit record, `FHitReactionInfo`, carries only a pointer to the attack data (`CombatTypes.h:794, 813`), and
about 215 lines in 10 files read the attack through it **[reported]**. A slot override cannot be seen at hit time.

**Proposed: `FResolvedAttackSpec`.**
- **When it is built.** Once, when an attack starts, from the four layers (4.2), the first read from the attacking
  character's combat settings [D-20261007-53]: from the **resolved slot spec**, the
  attack plus the slot's diff, never from the attack asset alone [D-20261007-35]. Today's defender-side snapshot,
  `FAttackExecutionSnapshot`, copies tags, type and the defense profile from `CurrentAttackData` (**[reported]**
  `CombatComponent.cpp:2268-2279`, the research report §6); it is built from this spec instead, so that a slot override
  reaches the defender.
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
  - the unparryable, unblockable and block-interruptible flags, and the guard breaker: the readable flags the telegraph
    fires from (6.5) [D-20261007-37];
  - swing, lane and height;
  - **every timing window, resolved to montage time** (4.5): the phases, the parry window's lead, the hold start, the
    armor windows, the contact marker and the cancel point [D-20261007-38]. The window scheduler reads them (6.5).
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
- the ground state each role needs (grounded, airborne or any; both grounded by default), its height-difference limit,
  and that its victim must not be invulnerable [D-20261007-42];
- a quiet "can it start here?" check that returns refusal reasons, the ground state, height and invulnerability among
  them. The editor and the "why didn't it play" debug readout use those reasons. A refusal counts as fitting failed
  (3.8) [D-20261007-42].

**One row, one content asset; alternates inside it** [D-20261010-04]. A paired-set row names exactly one content
asset. The content's own internal variants, Duet's angle sector, distance and weight among them, are **alternates** of
that clip: the content may pick among them, through the seam, inside the direction, swing and variety choice the
selection tables have already made (4.16). An alternate never decides the direction. The seam therefore also answers:
- which alternate the content will play for the direction and swing it was asked for, and the start position that
  alternate needs, so that the fit (3.8) is computed against the alternate that will actually play (proposed);
- the ground state each role needs and the height-difference limit, read here and never copied into a table row
  [D-20261007-42, D-20261010-04].

`UPairedAnimationData` implements it first.

### 4.13 Windows and notifies

**Attack data owns every timing window** [D-20261007-38]. The windows of an attack (4.5) are data on the attack, with a
slot diff, anchored to sequence time plus segment [D-20261007-36]. The runtime reads them from the resolved attack spec
(6.5) [D-20261007-39]. **Montage notifies are export and import only.** This rewrites the earlier version of this
section, in which the montage was the source and the rules read it.

| Window | Owner | On the montage | Read by | When unset |
|---|---|---|---|---|
| Phases (Windup→Active, Active→Recovery) | attack data | a derived `AnimNotify_AttackPhaseTransition` mirror | combo timing; the hit window; the attack's cancel point (3.0) | today's defaults; the editor warns |
| Parry window | attack data, as a lead time (R-20261005-01, proposed form) | a derived parry-window mirror | 3.2 | the 0.35 s minimum at runtime, with a warning [D-20261005-74] |
| Contact marker | attack data | a derived mirror | the predictor (3.15); scripted hits (3.3) | the Active start |
| Hold start | attack data | a derived `AnimNotify_HoldWindowStart` mirror | the hold | no hold |
| Armor window on the attack's own section | attack data, with a level | a derived mirror | 3.6 | base armor only |
| Armor window on another character animation | the animation (a notify state carrying the level; proposed) | the notify itself | 3.6 | base armor only |
| Cancel notify | the montage, placed by hand (unchanged by D-20261007-38; 3.0) | the notify itself | 3.3, 3.4, 3.6, 3.7 | montage end, with a warning [D-20261006-02] |
| The dodge's i-frame window and cancel point | the dodge's data [D-20261007-50] | a derived mirror (proposed) | 3.14 | no invulnerability; cancel at montage end (proposed) |

**Export to montage** [D-20261007-38]. The existing generator, `FAttackDataNotifyGenerationService`
(`Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`; **[reported]** it writes phase transitions
and the hold start into the montage section from `ManualTiming`, lines 379–429 at `origin/main`, and classifies each
canonical notify as matching, stale or missing), becomes "export to montage". It is extended to the parry window, the
contact marker and the attack's armor windows, and hardened to the animation editor's own write steps (**[reported]**:
link to the segment, a guid, a transactional object, the trigger offset for a section boundary, `PostEditChange`). The
exported montage carries a fingerprint of the data that generated it (proposed; the pattern the research report cites).

**Adopt from montage** [D-20261007-38]. The timing action merged in #139 [D-20261004-33], `FAttackTimingDerivationService`
(**[reported]** it reads the phase and hold notifies of the section and probes the generator before writing), becomes
"adopt from montage": a one-shot, transacted read of a hand-authored montage's notifies into the attack's data windows,
extended to the parry window. It is the migration path for today's 29 attacks (7.3). Export and adopt are explicit
commands; neither is fired by an edit to the other side, so no loop can form.

**Derived notifies in the animation editor** [D-20261007-40]:
- they draw in a **distinct colour**, with **"(from attack data)"** in their name, so the designer sees which notifies
  are mirrors;
- **a hand-moved derived notify changes nothing in play**: the runtime reads the data;
- **on saving the montage**, a fingerprint mismatch prompts **Save / Reset / Leave**:
  - **Save** writes the dragged positions into the attack data (and, where the montage section is shared, into every
    attack that uses it, after the used-by notice);
  - **Reset** regenerates the notifies from the data;
  - **Leave** saves the montage as dragged, the data untouched, the mismatch flagged, with no effect in play.

  The owner first asked for "Keep, Reset, Leave", then, told what Leave does, "I think Save/Reset/Leave is best".
- **A stale mirror cannot go quiet** (**proposed**, the 2026-10-07 log, E23): after Leave, or after an animation edit
  under the windows, the mismatch also appears in the project-wide health report (5.6) and as a validation warning on
  every attack that uses that montage (4.4). The owner forwarded the consequence in the side-agent's words: "choosing
  'Leave' means the animation editor is lying about timing until someone resolves it."

**The cancel notify's shape** (proposed; D-20261006-02 fixes its meanings, not its class): one notify class with no
payload, placed once per montage. A second one on the same montage is an error. Attacks never carry it: their cancel
point is the Active→Recovery boundary, held as data; a cancel notify on an attack montage is a warning and is ignored
(4.4).

### 4.14 The dodge data

**Proposed**, to give the dodge's tunables the home the owner named: "the dodge's own data; settings defaults" (the
2026-10-07 log, E25). Those defaults sit on the combat rules by this spec's reading of D-20261007-53, decided later the
same afternoon (Q23).

- **Proposed class:** `UDodgeData`; **proposed asset name:** `DA_Dodge_<Character>`.
- **It holds:** the dodge clips (eight-way or free-angle, a content choice [D-20261007-49]); the distance and duration,
  each a toggle plus a value over the combat-rules default (Q23); the i-frame window and the cancel point, as data windows
  anchored like an attack's [D-20261007-50, D-20261007-51]; and the neutral-stick rule's fallback, straight back, as a
  constant [D-20261007-49].
- **Referenced by** the character, beside its reaction profile: the dodge is the body's move, not the weapon's
  (proposed; by D-20261005-68's "a tunable lives with the thing it describes"). A character with none cannot dodge, and
  validation warns on the player.
- **Its windows** follow 4.13: data-owned, exported to the dodge montage as derived notifies, driven at runtime by the
  same scheduler as an attack's (6.5).

### 4.15 The combat rules

**Decided** [D-20261007-53]: a project-wide **combat rules** asset is split from the per-character combat settings. It
holds "everything that must agree for everyone": the owner's list is the window minimum, the lockout, the slowdown, the
Staggered default, the aim cone, the ranges, the attacker cap and cooldowns, the predictor settings, the telegraph set
and the counter damage default. The per-character settings keep the type defaults and the default kit (4.9).

**Proposed class:** `UCombatRules`. **Proposed asset name:** `DA_CombatRules`. **Exactly one per project (proposed):**
the asset is named from a developer-settings entry (a `UDeveloperSettings` subclass, saved in `Config/DefaultGame.ini`),
so that there is one, it is found without opening a character, and a second one cannot be referenced by mistake; this is
the stress test's seventh item **[reported]**. No character references it.

**What it holds.** Two groups. The first is the decision's own list. The second is every other global value this spec
had placed on "the combat settings" that is not a per-type default; it is here by elimination, since the settings keep
only type defaults and the default kit, and that placement is a reading the owner should confirm (Q23).

| Rule | Value | Named by D-20261007-53? | Decision |
|---|---|---|---|
| Minimum parry window | 0.35 s before contact | yes (the window minimum) | D-20261005-09 |
| Parry lockout | 0.25 s, lenient: a press during it is honoured if the window is still open when it ends; it may go to zero | yes | D-20261005-32, D-20261007-18 |
| Attacker slowdown | no lower than 50% speed, at most 0.3 s; the ease shape tuned on captures (proposed) | yes | D-20261005-33, D-20261005-76 |
| Staggered time default | 1.5 s | yes | D-20261005-08 |
| Aim cone | 60° either side of the stick direction, or of the facing when no direction is held | yes | D-20261005-27, D-20261007-12 |
| Finisher and follow-up range | 350 cm | yes (ranges) | D-20261005-28, D-20261005-85 |
| Lock range, and "near" for the automatic re-pick | Q19 | yes (ranges) | D-20261007-13 |
| Most attackers on one target at once | 2; the tokens count the attackers of whoever is targeted | yes (the attacker cap) | D-20261005-35, D-20261007-54 |
| Per-enemy attack cooldown | length Q6; starts when the token returns | yes (cooldowns) | D-20261005-35, D-20261007-30 |
| Re-grant delay | no decided value, "short" | yes (cooldowns, a reading) | D-20261007-30 |
| Resume delay default | no decided value; per-profile override | yes (cooldowns, a reading) | D-20261007-29 |
| Predictor tolerance and horizon | no decided value; per-attack overrides | yes (predictor settings) | D-20261007-24 |
| Predictor thresholds (parry window, finisher choice, lethal strike) and confidence scale | Q20 | yes (predictor settings) | D-20261007-24 |
| Telegraph set | cues per readable flag, chosen on captures; the pooled FX asset holds them (6.5) | yes | D-20261007-37 |
| Counter damage default | 50, under each defense row's value and any clip override | yes | D-20261005-80, D-20261006-03 |
| Guard arc default | ±75°, overridable on a defense set | no (Q23) | D-20261005-11, D-20261005-70 |
| Block turn rate default | 360°/s, overridable on a defense set | no (Q23) | D-20261005-69 |
| Fastest natural turn default | 720°/s, overridable on a defense set | no (Q23) | D-20261005-75, D-20261005-68 |
| Hit-stun floor | 0.12 s | no (Q23) | D-20261005-34 |
| Follow-up strike multiplier | ×1.5 of the resolved attack's damage | no (Q23) | D-20261005-39, D-20261005-82 |
| Flick threshold | no decided value; tuned on captures (proposed) | no (Q23) | D-20261007-13 |
| Lock grace | no decided value, "short" | no (Q23) | D-20261007-48 |
| Shove-back distance | no decided value; tuned on captures | no (Q23) | D-20261007-44 |
| Fitting budget defaults (turn, distance, room) | no decided value; the paired set overrides | no (Q23) | D-20261007-23 |
| Height-difference limit default | no decided value; "defaults keep today's behaviour" | no (Q23) | D-20261007-42 |
| Dodge defaults (distance, duration, i-frame window) | no decided value; the dodge's data overrides (Q22) | no (Q23) | D-20261007-21, D-20261007-50 |
| Guard-facing threat selection, turn rate and budget | today's values, unchanged (3.11); the turn rate and budget overridable on a defense set, since they are weapon behaviour (proposed under D-20261005-68) | no (Q23) | — |
| Slow-motion watchdog | today's value, unchanged | no (Q23) | — |

**What it does not hold:** the per-type defaults (pressure, hit-stun, guard breaker, knockback, block shove, guard-break
push, impact feedback) and the default kit, which are each character's combat settings (4.9); and the values that live
with the thing they describe [D-20261005-68]: a weapon's overrides on its defense set, a paired set's fit budgets, a
character's profile fields, the dodge's own numbers.

**Which asset is read.** The one rules asset, by everyone, always. The question the review of sections 6 to 9 raised,
whose combat settings a rule reads, does not arise for a global rule any more; for a per-type default the answer is the
attacking character's settings (4.1, 4.2).

### 4.16 Selection tables

**Decided** [D-20261007-57]: the counter grid, the finisher lists, the enemy's slot choice and the opener and entry
rules share **one column vocabulary, one evaluator and one "rows evaluated" trace**, shown in a simulate-press panel
(5.12). They are built on **in-house table structs**; Epic's Chooser is a reference for its columns and its debugger's
shape, not a dependency. The owner's pick: "Adopt, with in-house table structs". The stress test's reason
**[reported]**: a parry-counter was three assets deep with no single place that said why it did not play, and counter
cells, finisher lists and enemy weights were three shapes for one kind of question.

**The column vocabulary.** Every table's key columns are drawn from this list; a table uses the ones its question needs
and leaves the rest "any":

| Column | Values | Used by |
|---|---|---|
| Input | Light, Heavy, Block, Evade, or a hold-and-release direction | openers and entry rules; slot links stay topology, not rows |
| Combo step | the slot the combo is at, or none (the stress test names this column with a retired word; this spec says combo, Appendix B) | entry rules |
| Stance or ground state | grounded, airborne, running; any | openers and entry rules; enemy slot rows |
| Target state | Staggered, in hit-stun, guarding, none; any | enemy slot rows |
| Direction | front, left, right, behind | counter rows; finisher rows |
| Swing | slash left, slash right, overhead, thrust; any | counter rows |
| Lane | left, centre, right; any, preferred when it matches | counter rows |
| Lethal | lethal, not lethal; any | in the vocabulary; unused by default (4.6, a reading) |
| Distance range | a minimum and a maximum | enemy slot rows; finisher rows may narrow by it (proposed) |
| Weight | a number, 1.0 by default | enemy slot rows |

**The tables:**

| Table | Home | Key columns | Output |
|---|---|---|---|
| Openers and entry rules | the combo set (4.4) | input; stance or ground state; combo step | the slot a press starts from |
| Slot rows (the enemy's choice) | the combo set's slots (4.4) | weight; distance range; target state; stance | the slot the enemy attacks with |
| Counter rows | the defense set (4.6) | direction; swing; lane | deflect, damage, fallback strike, seeks a paired counter |
| Paired counter rows | the paired set (4.7) | direction; swing; lane | one paired-content asset, kill sequence optional, damage override optional |
| Finisher rows | the paired set (4.7) | direction (distance range optional) | one paired-content asset |

**One evaluator** (proposed, following the pure selection function of 6.2): `Evaluate(Table, Context) -> { Chosen row or
none; Trace }`. It judges every row's key columns against the context, keeps the rows that pass, prefers a row that names
a lane over one that does not where lane applies ("lane preferred" [D-20261010-01]), applies the fallback order where
the table has one (3.3), applies the variety rule among the rows left (one always plays; two alternate; three or more
random, excluding the last two), and returns the row with the trace. The same evaluator serves the game, the editor's
simulate-press panel and the tests; a table built in code for a test is a plain struct, with no plugin [D-20261007-57].

**The trace.** For one press: every row evaluated, each key column's pass or fail with the context value it saw, the
rows left after the fallback order, the variety rule's exclusion, and the row chosen or the reason none was. The
paired-content seam's refusal reasons (4.12) are appended when a chosen row's content refuses to start. The debug
readout of 6.2 and the simulate-press panel (5.12) show the same trace.

**Paired lookup by key** [D-20261010-01]. A defense row that seeks a paired counter, and a lethal strike that needs a
finisher, are each one more evaluation: of the kit's paired set's counter rows with the defense row's direction, swing
and lane as the context, or of its finisher rows with the attacker's direction. Nothing references across sets, so
validation checks coverage, not references (4.3).

**Chooser as reference, not dependency.** Epic's Chooser (`UChooserTable`) has the column-per-context shape and a row
debugger; the stress test used it for the column vocabulary and the debugger's shape and did not verify its API status
in 5.6 **[reported]**. The plugin is enabled in `KatanaCombat.uproject` **[reported]**; the tables here do not use it.

### 4.17 Alternatives considered

The owner asked for a research pass before this model was kept: "Are we using the best model for data storage and
authoring surface placement? do a research pass for best practices … without falling victim to availability bias to our
existing system" (the 2026-10-07 log, E26). Two read-only reports came back, both model-written and saved in
`.superpowers/design/`: `research-data-model-stress-test.md` and `research-data-model-survey.md`. On them the owner
kept the storage model [D-20261007-56] and the override breadth [D-20261007-59], and adopted two hybrids.

**The eight models stress-tested** (twelve scenarios each; the scores are the report's **[reported]**): A, this
proposal, weapon → combo set + defense set + paired set with a reaction profile per character; B, one moveset asset
with every move inline; C, an ability-and-tags model with chains as tag requirements; D, table-driven selection, as
Epic's Chooser; E, timeline-first, with data on the montage; F, a state machine per weapon; G, fragment data; H,
everything on the character, which is today. The report's reading: B wins only with one weapon and one owner; C is the
wrong tool for an authored combo; E is the best place to see a window and the worst place to share one; F rewrites the
state handling for little; G is A with less schema; D beats A exactly on selection and its debugging.

**The two hybrids adopted:** H1, table-shaped selection with one trace (4.16) [D-20261007-57]; H2, the timeline as the
attack editor over the same data (5.11) [D-20261007-58]. They are orthogonal and move no field.

**The survey's plain counter-evidence, and the owner's reasons for staying:**
- **ComboCreator allows overrides on payloads only:** "Whole attacks are never overridden, because that is where silent
  divergence lives" (`2026-09-19-combocreator-design.md:95-98`, as the survey cites it **[reported]**). The owner kept
  every field overridable; the diff-only row, the source labels, the named pills for readable overrides, Save to asset
  and Extract to new attack are the guards against that divergence [D-20261007-59].
- **Ninja Combat owns defense on the target**, with a defense data asset per character or group and the weapon
  attacker-side only (https://tech.ninjabear.studio/cbt-concept-damage-and-mitigation.html, per the survey
  **[reported]**). The owner kept the split: what the weapon animates on the weapon, what the body decides on the body
  [D-20261010-03].
- **No shipped product references a paired set from a weapon:** Ninja and ACF key paired content from the ability or
  action, Duet from an independent asset with selection criteria (the survey, "Rule: weapon → combo set / defense set /
  paired set" **[reported]**). The survey calls this a note that no one has shipped it, not an argument against it; the
  owner kept the kit's paired set, found by key [D-20261007-56, D-20261010-01].
- **Shipped products gate combo input on the window:** Ninja Combat's combo window ("Input while the combo window is
  closed: usually ignored, unless some sort of Input Buffer is active", https://tech.ninjabear.studio/cbt-concept-combos.html,
  per the survey **[reported]**), and DCS puts the parry window on the defender (https://ue4dcs.com/dcs/System/ParrySystem,
  per the survey **[reported]**). Both are deliberate departures here: input is never lost (1.3) and the parry window is
  the attacker's [D-20261005-13, D-20261005-37, D-20261007-38].

The decisions are the authority; the reports are pointers to their evidence, and their counts and judgments stay marked
**[reported]** (Appendix B).

---

## 5. Editor UX

The owner chose a moveset grid with a read-only node map [D-20261005-02], and asked for an improvement on OnSight rather
than a copy of it (the 2026-10-04 log, E45). Everything in this section is proposed, except what D-20261007-57 and
D-20261007-58 decided: the simulate-press panel with the row trace (5.12), and the timeline as the attack editor with
the grid row's strip as a link to it (5.11); their details are proposed.

### 5.1 The moveset grid

- **Rows** are slots, grouped by lane: light, heavy, directional.
- **Columns** are inputs: Light, Heavy, and hold-and-release Light toward Forward, Back, Left and Right.
- **Each cell** is a chip for the link's target. It shows the slot name and the montage section. A back-to-opener link
  shows a loop icon, and an empty cell means no link.
- **Each slot's cell carries a diff marker** [D-20261007-35]: a count pill for the diff, plus a named pill for any
  readable override ("guard breaker", not "+1"), in the diff colour, not a severity colour.
- **Drag a row** to reorder it within its lane [D-20261005-02]. The lane's Light or Heavy links are rewired so that they
  follow the new order. The rewiring rule is proposed.
- **Drop a slot on a cell** to make a branch.
- **Right-click a cell** to set "back to opener" or to clear it.
- **"Append to lane"** creates the next slot, already linked.
- **"New attack from montage section"** creates an attack and its slot in one step.
- **Each drag is one undo step.** The selection is held by slot id, so undo and redo keep it.
- **Opening a slot opens the timeline** (5.11); the row's timing link (5.4) is the way in [D-20261007-58].
- The kit's defense set and paired set have their own editors (5.9), and every selection table has the simulate-press
  panel (5.12).

### 5.2 The node map

- Read-only.
- Lanes are columns, and links are arrows labelled with their input.
- A back-to-opener link is a return arrow with the loop icon.
- Clicking a node selects its row in the grid.

### 5.3 The slot row: the diff

The slot is an attack plus a diff, and the row shows the diff [D-20261007-35]. The owner rejected read-only rows ("the
user still gets the full deluge of info without being able to do anything about it") and a flat list of everything.

- **The row at rest shows only the diff**, in four collapsible groups with counts, for example "Impact: 1 of 11":
  Links & AI; Impact; Identity; Timing & reach, which holds the timing link (5.4) into the timeline (5.11). The group
  names are the design pass's and are proposed.
- **"Show inherited"** reveals the greyed inherited values, each with its source: "from combat settings (Light): 25 cm",
  "from set" (hidden while the set layer is hidden, 4.2 [D-20261007-34]), or "from attack".
- **An overridden field** shows the slot's value, the attack's value struck through, the source label, a **"Save to
  asset"** button and a reset arrow. Save to asset writes the value into the attack and clears the override, after the
  used-by notice [D-20261007-35].
- **Turning an override on** seeds it with the current value.
- **"Extract to new attack"** sits on the row header with the used-by line; it replaces "make unique" (4.4)
  [D-20261007-35].
- **A readable override** (guard breaker, unparryable, unblockable, block-interruptible) shows the telegraph cue that
  will fire for it, with "from combat rules" (6.5) [D-20261007-37, D-20261007-53], and a warning when the same attack is reachable
  from another slot without the override (4.4, proposed).
- **The edit-through section** shows the attack's own fields, with "used by N slots in M sets" before the first edit.
- **An "Auto-detect swing" button** on the attack card and on the attack asset's details panel, and the same action in
  the Content Browser on one or many attack assets, computes the `Swing` field from the weapon trace path and writes it
  once; the field stays editable by hand, and a value set by hand is marked so that re-running the action asks before
  overwriting it [D-20261005-84].

### 5.4 The timing link

Each slot row shows a **read-only summary strip** of its attack's data windows (4.5): the phases, the authored and
effective parry window, the hold start, the armor windows and the contact marker, with the slot's overrides marked. It is
a **link**: clicking it, or opening the slot or the attack, opens the timeline (5.11), where the windows are edited over
the frames they govern [D-20261007-58]. The owner asked "Does A allow for visual notify movement in the attack set or at
least the attack data?"; the first answer was an editable strip inside the row, and D-20261007-58 replaced it with the
timeline: a strip in a row was OnSight's collapsed preview in a new coat (the stress test, **[reported]**). Dragging in
the timeline edits the data directly [D-20261007-38]; the strip only shows it.

### 5.5 Validation badges

- Each badge sits on the row or cell it concerns. It states the problem and the fix, and it navigates there in one click.
- The rules are those of 4.3 to 4.7.
- Loops through a back-to-opener link are shown, not flagged.

### 5.6 Who uses this set

- A combo set, defense set or paired set lists the kits that reference it, and each kit lists the characters and AI
  that use it [D-20261010-02].
- Each attack lists the sets it appears in.
- A paired set's counter rows list the defense-set rows, across the kits that carry both, whose key reaches them; nothing
  references a paired row, so "used by" here means "reached by key" [D-20261010-01].
- A project-wide report lists attacks that no set references, paired counter rows that no defense row's key can reach,
  and directions with no counter or finisher row (coverage).
- The same report lists **stale mirrors**: montages whose derived notifies differ from the attack data that owns them,
  with the attacks affected (**proposed**, 4.13).

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

Both editors show **table rows** with the coverage colouring: every selection table (4.16) is edited as rows whose key
columns are pickers, and a cell or row is coloured by what would play for it, so that a gap is seen before it is felt
[D-20261005-30, D-20261007-57]. Both carry the simulate-press panel (5.12).

**The defense set editor:**
- **Block tab:** one row per direction (front, left, right, behind) with the block reaction; the guard enter and exit
  clips above them [D-20261005-55]; the rows outside the guard arc are greyed while the arc is ±75°.
- **Counter tab:** the counter table as a grid with the four directions as rows and the four swings as columns
  [D-20261005-45]; each cell is a counter row (4.6). The cell shows its deflect and its fallback strike as two chips, an
  empty chip for missing content, a **"seeks a paired counter"** toggle, and the row's counter damage with its source
  (the row, or the rules default) [D-20261006-03]. When the toggle is on, the cell is coloured by **coverage**: whether
  the kit's paired set has a counter row for this direction and swing, found by key, with the one that would play named
  on hover; when off, or when none is found, by the fallback strike that would play [D-20261005-30, D-20261010-01].
  Optional lane refinements appear as sub-rows. Clicking the coverage colour opens the paired set at those rows.
- **Pickers** for every clip; there is no counter-reference picker, since nothing is referenced [D-20261010-01].

**The paired set editor** [D-20261005-57]:
- **Counter tab:** the counter rows, one group per direction; every row shows its content and, when it has one, its
  authored kill sequence as two chips, with its swing and lane keys and any damage override [D-20261007-07,
  D-20261006-03]. A direction with no row is marked; a counter with no kill sequence is noted, not flagged, since it
  kills at its own contact. Each row names the defense-set rows whose key reaches it, across the kits that carry this
  set.
- **Finisher tab:** the finisher rows, one group per direction, in play order for the variety rule [D-20261007-31]; an
  empty direction is a warning, and it names the authored direction a fit would use [D-20261005-47, D-20261007-23]. The
  fitting budgets (turn, distance, room) sit above the rows with their source, the set or the rules default
  [D-20261007-23]. Each row's content shows, read through the seam, the ground state it declares per role, its height
  limit and its alternates [D-20261007-42, D-20261010-04].
- **Player finishers:** the rows that hold player-finisher content are grouped and counted; the set's header says
  whether it can finish the player, from that count [D-20261007-55].
- **Coverage strip:** the four directions, coloured by whether a counter row and a finisher row exist, so missing
  directions are seen at a glance; an uncovered finisher direction shows the fit it would take.
- The paired content's "can it start here?" refusal reasons are shown on hover (4.12).

### 5.10 What this improves on

- **Over OnSight's three flat card lists**, and ComboCreator's design (which has no UI yet): one grid plus a map,
  per-field overrides that name their source, coverage-coloured table rows for counters and finishers, the timeline as
  the attack editor, and one simulate-press trace for every selection. OnSight keeps its timing preview collapsed; the
  timeline opens the windows over the frames they govern [D-20261007-58]. ComboCreator's timeline engineering (one
  transform, one widget per window, one transaction per gesture, frame snapping over stored seconds) is reported as
  directly reusable here by the stress test, which judged from its spec and did not open its editor source
  **[reported]**.
- **Over today's pain points [reported]:**
  - swapping light hits 3 and 4 means editing 3 assets;
  - changing the heavy branch partway through the light combo means editing 8 assets;
  - changing forward routing means editing 11 assets;
  - finding where an attack is used takes 4 hops.

  The editor stage measures the clicks each of these tasks takes, against today's editor.

### 5.11 The timeline

**Decided** [D-20261007-58]: opening an attack or a slot opens a **timeline** whose lanes are the data windows drawn
over the montage; the grid stays for topology, and the row's strip becomes a link to it (5.4). The owner's pick: "Adopt;
the grid row's strip becomes a link to it".

**Lanes**, over the montage section as the backdrop, each a data window of 4.5 or a moment the attack owns:
- **the attack lane and the slot lane** for each window, with an **authoritative mark** on the lane whose value the
  runtime reads: the slot's when the diff sets it, else the attack's [D-20261007-35, D-20261007-38];
- **phases:** the Windup→Active and Active→Recovery boundaries, the second being the attack's cancel point (3.0);
- **hold start**;
- **contact:** the contact marker, with the parry window drawn as its lead, authored and effective, the extension
  hatched; the window's end stays pinned to the marker, so a drag changes the lead (3.2);
- **armor:** the attack's armor windows with their levels (3.6);
- **hits:** the hit window inside Active and the hits the attack may land (`MaxHitCount`); a reading of the decision's
  word, Appendix B;
- **telegraph:** the cue that fires at attack start for each readable flag the slot's spec carries, named "from combat
  rules" (6.5) [D-20261007-37, D-20261007-53];
- **FX moments:** the impact feedback per outcome and its moment, as the attack's impact block has it (4.5); a reading,
  Appendix B;
- **the combo lane:** the links out at the combo point, Light, Heavy and each hold-and-release direction, as chips that
  open the target slot's timeline; a back-to-opener link shows the loop icon.

**Editing.** Dragging a window edits the data directly [D-20261007-38]: on the slot lane the slot override, on the
attack lane the attack's value after the used-by notice; each drag is one undo step; turning a slot lane on seeds it with
the attack's value. The montage's derived notifies are shown as a **mirror lane** with a stale badge when they differ
from the data, carrying the Save / Reset / Leave actions (4.13) [D-20261007-40]. A warning appears when the animation
has changed under its windows.

**Persona export is optional** [D-20261007-58]. Nobody needs to drag a notify in the animation editor once the timeline
edits the data over the frames, so "export to montage" becomes a command the designer runs when the mirror is wanted;
the Save / Reset / Leave prompt stays decided for the montages that carry one [D-20261007-40], and its weight drops.

**Preview.** The timeline scrubs the montage in a viewport, and the simulate-press panel (5.12) sits beside it, so that
a press at a chosen frame shows the rows evaluated against this slot.

**Reusable engineering [reported].** ComboCreator's timeline (one transform, one widget per window, one transaction per
gesture, frame snapping over stored seconds) is reported as directly reusable by the stress test, which judged from its
spec and did not open its editor source; D-20261007-36 already adopted its anchoring. It is the shape to follow, and
whether it is code to copy is a question for the editor stage.

### 5.12 The simulate-press panel

**Decided** [D-20261007-57]: one "rows evaluated" trace, shown in a simulate-press panel. The panel takes a pretend
press and its context and shows the trace of 4.16 before play.

- **Inputs:** the press (Light, Heavy, Block, Evade, or a hold-and-release direction); the current slot or none; the
  stance or ground state; the target's state (Staggered, in hit-stun, guarding, none), direction, distance and remaining
  health; for a Block press, the incoming attack's swing and lane; for an attack press, whether the strike would be
  lethal. Every input has a default, so that one click shows something.
- **Output:** every table the press would consult, in evaluation order (opener or entry rule; slot row; counter row;
  paired counter row; finisher row), each as the rows evaluated with every key column's pass or fail, the rows left
  after the fallback order, the variety rule's exclusion, the row chosen or the reason none was, and the paired content's
  refusal reasons when its "can it start here?" check would refuse (4.12).
- **It runs the same evaluator as the game** (4.16), so what it shows is what would play; a debug readout in play shows
  the same trace for a real press (6.2).
- It sits in the combo set editor, in the defense set and paired set editors (5.9) and beside the timeline (5.11).

---

## 6. Runtime flow

### 6.1 State

- the kit in use: the character's own, or its combat settings' default kit [D-20261005-49, D-20261010-02];
- the weapon and the current combo set, defense set and paired set, from that kit [D-20261005-48, D-20261005-57];
- the combat rules, one asset read by everyone [D-20261007-53];
- the current slot id;
- the combo state machine (`FAttackStateMachine`, `CombatTypes.h:1736`);
- the resolved attack spec of the running attack;
- the parry lockout timer, and the press remembered during it [D-20261007-18];
- the targeting mode, free or lock, the locked target, and its out-of-range or out-of-sight grace (3.13);
- the guard-break ledger;
- the Staggered and hit-stun timers;
- the dodge: playing or not, its i-frame window and cancel point (3.14);
- the push in flight, if any (3.10);
- the data windows of the running attack, driven by the window scheduler (6.5);
- the contact predictor's latest result per attack in progress (3.15);
- the attack tokens, each held until its attack ends, with its cooldown and the re-grant delay (3.9).

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

This is the one evaluator of 4.16 applied to the combo set: the links are judged as topology, the openers and entry
rules as rows, and the rejections are the row trace [D-20261007-57]. The same evaluator judges the counter rows, the
paired rows and the enemy's slot rows, so every "why didn't it play" is one trace, in play and in the simulate-press
panel (5.12).

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
3. The target is Staggered: a follow-up strike. A light press uses the light opener, a Heavy press the heavy opener, from
   the initial press; the hold and the charge do not open [D-20261007-17, D-20261007-33]. If it is lethal and the
   predictor says it lands, a paired finisher (3.3, 3.8).
4. Otherwise, the lethal check on the attack the press resolves to, with the predictor's confirmation that it lands: a
   paired finisher from the direction's list, fitted when the direction is uncovered; an ordinary attack when fitting
   fails (3.8) [D-20261005-42, D-20261007-22, D-20261007-23, D-20261007-31].
5. Otherwise, select the transition through the evaluator (6.2), start the attack, and build its resolved attack spec
   from the slot's spec (4.11), its type defaults from the attacker's combat settings [D-20261007-53].
6. At attack start the telegraph fires for each readable flag in the spec (6.5) [D-20261007-37], and the window
   scheduler takes the spec's windows (6.5) [D-20261007-39].
7. In lock mode, a strike on an enemy other than the locked one transfers the lock to it (3.13) [D-20261007-46].

**A buffered press fires** when the state that held it ends; at its attack's hold-window start the button state is
checked as usual [D-20261007-45].

**A lock press** (3.13) [D-20261007-13]:
1. Not locked: pick the best target by the cone and its scores; lock on it, and strafe facing it.
2. Locked: drop the lock.

**A look-stick flick, or a scroll, while locked:** switch to the nearest enemy in that direction [D-20261007-13].

**A directional light while locked:** aimed by the stick as in free mode; the lock transfers to the enemy that way, or
breaks when there is none and the attack misses [D-20261007-47].

**The lock ends or moves** on the events of 3.13 [D-20261007-48]: the target dies (re-pick when one is near); out of
range or sight for the grace; a second press; the player Staggered or finished; a directional light; a Block that blocks
a different attacker (a reading).

**A dodge press** (3.14), in the order of its table [D-20261007-49 to D-20261007-52]:
1. The player is in hit-stun or Staggered: the press waits for the escape to open, both minimums when both run, then
   ends the lock, the enemy's opening with it when Staggered, and the committed dodge plays (3.4, 3.7)
   [D-20261005-87, D-20261005-88, D-20261006-01].
2. Inside the player's own sequence: buffered to the cancel notify (a reading).
3. In Windup or Active: never cancelled; buffered to Recovery (a reading). In Recovery: the dodge ends it.
4. Guard drops; a push in flight is cancelled [D-20261007-51, D-20261007-32].
5. During a dodge: buffered to its cancel point; consecutive dodges are allowed [D-20261007-52].
6. The dodge plays toward the stick, camera-relative; with a neutral stick, away from the current threat, or straight
   back [D-20261007-49]. Its i-frame window ignores contacts; after its cancel point an attack press starts the opener
   [D-20261007-50, D-20261007-51].

**A Block press:**
1. The guard rules (3.1).
2. The parry rules (3.2), including the press remembered through a lockout [D-20261007-18].
3. The parry-counter (3.3): content, damage, outcome; then the turn, the parry and the counter; then each character's
   cancel notify.

**A weapon contact** (this order is proposed; today the resolver decides the block before anything else):
1. Block resolution (3.1).
2. Guard break (3.5).
3. Interruption (3.6), with the guard ending on an interrupting hit through or around it [D-20261007-25].
4. Hit-stun (3.7).
5. Knockback (3.10).

**All the weapon contacts of one frame** [D-20261007-19]: each is judged against the state at the frame's start; damage
applies first for all of them; a killing hit replaces the reaction; among the surviving interrupting hits on one victim
the highest pressure, then the latest, supplies the reaction, hit-stun and push (3.6).

**A scripted hit** (3.3) [D-20261007-41]: applied at the clip's contact marker without a trace; the counter's applies no
hit-stun or push, a follow-up's applies its attack's.

**An enemy chooses its next attack** (3.8, 3.9) [D-20261007-20, D-20261007-29, D-20261007-30]:
1. Others hold: no new attack while the player is in a paired sequence; a token granted but not started is returned; a
   holding enemy re-enters the queue after its resume delay when the sequence ends.
2. A token against the target is taken and held until the attack ends for any reason; the cap counts the attackers of
   that target [D-20261007-54]; the cooldown runs from its return, and a freed token waits the re-grant delay.
2a. The slot is chosen among its kit's combo set's slot rows by their selection columns (weight, distance range, target
   state), through the one evaluator, with the trace kept for the debug readout [D-20261005-63, D-20261007-57].
3. A marked enemy whose next attack would be lethal, against a player who is Staggered or in hit-stun, with the
   predictor confirming the hit at the strict threshold, chooses the finisher as that attack, at its start. A swing in
   progress is never promoted.

**A paired sequence starts** (3.9) [D-20261007-44]: a second enemy whose swing is in progress aborts with a recoil or
abort reaction, is shoved back a short settings distance, returns its token, and holds.

### 6.4 Components that change

| Component | What changes |
|---|---|
| **Combat** (`UCombatComponent`) | The kit in use; the combo set and current slot; the one table evaluator and its trace [D-20261007-57]; the resolved spec at attack start, built from the slot's spec, and `FAttackExecutionSnapshot` built from it rather than from `CurrentAttackData` [D-20261007-35, D-20261007-38]; the window scheduler and the data identity for windows (6.5) [D-20261007-39]; the telegraph at attack start [D-20261007-37]; guard dropping when an attack starts, and Block buffered until Recovery; the parry lockout, with the press remembered through it [D-20261007-18]; hit-stun holding attacks and movement, with the dodge escape after its minimum [D-20261005-87]; the dodge (3.14) [D-20261007-49 to D-20261007-52]; the target of every attack press, from Targeting (3.13); presses buffered inside the parry-counter until the player's cancel notify [D-20261007-09], honouring the hold when they fire [D-20261007-45]; the contact predictor's per-tick evaluation for the attack in progress (3.15), on a pure library over the trace path (proposed). |
| **Targeting** | Free mode: the aim cone around the stick direction, or the facing when no direction is held, Staggered or eligible enemies first and the nearest otherwise, for every attack press, with 350 cm for a paired finisher and a follow-up strike [D-20261007-12]; lock mode: the lock press, the locked target for every press, the strafe facing it, the flick (or scroll) switch, the drop on death with the automatic re-pick, and the second press [D-20261007-13]; the lock following action, with the strafe re-centring [D-20261007-46]; the directional light aimed by the stick while locked, transferring or breaking the lock [D-20261007-47]; the events that end a lock, with the grace [D-20261007-48]; a new lock input action; today's soft aim retired. The code-driven turn for block and parry (the adaptive rate and its cap), through the alignment arbiter. Motion warping stays for attacks and paired content [D-20261004-21]; the approach warp of a fit to clip goes through it [D-20261007-23]. |
| **HitReaction** | Pressure against armor, armor windows and the flinch; armor 0 while Staggered, the base in hit-stun, super armor flinching [D-20261007-26]; the contact order within a frame [D-20261007-19]; the guard ending on an interrupting hit and returning while Block is held [D-20261007-25]; hit-stun from the resolved spec, with its floor, holding the AI too; one Staggered state using the causing attack's time, and always the player's own time for the player [D-20261006-06], running beside hit-stun with both ending before the character is free [D-20261006-01]; the cancel notify on hit and Staggered reactions [D-20261006-02]; the guard-break count, built dormant [D-20261007-15]; knockback by outcome, with the block shove and the guard-break push on their own defaults [D-20261007-27], no push on a flinch, a killing hit or in a paired sequence, a push in flight cancelled only by a dodge [D-20261007-32]; the dodge's i-frame window [D-20261007-50]; the reaction profile in place of the hit-reaction settings, with the player identified by its fields and no is-player branch [D-20261007-54]; death reading the variants array. |
| **PairedAnimation** | The parry-counter: content, damage and outcome at the press; the turn, the deflect and the counter; damage at the counter's contact, never clamped; the kill at contact, or the authored kill sequence; each half's cancel notify, and the protection until it [D-20261006-03, D-20261006-05, D-20261007-07, D-20261007-08]; the defense set's counter entries, the paired set's counters and their fallback; scripted hits at the contact marker for the single-character counter and the follow-up strike [D-20261007-41]; the follow-up strike and the finisher rows of the attacker's kit's paired set, found by key, with the variety rule [D-20261007-31, D-20261010-01]; the counter rows found by key for a defense row that seeks one [D-20261010-01]; the alternates chosen by the content inside our choice [D-20261010-04]; whether a kit can finish the player, from its paired set [D-20261007-55]; fit to clip with its budgets and the seam blend [D-20261007-23]; the per-clip ground state, height and invulnerability checks [D-20261007-42]; the paired-content interface. The post-parry timers of today go. The paired marker verification that re-reads `Notifies[NotifyEventIndex]` (**[reported]** `PairedAnimationComponent.cpp:650-712`) needs an equivalent once window identity is a data identity (6.5). |
| **Kit and weapon** (`UCombatKit`, `UWeaponComponent`, `UWeaponData`) | The kit is the root: it holds the weapon (the physical data, `UWeaponData` kept as a sub-asset) and references a combo set, a defense set and a paired set, in place of the attack configuration on the weapon; the character's `WeaponData` slot becomes `Kit`, and the combat settings' default kit stays the fallback for a character without one [D-20261005-48, D-20261005-49, D-20261005-57, D-20261010-02]. |
| **Defense resolver** (`DefenseResolver.cpp`) | The guard arc at 75°; parry candidates (any direction, unparryable, effective window); lands first. The cone and reachability gates go. |
| **Enemy AI and attack tokens** (`EnemyCombatAIComponent`, `CombatTokenSubsystem`) | Attacks chosen among the kit's combo-set slot rows by their selection columns (weight, distance range, target state), through the one evaluator, with `AvailableAttacks` and `FEnemyAttackConfig` retired [D-20261005-63, D-20261007-57]; the tokens count the attackers of whoever is targeted [D-20261007-54]; 2 attackers; tokens held until the attack ends for any reason, the cooldown from the return, the re-grant delay, the enemy's finisher as its attack [D-20261007-30]; others hold with "in progress" meaning montage started, combo continuations holding, unstarted tokens returned, and the resume delay [D-20261007-29]; the second attacker's abort and shove-back [D-20261007-44]; hit-stun and Staggered holding attacks and movement; enemy paired finishers only for marked types, chosen at the attack's start against a player who is Staggered or in hit-stun, with the predictor's confirmation, never a mid-swing promotion [D-20261007-20]. |
| **Cinematic effects** (`CombatEffectsWorldSubsystem`) | The attacker slowdown, as an actor time lease (no lower than 50%, at most 0.3 s), on the existing overlap-safe lease system. |
| **Combat rules and settings** (`UCombatRules`, `UCombatSettings`) | One rules asset for the project, named from a developer-settings entry, holding every global value (4.15); the per-character settings keep the type defaults and the default kit (4.9) [D-20261007-53]. |
| **Animation notifies** | The cancel notify class and its validation (4.13) [D-20261006-02]; the neutral notify state that spans the attack section and drives the data windows (6.5) [D-20261007-39]; the semantic window notifies kept as derived mirrors that the runtime no longer reads [D-20261007-38]. |
| **Editor module** | The combo set editor: the grid with diff markers, the map, the diff row with Save to asset and Extract to new attack [D-20261007-35], the timing link into the timeline, which is the attack editor [D-20261007-38, D-20261007-58], and validation; the defense set and paired set editors as coverage-coloured table rows, and the simulate-press panel with the row trace [D-20261007-57]; export to montage and adopt from montage, the derived-notify colour and name, and the Save / Reset / Leave prompt [D-20261007-38, D-20261007-40]; the migration operation (7.1). |

**Project instructions change too.** `CLAUDE.md`'s rule "Stateful Block input is immediate" no longer holds: a Block press
during your own attack is buffered [D-20261005-13]. Its rule 3, "ParryWindow is on the attacker's montage", becomes "the
parry window is the attacker's, held as data" [D-20261007-38]; the change lands with the pull request that moves the
windows. `CLAUDE.md` and `AGENTS.md` also gain the glossary requirement [D-20261005-40].

### 6.5 Windows at runtime

**One neutral notify state spanning the attack section drives the data windows** [D-20261007-39]. The montage carries one
neutral marker over the section instead of a semantic notify per window. Its tick evaluates every data window of that
attack instance, from the resolved attack spec (4.11): it **opens** a window when the montage position reaches its start,
**refreshes** it each tick for the play rate, and **closes** it at its end. It follows the montage's play rate, pause
and time dilation, and it **dies with the montage instance**, so an interrupted attack leaves no window open. The owner
asked why component tick is avoided; the honest answer was given (the cost argument is weak here; the lifetime argument
is real), component tick was offered with permission, and the owner kept the notify state.

**The registry stays; its identity changes.** The per-attack-instance window table already exists (**[reported]**
`OpenAttackWindowRecords` and the active window records of each kind, keyed by `FAttackWindowInstanceId`,
`CombatTypes.h:2078-2130`). Today it is bound to notifies by a source identity, `FAnimNotifyRuntimeSourceId`, the
montage plus a notify index (**[reported]** `CombatTypes.h:2054-2071`), and by NotifyBegin, Tick and End. Under
D-20261007-39 the identity becomes a **data identity**: the attack's spec, the slot id and the window's index in it; the
scheduler, not a notify, opens, refreshes and closes. Two consequences:
- `FAttackExecutionSnapshot` is built from the resolved slot spec, not from `CurrentAttackData` (4.11) [D-20261007-35];
- the paired marker verification that keys on a notify index (**[reported]** `PairedAnimationComponent.cpp:650-712`)
  needs an equivalent keyed on the data identity, or it breaks when a mirror is regenerated.

**Readable flags are telegraphed automatically** [D-20261007-37]. At the attack-start event (**[reported]**
`FOnAttackStarted`, `CombatTypes.h:3496`) the resolved spec's readable flags, guard breaker, unparryable, unblockable
and block-interruptible, each fire their cue from a **telegraph set keyed by flag**: a trail tint, a cue sound, a flash.
The set lives in the pooled FX asset (`UCombatFXData`) and is resolved through the same path as impact feedback
(`ResolveAndPlayImpactSound`, `ResolveAndSpawnImpactVFX` **[reported]**); the owner's pick calls it "automatic cue from
combat settings", and D-20261007-53 then placed the telegraph set among the combat rules, so the spec reads the pooled
asset as reached through the combat rules' telegraph set (4.15) (a reading, Appendix B). Because the cue is driven by
the spec, a slot override and its cue cannot disagree. **The exact
cues are chosen from rendered captures** [D-20261007-37]; none is decided here. Block-interruptible changes the
attacker's outcome, not the defender's reading, so it may need no cue; that is the design pass's view, and the captures
decide.

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
| `UPairedAnimationData` | `DA_ParryBridge_GateA`, `DA_Counter_GateA`, `DA_Finisher_GateA`, `Finisher_A`, and `Counter_LightAttack_1`–`11` **[reported]**; the stress test counts the eleven per-attack counter assets the same way on `origin/main` **[reported]**. |
| `UWeaponData` | The katana's weapon data (the class is `WeaponData.h`; the shipped asset was not re-read for this pass). It becomes the one kit's weapon (7.2). |

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
- **Timing first.** `HeavyAttack_1`–`4` keep default timing that does not match their animations. "Adopt from montage",
  PR #139's action [D-20261004-33] under its new name (4.13) [D-20261007-38], runs first over all 29 attacks, so that the
  combo set validates and every attack's windows are data (7.3).
- **Old fields.** After migration, the old link fields are read-only and ignored. They are removed once the combo set is
  the only path (stage 9.11).
- **The kit.** `UAttackConfiguration` is replaced by the combo set, and the root becomes the kit [D-20261010-02]: a new
  `DA_Kit_Katana` holds the katana's weapon data (today's `UWeaponData`, kept as the physical sub-asset, 4.3) and
  references `DA_ComboSet_Katana`, `DA_DefenseSet_Katana` and `DA_PairedSet_Katana` [D-20261005-48, D-20261005-57].
  The weapon's `AttackConfiguration` slot (`WeaponData.h:185`) is removed; the character's `WeaponData` slot becomes
  `Kit`; the combat settings' own `AttackConfiguration` field (`CombatSettings.h:112`, kept only for serialization) is
  removed, and `DefaultWeaponData` (`CombatSettings.h:78`) becomes `DefaultKit` [D-20261005-49]. One kit exists after
  this step, the katana's.
- **Enemies.** Enemies choose among their kit's combo set's slot rows by the selection columns, weight and distance
  range [D-20261005-63, D-20261007-57]. Today `BP_EnemyCharacter` lists `LightAttack_1`, and the four placed enemies
  override that list with one attack each (`LightAttack_4`, `_3`, `_1`, `_2`) **[reported]**. The migration gives each
  distinct enemy subset **a second kit**: the katana's weapon data, defense set and paired set shared with the player's
  kit, and its own combo set seeded from today's list as weights [D-20261010-02]; that is how the per-actor AI lists
  retire. How the per-actor differences survive inside one kit is Q15.

### 7.3 Attack fields

| Today | After |
|---|---|
| `HitStunDuration` | No shipped attack saves it **[reported]**, so there is nothing to move. It becomes the hit-stun override. |
| `Attack.Defense.Parryable` (only on `LightAttack_1`) | Removed: every attack is parryable unless flagged [D-20261005-07]. |
| `LightAttack_1`'s 0.10 s window | "Adopt from montage" reads it into the attack as a 0.10 s lead; the montage notify becomes its mirror. The 0.35 s minimum makes it effective [D-20261007-38]. |
| Other attacks' parry windows | None exist **[reported]**; the data field stays unset, and they get the 0.35 s minimum at runtime with an editor warning [D-20261005-74]. |
| `ManualTiming`, `bUseAnimNotifyTiming` | The window fields (4.5). **The "adopt from montage" one-shot** runs once over the 29 attacks: for each, the phase transitions and the hold start (what the timing action reads today), the parry window where one exists, and no contact marker (none is authored; the Active start stays the fallback) are read into the attack's data, anchored to sequence time plus segment; then "export to montage" redraws the section's notifies as derived mirrors and writes the fingerprint. The owner reviews the report. In play nothing changes until the runtime reads the data (stage 9.7); until then the mirrors drive play exactly as the hand-authored notifies did [D-20261007-36, D-20261007-38]. |
| `CounterData` | The paired set `DA_PairedSet_Katana` takes `DA_Counter_GateA`, the only counter the shipped flow reaches, as its front counter row (one row, one content asset [D-20261010-04]); the defense set's front rows seek a paired counter and find it by key, with its damage 25 kept as those rows' value [D-20261005-57, D-20261006-03, D-20261010-01]. `Counter_LightAttack_1`–`11` are never reached and use placeholder editor sounds **[reported]**, so they are migrated only if the owner says so; if they are, each becomes one counter row keyed by the swing of the attack it was authored against. |
| `FinisherData` (`DA_Finisher_GateA`, `Finisher_A`) | The paired set's front finisher row, found by key from the attacker's kit's paired set; the combo set holds no reference [D-20261005-47, D-20261005-57, D-20261010-01]. Which of the two is the front finisher is for the owner to pick during migration. |
| `ImpactAudioConfig`, `ImpactVFXConfig`, `HitstopConfig` | The impact feedback for the hit outcome. |
| `DefenseProfile` blocked-impact overrides | The impact feedback for the block outcome. |
| `SwingShape`, `NominalLane` | Kept. The new `Swing` field is filled for all 29 attacks by one run of "Auto-detect swing", and the designer reviews the result [D-20261005-84]. |
| The non-attack montages the migration reaches (the deflect, the counter halves, the hit and Staggered reactions, guard enter and exit) | Each gets a cancel notify where the owner places it; until then it is read as ending at montage end, with a warning [D-20261006-02]. |

### 7.4 Defense configuration and hit reactions into the defense set and reaction profiles

- **`DA_DefenseConfiguration_GateA`** splits into `DA_DefenseSet_Katana` (block reactions, the parry deflect, guard enter
  and exit), `DA_PairedSet_Katana` (the counter and finisher sequences) and `DA_ReactionProfile_Base` (the Staggered
  reaction and recoil; the global guard-facing values go to the combat rules, below), plus the player's and the enemy's
  profiles
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
| `MaximumAutomaticTurn`, `DefenseTurnRate` | Guard-facing and block turn; parry reach; the turn budget for each paired step | Combat rules defaults, overridable on the defense set: guard facing keeps today's values; the block turn rate starts at 360°/s. They no longer limit a parry. | D-20261005-05, D-20261004-21, D-20261005-68, D-20261005-69, D-20261007-53 |
| `NormalBlockFinalTolerance` | The 35° block gate | The combat rules' guard arc, 75° | D-20261005-11, D-20261007-53 |
| `CenterLaneHalfAngle`, `BoneHeightRows` | Lane and height classification for matching rows | Kept as global classification in the combat rules if direction or swing keys need them, otherwise removed. Proposed. | — |
| `ThreatLockMinSeconds`, `ThreatSwitchLeadSeconds`, `GuardedThreatRefreshSeconds`, `DefenseThreatRange`, `GuardManualOverrideThreshold`, `GuardAutoFacingResumeSeconds` | Guard facing | Combat rules, values unchanged | Unchanged (3.11); the home is D-20261007-53. |
| `MaximumHighConfidencePredictionAge` | How fresh a prediction must be for a parry | Removed, with the prediction gate | D-20261005-64 |
| `InteractionTombstoneSeconds`, `TerminalInteractionCacheCap`, `NormalBlockTranslationDriftTolerance` | Nothing at runtime | Removed | Unread **[reported]**. |
| `GuardEnterMontage`, `GuardExitMontage` | Nothing; never played | Defense set, and wired | D-20260929-07 ("wire it"), D-20261005-55 |
| `NoMontageParryBridgeSeconds`, `CounterWindowSeconds`, `FinisherReadySeconds` | Timers between the parry and later presses | Removed. There is no separate press window; the Staggered time replaces them. | D-20261005-24, D-20261005-22 |
| `ParryStaggerDuration` (the **attacker's** configuration) | Staggered length after a parry | The Staggered default in the combat rules, plus attack overrides | D-20261005-08, D-20261007-53 |
| `TimeDilationLeaseWatchdogSeconds` | Emergency restore for slow motion | Combat rules | Unchanged; the home is D-20261007-53. |
| `NormalBlockTranslationAllowance`, `PerfectParryTranslationAllowancePerRole` | Warp translation caps | Block: removed, because the code-driven turn does not translate. Parry: the paired content's own warp limits (4.12). | D-20261004-21 |
| `DefaultBlockImpactAudio` and `VFX`, `DefaultParryImpactAudio` and `VFX` | Defense sound and effect defaults | Combat settings type defaults for the block and parry outcomes | D-20261004-20 |
| `DefenderPresentationRows` | Block and parry presentation, keyed by outcome, height, lane, swing and tags | Defense set: block reactions by direction, and the deflect of each counter entry | D-20261005-48, D-20261004-23 |
| `AttackerResponseRows` (`ParryStagger`, `Recoil`) | The attacker's reaction to being parried, and its recoil | Reaction profile: the Staggered reaction by direction, and recoil | D-20261005-55 |
| Row payload: `PairedBridgeData`, `ReviewedDeflectionMarker`, `bRequiresBridgePreflight` | Today's paired parry selection, and the dead re-pick | Removed. The defense set's counter row carries the deflect and fallback strike and says whether a paired counter is sought; the paired set's counter rows are found by key. | D-20261004-22, D-20261005-48, D-20261005-57, D-20261010-01 |
| Row payload: `SourceSocketOverride`, `TargetBoneOverride` | Nothing | Removed | Unread **[reported]**. |
| Row payload: `Hitstop`, the audio and VFX overrides | Block and parry feedback | The attack's impact feedback per outcome | D-20261004-20 |
| Row payload: blend times, `bEnableRotationWarp`, `MaximumTranslation` | Montage blending and warping | Fields on the defense-set and reaction-profile entries where still meaningful. The rotation warp is replaced by the code-driven turn. | D-20261004-21 |

**The combat settings split** [D-20261007-53]. `DA_CombatSettings_Default` is read once and written twice: its global
values (the list of 4.15) go to the new `DA_CombatRules`, the one rules asset named from the developer-settings entry;
its per-type defaults and `DefaultWeaponData`, renamed `DefaultKit`, stay in `DA_CombatSettings_Default`, now a
per-character asset. `DA_CombatSettings_DefenseMatrix` keeps its per-type defaults and default kit as the test level's
settings and reads the same rules asset; where its global values differ from the default asset's today, the migration
reports the difference to the owner rather than choosing (the test level's fixtures set their own values in any case,
8.1). The question of whose settings a rule reads ends with the split (4.15).

### 7.5 Renaming the GateA and GateB assets

The decisions: [D-20261004-19, D-20261004-17]. Assets the redesign retires are removed rather than renamed. **The names
below are proposed** (Q16). They describe each asset, and they avoid the retired words [D-20261005-40].

| Today | Proposed |
|---|---|
| Folder `Data/PDA/Defense/GateA/` | `Data/PDA/Defense/Paired/` |
| Folder `Animation/Montages/Defense/GateA/` | `Animation/Montages/Defense/ParryAndBlock/` |
| Folder `Data/PDA/Defense/GateB/` (and `GateB/Attacks/`) | `Data/PDA/Defense/DefenseMatrix/` (and `DefenseMatrix/Attacks/`) |
| Folder `Animation/Montages/Defense/GateB/` | `Animation/Montages/Defense/DefenseMatrix/` |
| — | `DA_Kit_Katana`, new: the katana's kit, holding the weapon data and the three sets (4.3) [D-20261010-02] |
| — | `DA_CombatRules`, new: the one combat rules asset (4.15) [D-20261007-53] |
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
  the parry-counter stage (9.8) retires it and keeps its failure-path tests.
- **`FFinisherTriggerConfig`.** It has no owner today. D-20260929-07 said to wire it, but D-20261005-18 removes its
  thresholds, so it is retired rather than wired (Appendix B).
- **The hard-coded 25%** (`HitReactionComponent.cpp:1423`), and the never-granted `Context.LowHealthFinisher` tag.
- **`UAttackConfiguration`**, replaced by the combo set inside the kit [D-20261005-48, D-20261010-02].
- **The combo set's finisher references and the defense entry's counter reference** this spec had proposed are never
  built: the lookup is by key [D-20261010-01].
- **Any is-player branch in the rules code** is replaced by a profile field; the migration lists each one it finds for
  the owner rather than guessing which rule it served [D-20261007-54].
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
- **A decided-rule check on shipped content is not a pinned tunable.** A check that the player's profile has guard-break
  counting off asserts a decided rule, "the player's guard breaks only to guard breakers" [D-20261005-14,
  D-20261007-54], not a tuning value; such a check names its decision in its name or its message, and there is one of
  them [D-20261007-54].
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
  - the interrupt rule;
  - the table evaluator and its trace, on tables built in code [D-20261007-57].
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
| Guard on an interrupting hit | `…Guard.OutOfArcInterruptEndsGuard`; `…Guard.UnblockableInterruptEndsGuard`; `…Guard.GuardReturnsAtOnceWhileBlockHeld`; `…Guard.ReturnedGuardIsNotAParryPress`; `…Guard.FlinchNeverEndsGuard`. | — |
| No chip damage | `…Guard.BlockDealsNoDamageOnEveryAttackType`. | — |
| Block turn | `…Guard.BlockTurnFacesAttackerFully`; `…Guard.BlockTurnRateFromDefenseSetElseSettings`: the fixture sets the rate. | The turn reads as natural at the tuned rate. |
| Staggered player | `…Staggered.PlayerCannotAttackGuardOrSteer`; `…Staggered.PlayerCarriedByFullPush`; `…Staggered.PlayerUsesOwnProfileTime` (the fixture sets the time); `…Staggered.PlayerDodgeEscapeOpensAtMaxOfFloorAndHalf`; `…Staggered.PlayerDodgeOutEndsTheStateForBothSides`; `…Staggered.PlayerInputBufferedFiresAfter`. | The stumble, the push and the dodge out read as one beat. |
| Parry window | `…Parry.EffectiveWindowNeverBelowMinimum`; `…Parry.FloorAppliesAfterScale`; `…Parry.WindowNeverOpensBeforeAttackStarts`; `…Parry.NoAuthoredWindowGetsMinimum`; `…Parry.ArrivalPredictedFromTrajectoryMovesWithSlowdown`; `…Parry.WindowReadFromResolvedSpecNotNotify`: the fixture moves the mirror notify and the window does not move. | — |
| Windows as data | `…Windows.AnchorSurvivesSegmentRetime`; `…Windows.SchedulerOpensRefreshesClosesWithPlayRate`; `…Windows.SchedulerDiesWithMontageInstance`; `…Windows.SlotDiffOverridesAttackWindow`; `…Windows.SnapshotBuiltFromResolvedSlotSpec`; `…Editor.ExportToMontageDrawsDerivedNotifies`; `…Editor.AdoptFromMontageReadsHandAuthoredNotifiesOnce`; `…Editor.HandMovedMirrorChangesNothingInPlay`; `…Editor.SaveResetLeavePromptOnFingerprintMismatch`; `…Content.StaleMirrorWarnsOnEveryAttackUsingTheMontage`. | — |
| Telegraph | `…Telegraph.FiresFromResolvedSpecAtAttackStart`; `…Telegraph.SlotOverrideAndCueAgree`. | The cues read before contact; chosen on the sweep. |
| Contact predictor | `…Predictor.ArrivalFollowsWarpAndTargetMotion`; `…Predictor.ConfidenceAgainstPerUserThreshold`; `…Predictor.ToleranceAndHorizonFromSettingsElseAttack`; `…Predictor.NeverDecidesParrySuccess`. | — |
| Parry lockout | `…Parry.MissedPressLocksOutParries`; `…Parry.PressWithNothingIncomingIsNotAMiss`; `…Parry.SuccessStartsNoLockout`; `…Parry.BlockingWorksDuringLockout`; `…Parry.LockedOutPressCountsWhenWindowStillOpenAtLockoutEnd`; `…Parry.LockedOutPressForgottenWhenWindowClosed`; `…Parry.ZeroLockoutNeverLocksOut` (the fixture sets the lockout). | — |
| Unparryable | `…Parry.UnparryableAttackIsNotACandidate`; `…Parry.AttacksAreParryableByDefault`. | — |
| Lands first | `…Parry.AnswersEarliestContact`; `…Parry.EqualContactFallsBackToAngle`. | — |
| Adaptive turn and slowdown | `…Parry.TurnFinishesExactlyAtPredictedArrival`; `…Parry.SlowdownEasesInAndOut`; `…Parry.SlowdownNeverBelowFloorSpeedOrBeyondMaxTime`; `…Parry.SucceedsWhenTurnCannotFinish`. | Side and rear parries pressed late, midway and early, measured for foot slide, pelvis jump and yaw rate, as in the earlier prototype. |
| Parry-counter outcome | `…Counter.LethalCounterKillsAtItsContact`; `…Counter.AuthoredKillSequencePlaysInsteadWhenLethal`; `…Counter.NoKillSequenceNeeded`; `…Counter.NonLethalCounterLeavesStaggered`; `…Counter.OutcomeFixedAtPress`, in which the health changes between the press and contact and the clip does not; `…Counter.DamageAppliedAtContactNeverClamped`. | Front, side and rear parry-counters, lethal and non-lethal. |
| Counter damage | `…Counter.ContentThenDamageThenOutcome`; `…Counter.UsesEntryDamageElseSettingsDefault`; `…Counter.ClipOverrideBeatsEntryDamage`; `…Counter.FallbackStrikeAlwaysDealsEntryDamage`; `…Counter.StaggeredStartsAtEnemyCancelNotify`. | — |
| Cancel notify | `…CancelNotify.PlayerFreeFromTheirOwn`; `…CancelNotify.EnemyStaggeredFromItsOwn`; `…CancelNotify.BothUntouchableUntilTheirOwn`; `…CancelNotify.HitReactionActsOnceHitStunAlsoOver`; `…CancelNotify.FallbackStrikeCarriesItsOwn`; `…CancelNotify.AttacksCancelAtPhaseTransition`; `…CancelNotify.MissingMeansMontageEndAndWarns`. | The hand-off from the counter to free play reads as one motion. |
| Swing detection | `…Editor.AutoDetectSwingClassifiesLateralOverheadAndThrustPaths`, on synthetic socket paths; `…Editor.AutoDetectSwingAsksBeforeOverwritingAHandValue`. | — |
| Counter fallback | `…Counter.FallbackOrderDirectionSwingThenDirectionThenAnyThenStrike`; `…Counter.LethalFallbackStrikeKillsAtContact`; `…Counter.FinisherPoolNotConsultedForParryCounter`. | — |
| Follow-up strike | `…FollowUp.AppliesMultiplierToResolvedAttackDamage`; `…FollowUp.ReachIs350cm`; `…FollowUp.NonLethalEndsStaggeredImmediately`; `…FollowUp.OnlyOnePerParryCounter`; `…FollowUp.PairedClipWhenDirectionMatches`; `…FollowUp.LethalPlaysPairedFinisher`; `…FollowUp.PressElsewhereLeavesStaggeredRunning`; `…FollowUp.LightPressUsesLightOpenerFromInitialPress`; `…FollowUp.HeavyPressUsesHeavyOpenerChargeIgnored`; `…FollowUp.HoldDoesNotOpenOnStaggeredTarget`; `…FollowUp.DirectionalLightNeverFollowsUp`; `…FollowUp.PressInsideSequenceFiresAtPlayerCancelNotify`; `…FollowUp.LastInputWinsInsideSequence`; `…FollowUp.BufferedPressHonoursHoldAtHoldWindowStart`. | The follow-up strike reads clearly. |
| Scripted hits | `…Counter.FallbackStrikeLandsAtContactMarkerWithoutTrace`; `…Counter.ScriptedCounterAppliesNoHitStunOrPush`; `…FollowUp.ScriptedStrikeAppliesItsAttacksHitStunAndPush`. | — |
| Staggered | `…Staggered.OneStateOneDuration`; `…Staggered.CausingAttackOverrideWins`; `…Staggered.PlayerTimeIgnoresAttackAndSlotOverrides`; `…Staggered.EnemyHeldWhileStaggered`; `…Staggered.RunsBesideHitStunFreeWhenBothEnd`; `…Staggered.NoGuardWhileStaggeredEvenInHitStun`; `…Staggered.DodgeOutNeedsBothMinimums`; `…Staggered.HitNeverShortensALock`. | — |
| Guard break | `…GuardBreak.GuardBreakerBreaksAtOnce`; `…GuardBreak.SlidingWindowSumsLastWSeconds` (the fixture sets N and W, on a fixture enemy that guards); `…GuardBreak.CountClearsOnBreak`; `…GuardBreak.CountingToggleOffNeverCounts`; `…GuardBreak.CountingOffProfileBreaksOnlyToGuardBreakers`; `…GuardBreak.BreakingHitDealsNoDamage`; `…GuardBreak.UnblockableWinsOverGuardBreaker`; `…Content.UnblockableGuardBreakerPairWarns`. | The guard-break reaction reads clearly. |
| Interruption | `…Interrupt.OnlyWhenPressureExceedsArmor`, using strict inequality; `…Interrupt.ArmorIsMaxOfBaseAndWindow`; `…Interrupt.FlinchKeepsCurrentAction`; `…Interrupt.ArmorIsZeroWhileStaggered`; `…Interrupt.BaseArmorAppliesInHitStun`; `…Interrupt.SuperArmorNeverInterruptsButFlinches`. | A flinch and an interrupt are told apart at a glance. |
| Contact order | `…Contact.FrameJudgedAgainstFrameStartState`, a trade in which both hits land; `…Contact.DamageAppliesBeforeReactions`; `…Contact.KillingHitReplacesReactionNoStunNoPush`; `…Contact.HighestPressureThenLatestSuppliesReaction`. | A trade reads as two hits. |
| Hit-stun | `…HitStun.NeverBelowFloor`; `…HitStun.ActionsWaitGuardAllowed`; `…HitStun.InputBufferedFiresAfter`; `…HitStun.EnemyAIHoldsThenResumes`; `…HitStun.DodgeEscapeOpensAtMaxOfFloorAndHalf`; `…HitStun.EscapeDodgeIsCommitted`. | Hit-stun lengths read well. |
| Knockback by outcome | `…Knockback.FlinchDoesNotPush`; `…Knockback.BlockedHitUsesBlockShoveOwnDefault`; `…Knockback.GuardBreakUsesOwnDefault`; `…Knockback.ShoveAndBreakPushNotDerivedFromHitPush` (the fixture sets the hit push and checks the others do not move); `…Knockback.StaggeredOrInHitStunVictimsPushNormally`; `…Knockback.NoneDuringPairedSequence`; `…Knockback.OnlyDodgeCancelsPushInFlight`; `…Knockback.ActionsRideOnPushInFlight`; `…Content.HitStunShorterThanPushWarns`. | The block shove and the break push read as different weights; the shove's direction and the carried-over behaviours are judged on the knockback scenarios [D-20261007-28]. |
| Paired finishers | `…Finisher.OnlyWhenLethalAndPredictedToLand`; `…Finisher.AnyLethalAimedStrikeQualifies`; `…Finisher.NoHealthPercentRoute`; `…Finisher.StaggeredAloneDoesNotFinish`; `…Finisher.KeyedByDirectionOnly`; `…Finisher.ListPerDirectionUsesVarietyRuleNoThrottle`; `…Finisher.UncoveredDirectionFitsToNearestAuthored`; `…Finisher.FitBeyondBudgetsFailsToOrdinaryAttack`; `…Finisher.OrdinaryAttackAfterFailedFitKillsOnlyIfItLands`; `…Finisher.GroundStateHeightAndInvulnerabilityCountAsFitFailed`; `…Finisher.AimConeAndRange`; `…Finisher.EnemyOnlyWhenItsKitsPairedSetHoldsPlayerFinisherContent`; `…Finisher.EnemyFinisherChosenAtAttackStartOnly`; `…Finisher.EnemyNeverPromotesSwingInProgress`; `…Finisher.EnemyFinisherNeedsStaggeredOrHitStunAndPrediction`; `…Finisher.GuardBreakAloneGrantsNoEnemyFinisher`. | Alignment from up to 350 cm; the seam blend of a fitted clip, measured as the other paired checks are. |
| Several attackers | `…Attackers.AtMostConfiguredCountPerTarget`; `…Attackers.TokenHeldUntilAttackEndsForAnyReason`; `…Attackers.CooldownRunsFromTokenReturn`; `…Attackers.FreedTokenWaitsReGrantDelay`; `…Attackers.EnemyFinisherHoldsAToken`; `…Attackers.OthersHoldDuringEveryPairedSequence`; `…Attackers.MontageStartedMeansInProgress`; `…Attackers.ComboContinuationHolds`; `…Attackers.UnstartedTokenReturned`; `…Attackers.SecondAttackerAbortsAndIsShovedBack`; `…Attackers.HoldingEnemiesResumeAfterDelay`. | A one-against-four crowd fight; the shove-back's spacing. |
| Targeting | `…Targeting.FreeModeConeAroundStickDirection`; `…Targeting.FreeModeConeAroundFacingWhenNoDirectionHeld`; `…Targeting.StaggeredOrEligibleWinsTiesElseNearest`; `…Targeting.CameraNeverDecides`; `…Targeting.OneRuleForOrdinaryFollowUpAndFinisher`; `…Targeting.LockPicksBestByConeAndScores`; `…Targeting.LockedPressesAimAtLockedTarget`; `…Targeting.FlickSwitchesToNearestInThatDirection`; `…Targeting.LockDropsOnDeathAndRePicksWhenNear`; `…Targeting.SecondPressDropsLock`; `…Targeting.LockFollowsActionToTheEnemyActedOn`; `…Targeting.DirectionalLightWhileLockedAimsByStickAndTransfers`; `…Targeting.DirectionalLightWithNoEnemyBreaksLockAndMisses`; `…Targeting.LockEndsOutOfRangeOrSightAfterGrace`; `…Targeting.LockEndsWhenPlayerStaggeredOrFinished`. | Attacks go where the stick points; the lock, strafe and switch read as one system. |
| Dodge | `…Dodge.StickDirectionCameraRelative`; `…Dodge.NeutralStickAwayFromThreatElseStraightBack`; `…Dodge.IFrameWindowIgnoresContacts`; `…Dodge.CancelsRecoveryNeverWindupOrActive`; `…Dodge.DropsGuard`; `…Dodge.EscapesHitStunAndStaggeredAfterMinimums`; `…Dodge.CancelsPushInFlight`; `…Dodge.AttackPressBeforeCancelPointIsBuffered`; `…Dodge.AttackAfterCancelPointStartsOpener`; `…Dodge.NoCooldownConsecutiveDodgesAllowed`; `…Dodge.PressDuringDodgeBuffersToCancelPoint`. | Distance, duration and the i-frame window judged on captures (Q22). |
| Precedence | `…Data.SlotOverrideWins`; `…Data.AttackOverrideBeatsSetDefault`; `…Data.SetDefaultBeatsSettings`; `…Data.ResolvedSpecUnchangedByMidAttackEdit`. | — |
| Kit | `…Data.KitSuppliesWeaponAndThreeSets`; `…Data.NoKitUsesSettingsDefaultKit`; `…Data.TwoKitsShareWeaponDefenseAndPairedSets`; `…Data.KitMissingAPartFailsValidation`. | — |
| Combat rules | `…Rules.OneRulesAssetReadByEveryCharacter`; `…Rules.PerCharacterTypeDefaultDoesNotChangeAGlobalRule`; `…Rules.SettingsHoldOnlyTypeDefaultsAndDefaultKit`. | — |
| Player by data | `…Player.ProfileFieldsDecideCountingStaggeredTimeAndDodgeEscape`, on two fixture characters, neither "the player", one with the player's fields; `…Player.TokensCountAttackersOfTheTarget`; `…Content.PlayerProfileHasCountingOff`, the decided-rule check of 8.1. | — |
| Selection tables | `…Tables.OneEvaluatorServesOpenerSlotCounterPairedAndFinisherRows`; `…Tables.TraceListsEveryRowWithPassFailPerColumn`; `…Tables.LanePreferredWhenItMatches`; `…Tables.VarietyRuleExcludesLastTwo`; `…Tables.BuiltInCodeWithoutAssets`. | — |
| Paired lookup by key | `…Paired.FinisherRowsFoundByDirectionFromAttackersKit`; `…Paired.CounterRowsFoundByDirectionAndSwingFromDefendersKit`; `…Paired.RowOptedOutPlaysFallbackStrike`; `…Paired.NoCrossReferenceExists`; `…Paired.CanFinishThePlayerIsContentPresence`; `…Content.CoverageGapsNamedNoReferencesChecked`. | — |
| One row, one content asset | `…Paired.AlternateChosenInsideOurDirectionNeverChangesIt`; `…Paired.GroundStateAndHeightReadThroughSeamNotRow`. | — |
| Timeline and simulate-press | `…Editor.TimelineLanesDrawDataWindowsOverMontage`; `…Editor.AuthoritativeMarkFollowsSlotDiff`; `…Editor.TimelineDragWritesSlotOrAttackValue`; `…Editor.SimulatePressShowsSameTraceAsRuntime`. | — |
| Selection function (the evaluator on the combo set) | `…ComboSet.ReturnsChosenAndRejectionReasons`; `…ComboSet.BackToOpenerIsTheOnlyLegalCycle`. | — |
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

**Where the 7 and 10 October decisions land** (this spec's call, stated so that it can be overruled): the table
evaluator and its trace, judging the combo set's openers and entry rules, in 9.1 [D-20261007-57]; the timeline as the
attack editor and the simulate-press panel in 9.2 [D-20261007-57, D-20261007-58]; the kit rename, the combat rules
split, the defense and paired sets as tables found by key, the player's profile fields and the marker as content
presence in 9.3, with the data they shape [D-20261007-53 to -55, D-20261010-01 to -04]; the paired lookup by key in
play, for counters and finishers, in 9.8 [D-20261010-01].

### 9.1 Combo sets with today's behaviour

- **What:**
  - `UComboSet` with slots, links, openers and the back-to-opener link;
  - the table evaluator with its trace, judging the openers and entry rules as rows and the links as topology
    [D-20261007-57];
  - the runtime reading the set;
  - set validation;
  - migrating `DA_Configuration_Katana` and the 20 combat attacks into `DA_ComboSet_Katana`;
  - the old link fields kept read-only.
- **Ships:** the same combos, now with loops that validate, and one trace that says why a press chose what it chose.
- **Decisions:** D-20261005-01 (structure), D-20261005-03, D-20261007-57 (the evaluator).
- **Needs:** Q9, Q10 to Q14. PR #139 has merged.

### 9.2 The moveset grid

- **What:**
  - the grid and the node map;
  - the timeline as the attack editor, opened from the row's timing link, with its lanes, the authoritative mark and the
    mirror lane [D-20261007-58];
  - the simulate-press panel showing the row trace [D-20261007-57];
  - the badges and "who uses this set";
  - pickers and edit-through;
  - the slot row as a diff, with its four groups and counts, "show inherited", Save to asset per field, Extract to new
    attack on the row, and the diff markers on the grid [D-20261007-35];
  - inline overrides for the fields that are already layered (knockback, damage), with the combo-set layer hidden
    until a second combo set differs from the settings [D-20261007-34].
- **Ships:** the owner's chosen editor, with the timeline and the simulate-press panel.
- **Decisions:** D-20261005-02, D-20261007-34, D-20261007-35, D-20261007-57, D-20261007-58.
- **Needs:** approval of section 5.

### 9.3 Layered attack data, the kit, the combat rules and reaction profiles

- **What:**
  - the impact block on attacks, as data, with set-level defaults and slot overrides;
  - the resolved attack spec carried on hits;
  - `UDefenseSet` and `UPairedSet` as selection tables (4.16), filled with today's block, deflect, counter and finisher
    content, with the paired rows found by key and no cross-references [D-20261007-57, D-20261010-01, D-20261010-04];
  - **the kit** (`UCombatKit`, `DA_Kit_Katana`), holding the weapon data as its physical sub-asset and the three sets,
    with the default kit as the fallback; a second kit per distinct enemy subset for the AI lists (7.2)
    [D-20261010-02, D-20261005-63];
  - **the combat rules split** from the per-character settings (`UCombatRules`, `DA_CombatRules`, the
    developer-settings root) (4.15, 7.4) [D-20261007-53];
  - `UReactionProfile` with a base profile and inheritance, its player fields with no is-player branch and the
    shipped-content check on the player's profile [D-20261007-54], and the marker as content presence on the paired set
    [D-20261007-55];
  - retiring `UDefenseConfiguration`;
  - the owner's held content;
  - the asset renames;
  - the charge-loop snap fix;
  - **the timing windows as attack data** (4.5, 4.13), anchored to sequence time plus segment, with the "adopt from
    montage" one-shot over the 29 attacks and "export to montage" redrawing the mirrors (7.3); the windows edited in the
    timeline of 9.2 (5.11), with the Save / Reset / Leave prompt where a mirror exists. **In play nothing changes at this
    stage:** the mirrors drive play exactly as the
    hand-authored notifies did, until the runtime reads the data in 9.7 [D-20261007-36, D-20261007-38, D-20261007-40];
  - every attack field slot-overridable, in the data [D-20261007-35].
- **Ships:** the same rules, with one place per question and the owner's tuning live.
- **Decisions:** D-20261004-20, D-20261004-22 (placement per D-20261005-48), D-20261004-32, D-20261004-38,
  D-20261004-41 (data), D-20261004-19, D-20261002-07, D-20261005-45 (keys), D-20261005-46, D-20261005-47 (no body
  types), D-20261005-48, D-20261005-49, D-20261005-55, D-20261005-56, D-20261005-57, D-20261007-31 (finisher lists),
  D-20261007-35, D-20261007-36, D-20261007-38, D-20261007-40, D-20261007-42 (per-clip requirements as data),
  D-20261007-53 to D-20261007-57, D-20261007-59, D-20261010-01 to D-20261010-04.
- **Needs:** Q1, Q16, Q15, Q23, Q24.

### 9.4 The dodge

- **What:** the dodge (3.14): the Evade press; the direction from the stick, camera-relative, or away from the threat;
  the committed move with its i-frame window and cancel point as data on the dodge's own asset (4.14); its cancels
  (Recovery, guard, a push in flight) and what it never cancels (Windup, Active); no resource and no cooldown, with a
  press during a dodge buffered to its cancel point. The escapes from hit-stun and Staggered join it in 9.5.
- **Ships:** a dodge the owner can feel before hit-stun lands on top of it; the first captures for its numbers.
- **Decisions:** D-20261007-21, D-20261007-32 (the push cancel), D-20261007-49 to D-20261007-52.
- **Needs:** Q22 for its numbers; nothing else open. It comes before hit-stun because D-20261007-21 says so: "a small
  stage before hit-stun ships".

### 9.5 Hit rules

- **What:**
  - pressure against armor, with armor windows and the flinch; armor 0 while Staggered, the base in hit-stun, super armor
    flinching [D-20261007-26];
  - the contact order within a frame: frame-start state, damage first, a killing hit replacing the reaction, the
    strongest surviving hit supplying the reaction [D-20261007-19];
  - the guard ending on an interrupting hit through or around it, and returning while Block is held [D-20261007-25];
  - hit-stun with its floor, holding attacks and movement while input buffers; the dodge escape from 9.4; a buffered
    press honouring the hold when it fires [D-20261007-45];
  - one Staggered state, still caused only by a parry-counter at this stage, running beside hit-stun with both ending
    before the character is free [D-20261006-01];
  - the cancel notify on hit reactions, read as montage end with a warning where missing [D-20261006-02];
  - knockback by outcome: no push on a flinch or a killing hit, the block shove and the guard-break push on their own
    per-type defaults [D-20261007-27], none in paired sequences; a push in flight cancelled only by a dodge, with the
    editor's hit-stun-shorter-than-push warning [D-20261007-32];
  - the damage ÷ health rule removed.
- **Decisions:** D-20261004-42, D-20261005-17, D-20261005-34, D-20261005-37, D-20261005-53, D-20261005-58,
  D-20261005-59, D-20261005-60, D-20261005-87, D-20261006-01, D-20261006-02, D-20261007-19, D-20261007-25,
  D-20261007-26, D-20261007-27, D-20261007-32, D-20261007-45.
- **Needs:** Q2, Q3, Q8; Q21 after the knockback captures (the shove's direction and duration, and the carried-over push
  behaviours) [D-20261007-28].

### 9.6 Guard rules

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

### 9.7 Parry rules

- **What:**
  - the effective window, with its minimum and scale;
  - the lockout in its lenient form, with the press remembered through it, and the unparryable flag;
  - lands first;
  - the adaptive turn and the attacker slowdown;
  - parry feedback and parry assists;
  - **the window-ownership runtime change** (6.5), placed here because the parry window is the first window whose home
    the rules depend on: the neutral notify state spanning the attack section drives every data window of the attack
    instance; window identity becomes a data identity in the existing registry; `FAttackExecutionSnapshot` is built
    from the resolved slot spec; the paired marker verification gets its data-identity equivalent; and the semantic
    window notifies become mirrors the runtime no longer reads [D-20261007-38, D-20261007-39];
  - the telegraph for readable flags at attack start, with its cues chosen on captures [D-20261007-37];
  - the contact predictor (3.15), serving the parry window first [D-20261007-24].
- **Meanwhile:** after a successful parry, today's post-parry sequence still plays.
- **Decisions:** D-20261005-05 to D-20261005-07, D-20261005-09, D-20261005-10, D-20261005-32, D-20261005-33,
  D-20261005-36, D-20261005-50, D-20261005-64, D-20261005-71 to D-20261005-79, D-20261004-06, D-20261004-07,
  D-20261007-18, D-20261007-24, D-20261007-37, D-20261007-38, D-20261007-39.
- **Also needs:** the owner's veto or approval of the lead-time form (R-20261005-01); the home is decided.
- **Needs:** Q8; Q20 for the predictor's thresholds and scale.

### 9.8 The parry-counter, and paired finishers by lethal damage

- **What:**
  - the counter rows with their damage, the paired set's counter rows found by key, their fallback, and the optional kill
    sequence [D-20261010-01];
  - the decision at the press: content, damage, outcome; damage at the counter's contact, never clamped; the kill at
    contact, or the authored kill sequence;
  - the cancel notify on every clip the sequence plays, and the protection until each character's own;
  - the follow-up strike, paired when a clip matches the direction; the light opener or the heavy opener from the
    initial press, with no hold and the charge ignored [D-20261007-17, D-20261007-33]; presses buffered inside the
    sequence, last input wins;
  - scripted hits at the contact marker for the single-character counter and the follow-up strike [D-20261007-41];
  - no latch and no end-position rule; a press aimed elsewhere leaves Staggered running;
  - free-mode aim for every attack press: the cone around the stick direction, or the facing when no direction is held,
    replacing today's soft aim (3.13);
  - paired finishers on any lethal aimed strike that the predictor says lands, keyed by direction, at 350 cm, from a
    list per direction with the variety rule, found by key from the attacker's kit's paired set [D-20261007-22,
    D-20261007-31, D-20261010-01];
  - fit to clip for an uncovered direction, with its budgets on the paired set and the seam blend, verified on a
    rendered check; the ordinary attack only when fitting fails [D-20261007-22, D-20261007-23];
  - the per-clip ground state, height and invulnerability checks [D-20261007-42];
  - enemy paired finishers only from a kit whose paired set holds player-finisher content [D-20261007-55], chosen at the
    attack's start against a player who is Staggered or in hit-stun, with the predictor's confirmation, never a mid-swing
    promotion [D-20261007-20];
  - the lock transferring to the enemy acted on, when lock mode exists (9.9) [D-20261007-46];
  - today's post-parry stages, including #137's interim window, and the 25% threshold removed.
- **Decisions:** D-20261005-18 to D-20261005-24, D-20261005-27, D-20261005-28, D-20261005-30, D-20261005-38,
  D-20261005-39, D-20261005-42, D-20261005-43, D-20261005-45, D-20261005-47, D-20261005-51, D-20261005-54,
  D-20261005-57, D-20261005-80 to D-20261005-82, D-20261005-84 to D-20261005-86, D-20261006-02 to D-20261006-05,
  D-20261007-07 to D-20261007-12, D-20261007-14, D-20261007-17, D-20261007-20, D-20261007-22, D-20261007-23,
  D-20261007-31, D-20261007-33, D-20261007-41, D-20261007-42, D-20261007-43, D-20261007-55, D-20261010-01.
- **Needs:** Q4, Q5, Q24; the fitting budgets' values from the captures; counter content (3.3).
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

### 9.9 Lock mode

- **What:** a new lock input action; the lock press and the best target by the cone and its scores; every press aimed
  at the locked target; the strafe facing it; the flick (or scroll) switch to the nearest enemy in that direction; the
  lock following action, with the strafe re-centring [D-20261007-46]; the directional light aimed by the stick while
  locked, transferring or breaking the lock [D-20261007-47]; the events that end a lock, with the out-of-range and
  out-of-sight grace [D-20261007-48]; the drop on the target's death with the automatic re-pick when one is near; the
  second press (3.13).
- **Ships:** the optional lock the pillars ask for, one that never holds the player to a target they have left.
- **Decisions:** D-20261007-13, D-20261007-14, D-20261007-46, D-20261007-47, D-20261007-48.
- **Needs:** Q19 (the range, "near" and the grace); the camera's framing while locked, which belongs to the camera
  design and is not in this spec.

### 9.10 Several attackers

- **What:** two attack tokens, each held until its attack ends for any reason, the cooldown from the return, the
  re-grant delay, and the enemy's finisher as its attack [D-20261007-30]; others hold during every paired sequence
  involving the player, with "in progress" meaning montage started, combo continuations holding, unstarted tokens
  returned, and the per-enemy resume delay [D-20261007-29]; the second attacker's abort and shove-back [D-20261007-44].
- **Decisions:** D-20261005-21, D-20261005-35, D-20261005-36, D-20261007-08, D-20261007-29, D-20261007-30,
  D-20261007-44.
- **Needs:** Q6 (the cooldown's length), Q7 (the unpaired follow-up); the shove-back distance from captures.

### 9.11 Removing the deprecated fields

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
target and lock mode's numbers. The hits review and the authoring pass of 7 October (D-20261007-19 to -52) answered the
Heavy press (D-20261007-33) and narrowed two: the cooldown's start is decided (Q6 asks its length only, D-20261007-30)
and others hold during a paired follow-up (Q7 asks about the unpaired one only, D-20261007-29). They left three things
open by the owner's own words, asked here: the contact predictor's thresholds and scale (Q20, D-20261007-24), the block
shove's direction and the carried-over push behaviours, to be decided on captures (Q21, D-20261007-28), and the dodge's
numbers (Q22, D-20261007-21, D-20261007-50). Lock mode's numbers stay open (Q19), with the lock grace added to them
(D-20261007-48). The hit-stun stacking question (Q3) stays.

The data-model decisions of 7 and 10 October (D-20261007-53 to -59, D-20261010-01 to -04) answered two things before
they reached this list: the paired-reference question the owner raised ("why do we need references to finishers in all
these locations?", the 2026-10-07 log, E26) and the marker's home; and they closed the review's question of whose
settings a rule reads (4.15). They added four: the home of the global values D-20261007-53 does not name (Q23), how a
paired set holds player-finisher content (Q24), and the two ambiguities the stress test exposed, a row drag carrying the
slot's diff (Q25) and the set layer sitting below the attack layer (Q26).

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

**Q6. How long is the per-enemy attack cooldown?** When it starts is decided: at the token's return, which happens when
the attack ends for any reason [D-20261007-30]. Only the length is open.
- A. 1.0 s, today's value, now counted from the return. **(Recommended: the known value, counted so that it actually
  spaces attacks out.)**
- B. 0.5 s.
- C. 1.5 s.
- D. Set per enemy type, on the reaction profile.

**Q7. Do other enemies hold during a non-lethal follow-up strike that is not paired?** A paired follow-up is a paired
sequence involving the player, so others hold during it [D-20261007-29]. The single-character fallback strike is not.
- A. No: it is not a paired sequence, and D-20261007-29's scope is paired sequences. **(Recommended.)**
- B. Yes, as for a parry-counter, so that every follow-up plays the same against a crowd.
- C. Only against the struck enemy.
- D. Only while the strike's clip plays, with no resume delay after.

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

**Q15. Enemies' per-actor attack choices.** Enemies now pick slot rows of their kit's combo set by the selection
columns, weight and distance range [D-20261005-63, D-20261007-57], and each distinct enemy subset is a second kit that
shares the katana's weapon, defense set and paired set and differs in combo set [D-20261010-02]. Today
`BP_EnemyCharacter` lists `LightAttack_1`, and each of the four placed enemies in `Lvl_ThirdPerson1` overrides that list
with a different single attack; a weight stored on a shared combo set cannot express that.
- A. One enemy kit with its own combo set, seeded from the Blueprint's list, and a placed actor may override slot weights
  per instance (an optional map on the AI component). **(Recommended: one kit per enemy type, instance tweaks where
  needed.)**
- B. One kit per distinct list: four small combo sets, in four kits sharing the other three parts, for the four placed
  enemies.
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
- A. As proposed: the authoring foundation with the table evaluator, the editor with the timeline and the simulate-press
  panel, the data (the kit, the combat rules, the tables, the windows adopted as data), the dodge, then hits, guard,
  parry (with the runtime reading the windows), the parry-counter with paired lookup by key and free-mode aim, lock mode
  and attackers. **(Recommended: attack data changes once, as D-20261005-04 intended, and the dodge lands before
  hit-stun as D-20261007-21 asks.)**
- B. Rules first, on today's attack data, with authoring after.
- C. Data and rules first, and the editor last.
- D. A different order.

### Targeting

**Q19. Lock mode's distances: how far does a lock press look (and where does "out of range" begin), and how near must
the next enemy be for the automatic re-pick on death?** D-20261007-13 names neither, and D-20261007-48 adds a short
grace before an out-of-range or out-of-sight target ends the lock, also without a value. The spec has two distances
already: the 350 cm finisher range [D-20261005-28] and today's 500 cm soft-aim range.
- A. Both 350 cm, the finisher range; the grace a settings tunable felt on captures. **(Recommended: one number, and a
  lock never reaches past a finisher.)**
- B. Both 500 cm, today's soft-aim range; the grace as in A.
- C. The lock press looks to 500 cm; "near" and the out-of-range end are 350 cm; the grace as in A.
- D. Two separate distance tunables and the grace, all with no decided value, tuned on captures.

### The predictor, the push and the dodge

**Q20. The contact predictor's thresholds and its confidence scale.** D-20261007-24 gives each user its own threshold,
the finisher choice strict and the parry window lenient, and leaves the player's lethal strike unnamed; the register
marks the thresholds and the scale "not yet numbered (open question for the spec)".
- A. A confidence from 0 to 1, read as the fraction of the tolerance-widened blade path that reaches the capsule within
  the horizon; the lethal strike uses the finisher choice's strict threshold; the two numbers are set on the first
  predictor captures, with the parry window's lenient one chosen so that no attack visibly aimed at the player is ever
  refused as a candidate. **(Recommended: one scale, two thresholds, both felt before they are fixed.)**
- B. A yes-or-no per user with no scale, which D-20261007-24 rejected in favour of a confidence.
- C. Three thresholds, one per user, each a settings tunable, with the lethal strike's own value chosen separately.
- D. Thresholds per attack as well as per user, through the per-attack override that tolerance and horizon already
  have.

**Q21. The block shove's direction, its duration, and whether the existing push behaviours carry over.** D-20261007-27
fixes the shove's distances (Light 10 cm, Heavy 20 cm to start) and no duration. D-20261007-28 is the owner's "D. Decide
on captures" for the direction and for the carried-over behaviours (the stop at walls, the ledge rule, replacement on a
new push, the pause during hitstop); the register marks it "open, evidence pending". This question is to be answered
after the rendered knockback scenarios, with the captures in hand.
- A. Away from the attacker, over the type's hit-push duration (Light 0.2 s, Heavy 0.25 s from the code seeds), with
  every existing push behaviour carried over unchanged. **(Recommended as the first thing to capture: it needs no new
  rule, and the captures then show what to change.)**
- B. Along the attack's own push direction, as the hit push is, with the behaviours carried over.
- C. Away from the attacker, with a duration of its own per type, and replacement on a new push switched off for the
  shove so that a flurry of blocked lights does not stack pushes.
- D. Decided per capture scenario, with no single rule, which 1.1 would then need written out.

**Q22. The dodge's numbers: its distance, its duration and its i-frame window.** D-20261007-21 asks for them; the
owner's words (the 2026-10-07 log, E25) make them tunables for captures, homed on the dodge's own data with settings
defaults. None is decided, and this spec invents none.
- A. Start from the dodge clip's own root-motion travel and length, author the i-frame window on the first capture
  sweep, and fix no number before that sweep; the settings defaults are then filled from the chosen values.
  **(Recommended: the clip is the only honest starting point, and feel is judged on captures.)**
- B. Fixed starting numbers chosen by the owner now, before any clip is picked.
- C. Numbers borrowed from a reference game, tuned afterwards.
- D. A procedural dodge with settings-only values and no clip-authored travel, so the numbers are design values from
  the start.

### Data model and editor

**Q23. Which asset holds the global values D-20261007-53 does not name?** The decision lists the window minimum, the
lockout, the slowdown, the Staggered default, the aim cone, the ranges, the attacker cap and cooldowns, the predictor
settings, the telegraph set and the counter damage default for the rules asset, and type defaults plus the default kit
for the per-character settings. The hit-stun floor, the follow-up multiplier, the guard arc, block turn and fastest turn
defaults, the fitting budgets, the height-difference limit, the dodge defaults, the lock grace, the flick threshold, the
shove-back distance, the guard-facing values and the slow-motion watchdog are named by neither list (4.15).
- A. The combat rules hold every global value that is not a per-type default; the settings keep only the type defaults
  and the default kit, as the decision says. **(Recommended: by elimination, and one asset to open for any rule.)**
- B. The per-weapon-overridable defaults (guard arc, turn rates, fitting budgets, height limit, dodge defaults) stay on
  the per-character settings beside the type defaults; the rest go to the rules.
- C. No global default for the per-weapon-overridable values: each defense set, paired set and dodge asset must set
  them, and validation errors when one is unset.
- D. Decide each value by hand, with the list above as the form.

**Q24. How does a paired set "hold player-finisher content"** [D-20261007-55]? The decision makes the content's presence
the marker and says validation checks it; it does not say how a finisher row that targets the player is told from one
that targets an enemy (4.7).
- A. A separate finisher table, "finishers on the player", with the same direction key; the set can finish the player
  when that table has a row, and validation reads the count. **(Recommended: nothing is flagged by hand, and the lookup
  for an enemy's finisher reads one table.)**
- B. A victim column on the one finisher table (enemy or player), with the marker read as "any row whose victim is the
  player".
- C. A flag on the paired set, "can finish the player", that validation checks against the content it holds.
- D. A tag on the paired content itself, read through the seam.

**Q25. A row drag and the slot's diff.** The stress test noted that dragging a row to a new position carries its diff with
it, so a diff authored for position 3 travels to position 4 **[reported]**; no decision covers it (4.2, 4.4, 5.1).
- A. The diff travels with the row, and the grid says so in the drop tooltip; "keep overrides at position" is offered on
  the drop. **(Recommended: the slot is an attack plus a diff [D-20261007-35], so the diff is the slot's, and the owner
  sees the choice.)**
- B. The diff always travels with the row, with no option.
- C. The diff always stays at the position: the rows swap attacks, not overrides.
- D. A drag asks every time.

**Q26. The set layer sits below the attack layer.** The combo set's type default (layer 2) is overridden by an attack's
own value (layer 3), so a weapon cannot re-default an attack that sets its own value except slot by slot; the stress test
raised it and recommended keeping the order with a batch action **[reported]** (4.2).
- A. Keep the order [D-20261005-61, D-20261007-34] and add a grid batch action, "apply to all slots in this lane", that
  writes slot overrides. **(Recommended: predictable precedence, and the batch action is explicit.)**
- B. Allow a set-level "force" that beats the attack's value.
- C. Move the set layer above the attack layer.
- D. Leave it: a weapon that needs a different value extracts a new attack.

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
| D-20261005-01 to -04 | Combo set with edit-through; grid and node map; back-to-opener links; one spec. **D-20261005-01's "make unique" is replaced by "Extract to new attack" and its "optional per-field overrides" widened to every field by D-20261007-35** | 4.4, 5 |
| D-20261005-05 to -10 | The parry window, lockout, parryable by default, Staggered default, minimum and scale, slowdown. **D-20261005-09's "on the attacker's animation" is read through D-20261007-38:** the window is the attacker's, held as data | 3.2, 3.4 |
| D-20261005-11 to -15 | Guard arc and turn, no chip, block in recovery, the player's guard, counted guard break | 3.1, 3.5 |
| D-20261005-16, -17 | Interruption is pressure against armor | 3.6 |
| D-20261005-18 to -21 | Finisher eligibility, trigger, enemy finishers, others hold. **D-20261005-20 is narrowed by D-20261007-20** (chosen at the attack's start, against a player who is Staggered or in hit-stun, with prediction), **and its marker moves from the reaction profile to the kit's paired set by D-20261007-55**; **D-20261005-21's "swings already in progress finish" is superseded by D-20261007-44** | 3.8, 3.9, 4.7 |
| D-20261005-22 to -24 | Opportunity equals Staggered time; strike fallback; the parry is the counter (the parry-counter, since D-20261007-11) | 3.3 |
| D-20261005-25, -26 | Count by pressure; 4 within 3 s | 3.5 |
| D-20261005-27, -28 | 60° aim cone; 350 cm. **The cone's camera reference is replaced by D-20261007-12** (the stick direction, else the facing); the 60° stands | 3.3, 3.8, 3.13 |
| D-20261005-29 to -31 | Guard breakers; counter fallback order; guard-break Staggered | 3.3–3.5 |
| D-20261005-32, -33 | Lockout 0.25 s; slowdown bounds | 3.2 |
| D-20261005-34 to -37 | Hit-stun values; 2 attackers; lands first; what hit-stun blocks. **D-20261005-35's cap counts the attackers of whoever is targeted by D-20261007-54, and lives on the combat rules by D-20261007-53** | 3.7, 3.9, 4.15 |
| D-20261005-38, -39 | A lethal parry-counter ends in a kill, decided at the press; the follow-up strike. **The lethal-variant shape is amended by D-20261007-07:** optional, not required | 3.3, 3.8 |
| D-20261005-40 | The glossary; its parry and counter entries amended by D-20261007-11; its reaction-profile, defense-set, paired-set and weapon-data entries amended by D-20261007-54, D-20261007-55, D-20261010-01, D-20261010-02 and D-20261010-04 | 2, throughout |
| D-20261005-42 | Any lethal aimed strike plays a paired finisher; the glossary line amended | 1.3, 2.1, 3.8 |
| D-20261005-43 | The follow-up strike is paired when a clip matches | 3.3 |
| D-20261005-44 | Armor is the higher of the base and any active window | 2.1, 3.6 |
| D-20261005-45 | Counters keyed by direction and swing, lane optional | 3.3, 4.5, 4.6 |
| D-20261005-46 | Guard-break counting is a per-character toggle, off for the player. **D-20261007-54 makes the toggle, not the identity, what the rule reads** | 3.5, 4.8 |
| D-20261005-47 | No body types; finishers by direction only; no "can be finished" marker. **Its "normal death without a clip" is amended by D-20261007-22 and D-20261007-23:** an uncovered direction is fitted first, and an ordinary attack plays only when fitting fails; **one finisher per direction becomes a list by D-20261007-31** | 3.3, 3.8, 4.6, 4.8 |
| D-20261005-48 | Weapon data → combo set + defense set (plus the paired set since D-20261005-57); the reaction profile keeps the body's data. **Amended by D-20261010-02:** the root is the kit, which holds the weapon's physical data and the three sets | 2.1, 4 |
| D-20261005-49 | No weapon data means the combat settings' default weapon data. **Amended by D-20261010-02:** no kit means the character's combat settings' default kit | 4.3, 4.9, 6.1 |
| D-20261005-50 | The parry turn finishes exactly at contact: rate = angle ÷ time left; the slowdown is the safety valve | 3.2 |
| D-20261005-51 | Staggered ends at once after a non-lethal follow-up strike; one follow-up per parry-counter | 3.3, 3.4 |
| D-20261005-52 | The hit that breaks a guard deals no damage; the guard breaks and the defender is Staggered | 3.5 |
| D-20261005-53 | Hit-stun holds enemies as it holds the player; the AI resumes after | 3.7 |
| D-20261005-54 | A lethal counter with no lethal variant uses the finisher for that direction, else the non-lethal counter and the normal death. **Its middle step is superseded by D-20261007-07; the owner confirmed that reading in D-20261007-43** | 3.3 |
| D-20261005-55 | Guard enter and exit on the defense set; the Staggered reaction and recoil on the reaction profile | 4.6, 4.8, 7.4 |
| D-20261005-56 | Guard-break counting on in the base profile, off for the player | 3.5, 4.8 |
| D-20261005-57 | The paired set: a third weapon asset holding the counters and finishers by direction and role. **Its lethal-and-non-lethal-variant shape is amended by D-20261007-07; its finishers become a list per direction by D-20261007-31; its two "references" (the defense set's to counters, the combo set's to finishers) become lookups by key under D-20261010-01, and no set references into it** | 2.1, 4.1, 4.3, 4.7, 4.16, 5.9, 7 |
| D-20261005-58 | No push on a hit that does not interrupt | 3.6, 3.10 |
| D-20261005-59 | The block shove on a blocked hit; a push on the hit that breaks a guard. **D-20261007-27 gives both their own per-type defaults, not derived from the hit push, and supersedes the "40% of the type push" proposal this spec had placed beneath it** | 3.5, 3.10, 4.9 |
| D-20261005-60 | Normal pushes on victims that are Staggered or in hit-stun; none during paired sequences | 3.10 |
| D-20261005-61 | Four precedence layers; the combo set's is the per-weapon layer | 1.3, 4.2 |
| D-20261005-62 | The player is fully free after a non-lethal counter; aiming elsewhere ends the opening. **The aimed-elsewhere half is superseded by D-20261007-10**; the freedom stands | 3.3, 3.4 |
| D-20261005-63 | Enemies pick slots of their combo set by per-slot weight and range; AI attack lists retired. **Amended by D-20261007-57 and D-20261010-02:** the weight and range are selection columns on the slot row, and each distinct enemy subset is a second kit sharing the weapon, defense set and paired set | 4.1, 4.3, 4.4, 4.16, 6.4, 7.2 |
| D-20261005-64 | The parry succeeds on timing alone; room matters only for paired content | 3.2, 3.3, 3.8 |
| D-20261005-65 | Sections 1, 2 and 3.1 approved as written at `a34ecd69` | 1, 2, 3.1 |
| D-20261005-66 | A buffered Block press raises guard only if still held, and never parries | 3.1, 3.2 |
| D-20261005-67 | A Staggered player can do nothing until it ends; input buffered. **Superseded by D-20261005-88** | — |
| D-20261005-68 | A tunable lives with the thing it describes; the reaction profile covers reactions to hits only. D-20261010-03 keeps the split it draws | 2.1, 4.1, 4.6, 4.8, 4.9, 4.15 |
| D-20261005-69 | The block turn faces the attacker fully; its rate is a defense tunable, 360°/s to start | 3.1, 4.6, 4.9 |
| D-20261005-70 | The guard arc's default is in the combat settings, overridable on the defense set. **The default sits on the combat rules since D-20261007-53 (Q23)** | 3.1, 4.6, 4.15 |
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
| D-20261005-86 | A guard break grants an enemy nothing; a marked enemy's lethal hit still finishes. **Narrowed by D-20261007-20:** it finishes only as a finisher chosen at the attack's start, with prediction, never a promoted swing | 3.5, 3.8 |
| D-20261005-87 | A dodge press ends the player's hit-stun early, after max(0.12 s, half the stun); the dodge is committed. The dodge itself is designed in 3.14 since D-20261007-49 to -52 | 3.7, 3.14, 6.3 |
| D-20261005-88 | The player's Staggered state: reaction and full push; no attack, guard or stick movement; a dodge after the minimum ends it and closes the enemy's opening; 0.6 s by default on the player's profile. **The profile field, not the identity, is what the rule reads, by D-20261007-54** | 2.1, 3.1, 3.4, 3.5, 4.8, 6.3 |
| D-20261006-01 | Hit-stun and Staggered both run, each with its own timer and rules; free when both end; a dodge-out needs both minimums; a hit never shortens a lock or restores guard early | 3.4, 3.7, 6.3, 9.5 |
| D-20261006-02 | One general cancel notify on any non-attack montage, with one meaning per context; attacks keep the phase transition; missing means montage end with a warning | 2.2, 3.0, 3.3, 3.4, 3.6, 3.7, 4.6–4.8, 4.12, 4.13, 5.4, 7.3, 9.5, 9.8 |
| D-20261006-03 | Counter damage on the defense-set entry, settings default 50, a clip override optional; content, then damage, then the outcome; the fallback strike deals the entry's value. Amends D-20261005-80's home | 2.1, 2.2, 3.2, 3.3, 3.12, 4.6, 4.7, 4.9, 7.3 |
| D-20261006-04 | Rules before content: no rule from what content exists; content made, bought or fitted to the rules | 1.1, 1.3, 3.0, 3.3, 9.8 |
| D-20261006-05 | Counter damage lands at the counter's contact and never clamps | 3.3, 6.4 |
| D-20261006-06 | The player's own Staggered time always applies; an attack's or slot's override never changes it | 3.4, 3.12, 4.8, 6.4 |
| D-20261007-07 | Every counter can kill: lethal damage at the counter's contact kills there, the enemy's death outcome taking over; an authored kill sequence plays instead when one exists. Amends the lethal-variant shape of D-20261005-38 and D-20261005-57 | 1.3, 2.1, 2.2, 3.3, 3.4, 3.8, 4.6, 4.7, 4.12, 9.8 |
| D-20261007-08 | Inside a parry-counter each character is untouchable until its own cancel notify | 3.0, 3.3, 3.9, 9.10 |
| D-20261007-09 | An attack press inside the sequence is buffered and fires as the follow-up at the player's cancel notify; last input wins | 3.3, 6.3, 6.4 |
| D-20261007-10 | A press aimed elsewhere no longer ends the opening; Staggered runs its time. Supersedes the aimed-elsewhere half of D-20261005-62 | 3.3, 3.4 |
| D-20261007-11 | The words: parry (the deflect), counter (the strike back), parry-counter (the one-press sequence). Amends the glossary's parry and counter entries | 2.1, throughout |
| D-20261007-12 | Free-mode aim: one rule for every press; the cone around the stick direction, else the facing; Staggered or eligible win ties, nearest otherwise; the camera never decides. Replaces D-20261005-27's camera reference | 2.1, 2.2, 3.3, 3.8, 3.11, 3.12, 3.13, 4.9, 6.3, 6.4 |
| D-20261007-13 | Lock mode: the lock press, the locked target for every press, the strafe, the flick switch, the drop on death with a re-pick, the second press | 2.2, 3.3, 3.8, 3.11, 3.12, 3.13, 4.9, 6.3, 6.4, 9.9 |
| D-20261007-14 | The targeting and camera design lives in this spec, as 3.13 | 3.13 |
| D-20261007-15 | The counted guard break is built with the guard-break stage, tested on fixtures, and dormant until a shipped enemy guards | 3.5, 6.4, 9.6 |
| D-20261007-16 | Unblockable wins over guard breaker; validation warns on the pair | 3.1, 3.5, 4.4, 4.5, 9.6 |
| D-20261007-17 | A light press with a Staggered target is the follow-up with the light opener; the hold does not open; directional lights never follow up | 3.3, 6.3, 9.8 |
| D-20261007-18 | The lenient lockout: a press during it is remembered and counts if the window is still open at its end; the lockout may go to zero. **My reading of "the more lenient version"; the owner may veto** | 2.1, 3.2, 3.12, 4.9, 6.1, 6.3, 6.4, 9.7 |
| D-20261007-19 | Contact order: all weapon contacts of a frame judged against the frame-start state (a trade lands both); damage first; a killing hit replaces the reaction; among surviving interrupting hits the highest pressure, then the latest, supplies the reaction, hit-stun and push | 3.6, 3.10, 6.3, 9.5 |
| D-20261007-20 | Enemy finisher on the player: chosen as the marked enemy's next attack, at that attack's start, only when the player is Staggered or in hit-stun and the predictor says the hit lands; never a mid-swing promotion. Narrows D-20261005-20 and D-20261005-86 | 3.4, 3.7, 3.8, 3.15, 6.3, 6.4, 9.8 |
| D-20261007-21 | The dodge is designed in this spec and delivered as a small stage before hit-stun ships; its numbers are tunables for captures | 3.14, 4.14, 9.4, Q22 |
| D-20261007-22 | Lethal aimed strikes: coverage of every direction is authoring, not a rule; "will it hit" is predicted, never guessed; an uncovered direction is fitted to a clip; an ordinary attack only when fitting fails, and it kills only if it lands. Amends D-20261005-47's "normal death without a clip" | 1.3, 3.0, 3.3, 3.8, 3.11, 3.15, 6.3, 9.8 |
| D-20261007-23 | Fit to clip: the approach warp to the nearest authored direction, a small victim turn, budgets on the paired set with settings defaults, validation listing uncovered directions, procedural blending at the seam with a rendered check | 2.2, 3.8, 3.12, 4.7, 4.9, 5.9, 6.4, 9.8 |
| D-20261007-24 | The contact predictor: whether and when the blade path reaches the target's capsule, with a confidence; a threshold per user (finisher choice strict, parry window lenient); tolerance and horizon in combat settings with per-attack overrides (the combat rules since D-20261007-53); users: the parry window, the enemy's finisher choice, the player's lethal strike. **Thresholds and scale not numbered (Q20)** | 1.3, 2.2, 3.0, 3.2, 3.8, 3.12, 3.15, 4.9, 6.1, 6.4, 9.7 |
| D-20261007-25 | An interrupting hit on a guarding character (out of arc, or unblockable) ends the guard; it returns at once while Block is still held, never as a parry press; a flinch never ends guard | 3.1, 3.5, 3.6, 3.7, 6.3, 6.4, 9.5 |
| D-20261007-26 | Armor in the locked states: 0 while Staggered whatever the base; the base in hit-stun; super armor (a window above any pressure) never interrupted, the flinch still plays | 2.2, 3.4, 3.6, 3.7, 3.11, 3.12, 6.4, 9.5 |
| D-20261007-27 | The block shove and the guard-break push each have their own per-type defaults (shove Light 10 cm, Heavy 20 cm to start; break push to be tuned), with the hit push's per-attack and per-slot overrides; not derived from the hit push. Supersedes the 40% proposal under D-20261005-59 | 2.2, 3.4, 3.5, 3.10, 3.12, 4.5, 4.9, 6.4, 9.5 |
| D-20261007-28 | The shove's direction and the carried-over push behaviours (walls, ledges, replacement, hitstop pause) are decided on rendered captures; no rule yet. **Open, evidence pending (Q21)** | 3.10, 3.11, 3.12, 9.5, Q21 |
| D-20261007-29 | Others hold: in progress means the montage has started, in any phase; a combo continuation is a new attack and holds; an unstarted token is returned; holding enemies re-enter the queue with a per-enemy resume delay (settings, per-profile override); applies to every paired sequence involving the player, an enemy's finisher included. **Its "swings already started finish, contact ignored" part is superseded by D-20261007-44** | 2.2, 3.8, 3.9, 3.12, 4.8, 4.9, 6.3, 6.4, 9.10 |
| D-20261007-30 | Attack tokens: held from grant until the attack ends for any reason (finished, interrupted, parried, recoiled, death, Staggered); the cooldown starts at the return; a freed token waits a short re-grant delay; an enemy's finisher counts as its attack. Answers the 3.3–3.5 review's item 13 | 2.2, 3.4, 3.8, 3.9, 3.12, 4.9, 6.1, 6.3, 6.4, 9.10, Q6 |
| D-20261007-31 | Paired finishers: a list of clips per direction, chosen by the counters' variety rule (avoid the last two played); no throttle. Amends the one-per-direction shape of D-20261005-47 and D-20261005-57 | 2.1, 2.2, 3.3, 3.8, 3.12, 4.4, 4.7, 5.9, 6.3, 6.4, 9.3, 9.8 |
| D-20261007-32 | A push in flight: only a dodge cancels the remaining push; attack, guard and movement input run on top of it; the editor warns when an attack's hit-stun is shorter than its push | 3.7, 3.10, 3.14, 4.4, 6.1, 6.3, 6.4, 9.4, 9.5 |
| D-20261007-33 | A Heavy press with a Staggered target is the follow-up strike with the heavy opener at ×1.5; the charge is ignored and the hold does not open; lethal becomes a paired finisher. Closes the former question on the Heavy press | 3.3, 6.3, 9.8 |
| D-20261007-34 | The four precedence layers stay; the combo-set type-default layer is hidden in the editor until more than one combo set would differ from the settings | 4.2, 5.3, 9.2 |
| D-20261007-35 | The slot is an attack plus a diff; every attack field is overridable per slot; the row shows only the diff (groups with counts; "show inherited"); "Save to asset" per overridden field; "Extract to new attack" replaces "make unique"; diff markers on grid cells. The montage interface it asked for was then settled by D-20261007-38 to -40; **the override breadth was confirmed against the survey by D-20261007-59** | 1.1, 1.3, 2.1, 2.2, 4.2, 4.4, 4.5, 4.11, 4.17, 5.1, 5.3, 6.4, 9.2, 9.3 |
| D-20261007-36 | Timing windows are anchored to sequence time plus segment, resolved to montage time on load or cook | 1.3, 2.2, 3.2, 4.5, 4.13, 7.3, 9.3 |
| D-20261007-37 | Readable flags are telegraphed automatically: a telegraph set keyed by flag fires at attack start from the resolved attack spec, so a slot override and its cue cannot disagree; the cues are chosen from rendered captures. The telegraph set is among the combat rules since D-20261007-53 | 2.2, 4.5, 4.11, 4.15, 5.3, 5.11, 6.3, 6.4, 6.5, 9.7 |
| D-20261007-38 | Attack data owns every timing window (phases, parry window, hold start, armor windows, contact marker, cancel point), with a per-slot diff; the runtime reads them from the resolved attack spec; montage notifies are export/import only: the generator becomes "export to montage", the timing action "adopt from montage"; the timing strip edits the data (the timeline, since D-20261007-58). Amends "ParryWindow is on the attacker's montage" to "the parry window is the attacker's, held as data" | 1.1, 1.3, 2.1, 2.2, 3.0, 3.2, 3.6, 4.4, 4.5, 4.11, 4.13, 5.4, 5.11, 6.4, 6.5, 7.2, 7.3, 9.3, 9.7 |
| D-20261007-39 | One neutral notify state spanning the attack section drives the data windows (open, refresh for play rate, close), following play rate, pause and time dilation, dying with the montage instance; component tick was offered and the owner kept the notify state | 1.3, 4.5, 6.4, 6.5, 9.7 |
| D-20261007-40 | A hand-moved derived notify changes nothing in play; derived notifies draw in a distinct colour with "(from attack data)"; on saving the montage a fingerprint mismatch prompts Save / Reset / Leave, with the meanings in 4.13. Stays decided under D-20261007-58, with less weight, since the Persona export is optional | 2.2, 4.13, 5.4, 5.11, 6.4, 9.3 |
| D-20261007-41 | The single-character counter and the follow-up strike land as scripted hits at the clip's contact marker; the counter applies no hit-stun or push (Staggered follows at the cancel notify); a follow-up applies the hit-stun and push of the attack the press resolved to | 2.2, 3.3, 3.6, 3.10, 6.3, 6.4, 9.8 |
| D-20261007-42 | Paired content declares the ground state each role needs (grounded, airborne, any; both grounded by default) and a height-difference limit with a settings default; the victim is never invulnerable; a mismatch counts as fitting failed | 3.8, 3.11, 3.12, 4.7, 4.9, 4.12, 5.9, 6.4, 9.3, 9.8 |
| D-20261007-43 | The writer's readings stand: the kill sequence lives on the paired counter the defense entry references; D-20261005-54's "borrow a fitted finisher" middle step stays dropped | 3.3, 9.8, Appendix B 42 and 43 |
| D-20261007-44 | When the player's parry-counter or paired finisher starts, a second enemy whose swing is in progress aborts and is shoved back: its attack cancelled with a recoil or abort reaction, a short settings push away from the sequence (tuned on captures), its token returned, and it holds. Supersedes part of D-20261007-29, D-20261005-21's "swings already in progress finish" and the earlier reading 47 | 2.1, 2.2, 3.8, 3.9, 3.10, 3.12, 4.9, 6.3, 6.4, 9.10 |
| D-20261007-45 | A buffered Light or Heavy press starts its attack when the state that held it ends; at that attack's hold-window start the button state is checked as usual, so holding through and releasing after yields a directional light or a charged heavy | 3.3, 3.4, 3.7, 3.13, 6.3, 6.4, 9.5 |
| D-20261007-46 | Lock follows action: acting toward a different enemy while locked (a directional light, a parry-counter, a follow-up or finisher) transfers the lock to it; the strafe re-centres; the lock never holds the player to a target they are no longer fighting | 2.2, 3.3, 3.8, 3.13, 6.3, 6.4, 9.8, 9.9 |
| D-20261007-47 | While locked the stick aims a directional light as in free mode, toward the nearest enemy that way (camera-relative), transferring the lock; with no enemy that way the lock breaks and the attack goes in that direction and misses | 3.13, 3.14, 6.3, 6.4, 9.9 |
| D-20261007-48 | A lock ends or moves when: the target dies (re-pick a near enemy, else free); out of range or sight for a short settings grace; a second press; the player Staggered or finished; a light held for a directional attack; a Block that blocks a different attacker (**my reading:** the lock transfers; owner may veto); "anything else that keeps combat consistent" | 3.4, 3.12, 3.13, 3.14, 6.1, 6.3, 6.4, 9.9, Q19 |
| D-20261007-49 | Dodge direction: the stick, camera-relative; a neutral stick dodges away from the current threat, or straight back; eight-way or free-angle is a content choice | 2.2, 3.12, 3.14, 4.14, 6.3, 9.4 |
| D-20261007-50 | The dodge carries an authored invulnerability window, data-owned and sequence-anchored like every other window; its default is tuned on captures | 2.2, 3.0, 3.12, 3.14, 4.13, 4.14, 6.3, 9.4, Q22 |
| D-20261007-51 | Dodge cancels: a dodge press ends an attack's Recovery, drops guard, and escapes hit-stun or Staggered after their minimums; never Windup or Active; the dodge has its own cancel point, after which an attack press starts the opener, before which it is buffered | 3.0, 3.1, 3.4, 3.7, 3.14, 4.14, 6.3, 9.4 |
| D-20261007-52 | No resource and no cooldown; consecutive dodges allowed; a dodge press during a dodge is buffered to its cancel point | 2.2, 3.14, 6.3, 9.4 |
| D-20261007-53 | A project-wide combat rules asset is split from the per-character combat settings: it holds everything that must agree for everyone (window minimum, lockout, slowdown, Staggered default, aim cone, ranges, attacker cap and cooldowns, predictor settings, telegraph set, counter damage default); the settings keep type defaults and the default kit. **The placement of the global values the list does not name is this spec's reading (Q23)** | 1.3, 2.2, 3.1–3.15, 4.1, 4.2, 4.9, 4.10, 4.11, 4.15, 6.1, 6.3, 6.4, 6.5, 7.4, 7.5, 9.3 |
| D-20261007-54 | The player is identified by data, not identity: every player-specific rule is a profile field (counting toggle, own Staggered time, dodge escape allowed); tokens count the attackers of whoever is targeted; no is-player branch; one shipped-content invariant checks the player's profile has counting off | 1.3, 2.1, 2.2, 3.4, 3.5, 3.7, 3.9, 3.12, 3.14, 4.1, 4.8, 6.3, 6.4, 7.7, 8.1, 8.2, 9.3 |
| D-20261007-55 | The "can finish the player" marker moves from the reaction profile to the kit's paired set: a paired set that holds player-finisher content can finish the player; validation checks it. Amends the glossary's reaction-profile entry. **How the content is told apart is Q24** | 1.3, 2.1, 3.8, 3.12, 4.1, 4.7, 4.8, 5.9, 6.4, 9.3, 9.8, Q24 |
| D-20261007-56 | The storage model stands after the research pass: weapon → combo set + defense set + paired set; reaction profile per character; global rules asset; slot = attack + diff; data-owned windows. Evidence: the stress test and the survey, both model-written, in `.superpowers/design/` | header, 4.17, Appendix B |
| D-20261007-57 | Table-shaped selection: the counter grid, finisher lists, enemy slot choice and opener/entry rules share one column vocabulary, one evaluator and one "rows evaluated" trace shown in a simulate-press panel; in-house table structs; Epic's Chooser a reference for columns and debugger shape, not a dependency | 1.3, 2.1, 2.2, 3.3, 4.1, 4.4, 4.6, 4.7, 4.16, 5.9, 5.12, 6.2, 6.3, 6.4, 8.1, 8.2, 9.1, 9.2, 9.3 |
| D-20261007-58 | Timeline-first attack editor: opening an attack or a slot opens a timeline whose lanes are the data windows, phases, contact, armor, hits, telegraph and FX moments over the montage, plus a combo lane; the grid stays for topology and its row strip becomes a link; the Persona export is optional, Save / Reset / Leave stays decided. **The "hits" and "FX moments" lanes' contents are this spec's reading** | 2.2, 3.2, 5.1, 5.3, 5.4, 5.10, 5.11, 6.4, 9.2 |
| D-20261007-59 | Override breadth confirmed: every attack field stays overridable per slot (D-20261007-35); the diff-only row, source labels, named pills, Save to asset and Extract to new attack are the guards against the divergence ComboCreator's narrower rule warns about | 4.17, 9.3 |
| D-20261010-01 | Paired content is found by key through the selection tables, with no cross-references: a lethal strike asks the attacker's kit's paired set for the finisher rows of its direction; a defense entry asks the same kit's paired set for counter rows matching its direction and swing, lane preferred, variety rule applied; an entry may opt out of a paired counter; the combo set's finisher references are dropped. **Amends the reference wording of D-20261005-48 and D-20261005-57** | 1.3, 2.1, 2.2, 3.3, 3.8, 4.1, 4.3, 4.4, 4.5, 4.6, 4.7, 4.16, 5.6, 5.9, 6.4, 7.3, 7.4, 7.7, 8.2, 9.3, 9.8 |
| D-20261010-02 | The root asset is a Kit: the weapon's physical data (mesh, sockets, traces, multiplier) plus a combo set, a defense set and a paired set; two kits may share the weapon data and the defense and paired sets while differing in combo set. Replaces "weapon data" as the root. **Amends D-20261005-48, D-20261005-49 and D-20261005-63; whether `UWeaponData` stays a sub-asset is a proposed shape** | 1.2, 1.3, 2.1, 2.2, 3.1, 3.3, 3.8, 4.1, 4.3, 4.9, 5.6, 6.1, 6.4, 7.1, 7.2, 7.5, 7.7, 8.2, 9.3 |
| D-20261010-03 | Defense ownership split kept: what the weapon animates (block reactions, guard clips, deflects, counter entries, arc and turn overrides) lives on the defense set; what the body decides (guard-break counting, armor, Staggered time) lives on the reaction profile. The survey's counter-example, Ninja Combat's defense on the target, is the rejected alternative. **Its "Staggered time" is read as the character's own (D-20261005-88)** | 4.1, 4.6, 4.8, 4.17, Appendix B |
| D-20261010-04 | One paired-set entry is one two-character content asset; the content's internal variants (Duet's angle sector, distance, weight) are alternates of that clip, never the direction choice; direction, swing and the variety rule are decided by our selection tables; ground state and height are read from the content through the seam, never copied into a row | 2.1, 2.2, 3.8, 4.5, 4.7, 4.12, 5.9, 6.4, 7.3, 8.2, 9.3 |

**Superseded, and not used as rules:**
- D-20261004-28, the two modes by enemy type, superseded by D-20261005-24;
- D-20261004-36's window lead, superseded by D-20261005-05;
- D-20261004-11 and D-20261004-13, superseded by D-20261004-20 and D-20261004-21;
- D-20261004-22's placement of parry entries on the reaction profile, and D-20261004-20's "the reaction profile holds
  block and parry", superseded by D-20261005-48;
- D-20261005-62's aimed-elsewhere half (a press elsewhere ends the opening), superseded by D-20261007-10;
- D-20261005-67, superseded by D-20261005-88;
- D-20261005-21's "swings already in progress finish" and the matching part of D-20261007-29 ("swings already started
  finish, contact ignored"), superseded by D-20261007-44;
- D-20261005-01's "make unique", replaced by "Extract to new attack" [D-20261007-35];
- the "40% of the type's hit push" proposal this spec placed under D-20261005-59, superseded by D-20261007-27;
- D-20261005-47's "normal death without a clip" as the first answer to a missing direction, amended by D-20261007-22 and
  D-20261007-23 (fitting first);
- D-20261005-54's middle step, superseded by D-20261007-07 and confirmed dropped by D-20261007-43;
- the 3.3–3.5 review's latch and end-position items: closed. There is no latch and no end-position rule (3.3, the
  2026-10-07 log E8–E9), and D-20261007-43 confirmed the readings that followed; its item 13, the Staggered enemy's
  token, is answered by D-20261007-30;
- "weapon data" as the root asset, this spec's name under D-20261005-48 and D-20261005-49, renamed and reshaped as the
  kit [D-20261010-02]; "weapon" stays for the physical data;
- the combo set's per-direction finisher references and the defense entry's counter reference, this spec's proposed
  shapes under D-20261005-57, replaced by lookups by key [D-20261010-01];
- the "can finish the player" marker on the reaction profile, this spec's placement under D-20261005-20, moved to the
  kit's paired set as content presence [D-20261007-55];
- the global rules on `UCombatSettings`, under D-20261003-08 and D-20261004-20, split into the combat rules
  [D-20261007-53];
- the editable timing strip inside the grid row, replaced by the timeline with the strip as a link [D-20261007-58];
- the enemy's own combo set with per-slot weights as the whole answer to D-20261005-63, now a second kit per subset with
  the weights as selection columns [D-20261010-02, D-20261007-57];
- the review of sections 6 to 9's question "whose combat settings does a rule read", closed by the rules split
  [D-20261007-53].

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
- the finisher pool's home: the weapon's paired set, referenced by the combo set [D-20261005-57]; the reference has since
  become a lookup by key from the kit's paired set [D-20261010-01];
- knockback by outcome: none on a flinch, a block shove on a block, a push on a guard break (since given its own
  default by D-20261007-27), normal on victims that are Staggered or in hit-stun, none in paired sequences
  [D-20261005-58, D-20261005-59, D-20261005-60];
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
  lenient lockout [D-20261007-15 to D-20261007-18];
- the hits review: the contact order; the enemy's finisher at its attack's start; the dodge designed here; lethal strikes
  predicted and fitted; the fit's budgets and seam blend; the contact predictor [D-20261007-19 to D-20261007-24];
- guard on an interrupting hit; armor in the locked states and super armor; the shove and break push on their own
  defaults; the shove's direction left to captures [D-20261007-25 to D-20261007-28];
- others hold, the tokens, finisher lists and the push in flight [D-20261007-29 to D-20261007-32]; the Heavy press on a
  Staggered target [D-20261007-33];
- the four layers with the set layer hidden; the slot as a diff; windows as data, anchored, scheduled and mirrored;
  the telegraph; the Save / Reset / Leave prompt [D-20261007-34 to D-20261007-40];
- scripted hits; per-clip requirements; the writer's two readings confirmed; the second attacker's abort and shove-back
  [D-20261007-41 to D-20261007-44];
- buffered holds; the lock following action; the stick-aimed directional light while locked; what ends a lock
  [D-20261007-45 to D-20261007-48]; the dodge's direction, i-frame window, cancels and limits [D-20261007-49 to
  D-20261007-52];
- the settings' ownership and whose asset is read: a project-wide combat rules asset split from the per-character
  settings [D-20261007-53]; the player by data [D-20261007-54]; the marker's home [D-20261007-55];
- the data-model research: the storage model kept, table-shaped selection, the timeline editor, the override breadth
  [D-20261007-56 to D-20261007-59]; paired lookup by key, the kit, the defense split, one entry per content asset
  [D-20261010-01 to D-20261010-04]. The paired-reference question the owner raised at E26 ("why do we need references
  to finishers in all these locations?") is answered by D-20261010-01 before it reached section 10.

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
   D-20261005-57 moves both into the paired set. The defense set kept a reference until D-20261010-01 made it a lookup by
   key; the glossary entry is marked amended.
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
    your own cancel notify), then settled for the second attacker by D-20261007-44 (it aborts and is shoved back; item
    47 below).
16. "A short cooldown" in D-20261005-35: its start is decided by D-20261007-30 (at the token's return); its length is Q6.
17. No decision names a clock (Q8).
18. D-20261005-59 names the block shove's per-type default but no value; the values in 3.10 are proposed.
19. D-20261005-63 stores AI weights per slot, which a shared combo set cannot vary per placed actor (Q15).

**Readings this spec chose, for the owner to see:**

20. D-20261005-24's "tough ones two" is narrowed by D-20261005-39 to "several exchanges".
21. The counter is read as a strike that drops guard under D-20261005-13.
22. A guard break's Staggered time comes from the breaking attack, and starts at the breaking contact (D-20261005-31).
23. The directional-light guard-breaker default applies to slots reached through a hold link.
24. A window cannot open before its attack starts. This is arithmetic, not a decision.
25. The limit of two attackers counts the attackers of whoever is targeted, by D-20261007-54 (this reading had said the
    player's attackers); it is set in the combat rules [D-20261007-53].
26. The 0.12 s floor came from a proposed, unmerged hit-reaction spec. The owner adopted the number by picking
    D-20261005-34; the option's description (session transcript, 04:02 EDT) calls it "the 0.12 s floor decided earlier".
27. The glossary's back-to-opener entry uses a word that its own last table retires, for the parry-counter. This spec
    says "combo" for a run of linked attacks.
28. The owner's held knockback values (60 cm and 150 cm) come from a condensed summary. Check them on the asset.
29. An entry that names the parried attack's lane is preferred over one that does not, as the ordering of D-20261005-45's
    "optional refinement". **Decided since** by D-20261010-01's "lane preferred".
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
36. **Replaced by D-20261010-01.** The combo set's finisher references (one per direction) and the defense entry's
    counter reference were this spec's shapes under D-20261005-57; both are now lookups by key, and the paired set's
    counters carry swing and lane as key columns (4.7).
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
    the entry reaches it through its reference. **Confirmed by the owner** [D-20261007-43]: "No, keep both as the
    writer read them".
43. D-20261005-54's middle step (the finisher pool for a lethal counter without a lethal variant) is read as superseded
    by D-20261007-07: no separate kill clip is needed, so the pool is not consulted for a parry-counter, and a lethal
    press with no kill sequence kills at the counter's contact. **Confirmed by the owner** [D-20261007-43]; the register
    calls it the "borrow a fitted finisher" middle step, which "stays dropped".
44. D-20261005-81's "when the counter sequence ends" is read as the enemy's cancel notify on its half of the counter
    (D-20261006-02, D-20261007-07).
45. D-20261007-12's "nearest otherwise": among several Staggered or eligible enemies, the nearest wins. With that, the
    tie rule is settled and the former question on it is removed.
46. The stick direction (Free-1) is read in the frame movement uses, as the owner's "attack in the direction you are
    pressing" implies; no decision names the frame.
47. **Replaced by D-20261007-44.** The earlier reading here (an in-progress swing's contact on an untouchable character
    is ignored and the swing continues to its end) is superseded: when the player's parry-counter or paired finisher
    starts, a second enemy whose swing is already in progress aborts and is shoved back for spacing: its attack is
    cancelled at that moment with a recoil or abort reaction, it is pushed a short settings distance away from the
    sequence (default tuned on captures), its token returns, and it holds (3.9). The protection of D-20261007-08 and
    Contact-3 still stands beneath it; with the abort, no in-progress swing reaches either character.
48. Lock-1 with no candidate does nothing and leaves free mode in place; the lock does not change what guard faces.
    D-20261007-13 says neither.
49. D-20261006-03's order "content first, then damage, then the variant" is read with the room check and the variety
    rule inside "content", so that when the fallback strike is chosen for lack of room, the damage read is the entry's
    value, which the fallback strike always deals.
50. D-20261007-17 names the light press only. The Heavy press on a Staggered target was then decided by D-20261007-33:
    the heavy opener at ×1.5, the charge ignored and the hold not opening; the former question is removed.
51. The cancel notify's shape (one class, no payload, one per montage, ignored on attacks with a warning) is proposed;
    D-20261006-02 fixes its meanings, not its class (4.13).
52. D-20261006-01's "no guard while Staggered, whatever the hit-stun allows" is read as Guard-2 taking precedence over
    Guard-4 while both states run.
53. An ordinary attack has no range in free mode beyond today's attack warp; D-20261007-12 names no range, and the
    350 cm of D-20261005-28 and D-20261005-85 bound the paired finisher and the follow-up strike only.
54. Lock mode's distances (how far a lock press looks, and "near" for the re-pick on death) and the flick threshold are
    not in D-20261007-13; the distances are Q19 and the threshold is a proposed tunable with no decided value.
    D-20261007-48 adds the out-of-range and out-of-sight grace, "short", also without a value (Q19).

**Readings of the decisions D-20261007-19 to -52, for the owner to see.** The register's own hedges are carried
verbatim.

55. **D-20261007-48, the Block case.** The register's words: "a Block that blocks a different attacker (**my reading:**
    the lock transfers to that attacker, consistent with D-46 and the block turn; owner may veto)". This spec builds
    End-6 (3.13) on that reading and keeps the hedge.
56. **D-20261007-48's "anything else that keeps combat consistent".** The owner's words delegate further lock-ending
    events to the principle "the lock follows what you actually do". Under 1.1 an implementer does not choose a rule
    quietly, so this spec reads the delegation as: apply the principle, and list every further event in the
    implementation brief for the owner to see (End-7).
57. **D-20261007-24's thresholds and scale.** The register: "thresholds and the confidence scale are not yet numbered
    (open question for the spec)". Q20 asks them; 3.15 names no number. The player's lethal strike has no named
    threshold in the decision; Q20's recommendation pairs it with the finisher choice's strict one.
58. **D-20261007-24's lenient threshold for the parry window** against D-20261005-64, which removed the parry's
    confidence gate. This spec reads the lenient threshold as deciding only whether an attack is reaching the defender at
    all (Parry-3's "aimed at" and "in progress") and the arrival time; it never decides the parry's success, which stays
    timing alone [D-20261005-64, D-20261005-79]. A lenient threshold "chosen so that no attack visibly aimed at the
    player is ever refused" (Q20 A) keeps the two decisions consistent.
59. **D-20261007-28.** The register: "Status: open, evidence pending (rendered knockback scenarios)". Q21 is that question,
    to be answered after the captures; 3.10 and 3.11 keep today's push behaviours until then. D-20261007-27 names the
    shove's distances and no duration; the duration is folded into Q21.
60. **D-20261007-29's "per-profile override"** for the resume delay. This spec reads "profile" as the enemy's reaction
    profile, the only per-character profile it has (4.8), although D-20261005-68 keeps that profile to reactions to hits.
    The owner may prefer a separate AI profile when one exists. The default sits on the combat rules, read as one of
    D-20261007-53's "cooldowns" (4.15).
61. **D-20261007-44's "recoil or abort reaction".** The decision names the kind of clip, not the asset. This spec reads
    it as the reaction profile's recoil [D-20261005-55], with an abort reaction as content when one is authored.
62. **D-20261007-19's "then the latest".** Among surviving interrupting hits of equal pressure, "the latest" is read as
    the later contact time within the frame.
63. **D-20261007-41's scope.** The decision names "the single-character counter and follow-up strikes". A paired counter
    keeps landing its damage at its sync moment (4.12); the scripted hit is for the unpaired clips.
64. **D-20261007-20 against D-20261005-86.** D-20261005-86's "a marked enemy's lethal hit on the Staggered player still
    finishes" is narrowed: the finisher is chosen at the attack's start, against a player who is Staggered or in
    hit-stun, with the predictor's confirmation; a lethal swing already in progress is never promoted. The narrowing is the owner's own
    words ("enemies shouldnt be transitioning between normal combo attacks into finishers").
65. **D-20261007-37's home.** The register's headline puts the telegraph set "in the pooled FX asset keyed by flag"; the
    owner's pick reads "Automatic cue from combat settings". D-20261007-53 then listed the telegraph set among the combat
    rules, so this spec reads the pooled FX asset as reached through the rules asset's telegraph set (4.15, 6.5), and all
    three are true; the cues themselves are chosen on captures.
66. **D-20261007-38's reach.** It gives attack data the attack's windows. This spec reads it as leaving alone the cancel
    notify on non-attack montages (D-20261006-02) and armor windows on a character's non-attack animations, because no
    attack data owns those clips (3.0, 3.6, 4.13).
67. **The dodge's cancel point as data.** D-20261007-51 gives the dodge "its own cancel point" and D-20261007-50 makes
    its i-frame window "data-owned and sequence-anchored like every other window". This spec holds the cancel point on
    the dodge's data beside the i-frame window (3.14, 4.14); a cancel notify on the dodge montage would be the other
    reading.
68. **A dodge press in Windup or Active.** D-20261007-51 says the dodge "never cancels Windup or Active" and no more. By
    principle 5 (input is never lost) this spec buffers the press to Recovery (Dodge-3). A dodge press inside the
    player's own sequence is buffered to the cancel notify by D-20261007-09's rule for presses (Dodge-2); D-20261007-51
    names no sequence.
69. **A dodge and the lock.** D-20261007-48 does not list the dodge among the events that end a lock, so a dodge leaves
    the lock in place (3.14).
70. **D-20261007-47 and D-20261007-49 say "camera-relative"** for the stick. That supports reading 46 (the stick's frame
    is the one movement uses, the camera's); the camera still never picks the target (Free-5).
71. **D-20261007-35's montage interface.** The register marks it "pending research" at that decision; D-20261007-38 to
    -40 then settled it (data owns the windows; notifies are export/import). This spec treats the research as closed by
    those decisions.
72. **The stale-mirror warning and health-report entry** (4.4, 4.13, 5.6) are this spec's proposal, from the side-agent
    note the owner forwarded (the 2026-10-07 log, E23); D-20261007-40 fixes the save prompt only.
73. **D-20261007-42's "defaults keep today's behaviour"** for the height-difference limit: today has no explicit limit
    **[reported]**, so the settings default has no value here; it is a tunable with no decided value (3.12).
74. **D-20261007-23's budgets** have no values in the decision; this spec lists them as tunables with no decided value,
    set on the captures that verify the seam blend, and asks no separate question.
75. **D-20261007-21's "forms pending"** on the dodge's mechanics was answered by D-20261007-49 to -52 for the direction,
    the i-frame window, the cancels and the limits; the numbers remain tunables for captures (Q22).
76. **"Extract to new attack" and a shared montage section.** The design pass left open whether the action should also
    offer a new section in the same montage; since windows are data [D-20261007-38] nothing in play depends on it, so
    this spec leaves it to the editor stage.
77. **A lock press outside free play.** No decision says when a lock press is refused. The feature set reads it as
    allowed mid-attack and while guarding, and leaves it open inside a sequence, in hit-stun and while Staggered. This
    spec refuses it nowhere (Lock-1 acts "at that moment"); the owner may want it refused while Staggered, since End-4
    ends a lock there.

**Readings of the decisions D-20261007-53 to -59 and D-20261010-01 to -04, for the owner to see.** The register's own
hedges are carried verbatim; the two research reports are model-written and are cited as pointers, never as evidence.

78. **D-20261007-53's list and the values it does not name.** The register names the rules asset's contents as "window
    minimum, lockout, slowdown, Staggered default, aim cone, ranges, attacker cap and cooldowns, predictor settings,
    telegraph set, counter damage default" and the settings' as "type defaults and the default weapon data". This spec
    places every other global value that is not a per-type default on the rules asset by elimination (4.15) and asks
    the owner to confirm (Q23). "Ranges" is read to cover the finisher and follow-up range and the lock range;
    "cooldowns" to cover the per-enemy cooldown, the re-grant delay and the resume delay default.
79. **D-20261007-53's "per-character combat settings" and whose asset is read.** A per-type default is layer 1 of an
    attack's precedence (4.2), so this spec reads it from the settings of the character whose attack it is, when the
    resolved attack spec is built (4.11). The decision says the settings are per character and does not name the side;
    the reading closes the question the review of sections 6 to 9 raised.
80. **D-20261007-54's "dodge escape allowed".** The decision names the field and not the base profile's value. This spec
    sets it on for the player and off in the base profile, by analogy with the counting toggle and because no rule gives
    an enemy a dodge (3.4, 4.8); the owner may want it on everywhere once enemies dodge.
81. **D-20261007-54's "tokens count attackers of whoever is targeted".** Read as the token subsystem keeping its cap per
    target rather than for the player alone (3.9); today's subsystem counts attackers of the player only **[reported]**.
    Reading 25 is amended to match.
82. **D-20261007-55 beside D-20261007-54.** The marker is content presence on the paired set, and choosing a finisher on
    the player must still know that the victim is the player. This spec reads D-20261007-54 as covering rules (who
    breaks, how long Staggered lasts, who may dodge out) and D-20261007-55 as content selection, where "the player" is a
    key the finisher rows carry (Q24); it adds no is-player branch to any rule. The owner may prefer the rows keyed by
    the victim's reaction profile instead, which would take the identity out of the key too.
83. **D-20261007-57's combo-step column.** The stress test's column list names it "previous step" or with the retired
    word for a run of linked attacks **[reported]**; this spec writes "combo step" (4.16), as reading 27 already does.
84. **D-20261007-57's lethal column.** The vocabulary has a lethal column; on counter rows this spec leaves it "any" and
    gives it no use beyond D-20261007-07, since the kill sequence lives on the paired content (4.6). A later rule could
    use it; none does here.
85. **D-20261007-58's "hits" and "FX moments" lanes.** The decision names the lanes and not their contents. This spec
    reads "hits" as the hit window inside Active and the hits the attack may land (`MaxHitCount`), and "FX moments" as
    the impact feedback per outcome and its moment, from the attack's impact block (5.11).
86. **D-20261007-56's evidence is model-written.** The stress test counts "21 attack assets in the attack folder plus 9
    defense-proof attack assets" and "2 defense configurations" on `origin/main`, and says the brief it was given said
    29 attacks and 1 defense configuration; 7.1 counts 29 attacks (11 light, 4 heavy, 5 directional, 9 test) and 2
    defense configurations. The attack count differs by one and was not reconciled in this pass; both figures are
    carried as **[reported]**. The stress test also says its migration estimates "are judgments, not measurements" and
    that it did not open ComboCreator's editor source for its "reusable" claim; the survey says its sub-survey file:line
    citations are "pointers to check, not proof". All three hedges stand wherever this spec cites them.
87. **D-20261010-01's "lane preferred".** Read as the ordering of 3.3's fallback step 1, which reading 29 had proposed;
    now decided. Where no row names a lane, lane is "any" and the step is by direction and swing alone.
88. **D-20261010-02 and `UWeaponData`.** The decision names the kit's parts and does not say whether the weapon's physical
    data stays its own asset. This spec keeps `UWeaponData` as a sub-asset the kit references (4.3), as a proposed shape
    with its reasons; merging it into the kit is the other reading.
89. **D-20261010-03's "Staggered time" on the body.** The decision lists Staggered time among what the body decides. This
    spec reads that as the character's own Staggered time of D-20261005-88 (the player's 0.6 s), on the reaction profile;
    an attack's Staggered time stays with the attack and its default with the combat rules [D-20261005-08, D-20261007-53].
90. **D-20261010-04 at the seam.** The decision fixes what our tables decide and what the content may decide. How the
    seam tells the content which direction and swing were chosen, so that it picks an alternate inside them and reports
    the start position the fit must meet, is proposed in 4.12; a content asset whose alternates span two of our
    directions is flagged by a proposed validation warning (4.7).
91. **The stage placement of the 7 and 10 October decisions** is this spec's call (section 9): the kit rename, the rules
    split, the defense and paired tables and the player fields land in 9.3 with the data; the evaluator and its trace in
    9.1; the timeline and the simulate-press panel in 9.2; the paired lookup by key in play in 9.8.
