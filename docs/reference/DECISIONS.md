# Decision log

The owner's active decisions about KatanaCombat, from 29 September to 4 October 2026, in plain language.

- **Source.** Each entry was checked against the raw session transcript. Quoted words are the owner's own, copied as typed.
  Where an answer was a pick from a multiple-choice form, the entry says which option was picked.
- **IDs.** The `D-…` IDs refer to the project's local session notes. They are kept here only so entries can be traced.
- **Dates** are the day the owner decided, in US Eastern time.
- **Status** says whether the decision is built, and where.
- **The reasoning behind the defense decisions** is in the [design pillars](DESIGN_PILLARS.md). The facts they rest on are
  in the [October 2026 defense and feel findings](../audits/2026-10-defense-and-feel-findings.md).

## Standing rules for how work is done

| Date | Decision | Why | Status | ID |
|---|---|---|---|---|
| 2026-09-29 | No AI attribution in any commit message or public document. | "make sure you never leave ai attribution in any commit message or publicly facing document" | Active | D-20260929-03 |
| 2026-09-29 | Reply to every automated review thread on a pull request (Codex, Copilot), then resolve it. | Asked on several PRs, for example "yes please reply to all threads and resolve them all". | Active | D-20260929-04 |
| 2026-10-01 | Audits before a merge use ordinary review agents, not the billed multi-agent workflow tool. | "i dont know that i want to pay for the workflow based review yet can we just do an adversarial audit and analysis?" | Active | D-20261001-03 |
| 2026-10-02 | Tests never pin values that are meant to be configurable. Fixtures set their own values; shipped assets get invariant checks only. | "we should never be pinning tests to values that are meant to be configurable so that should be resolved so we can tuine assets" | Active. Existing pins are counted in the findings. | D-20261002-11 |
| 2026-10-04 | No internal planning, phasing or sequencing labels in the product or its history: source, test names, comments, tooltips, log text, content asset names, commit messages, branch names and PR titles. Describe the thing instead. Process documents keep their labels, and merged history is not rewritten. | Such names are, in the owner's words, "impossible to understand even by me". | Active. In `AGENTS.md` and `CLAUDE.md` since PR #135. | D-20261004-17 |
| 2026-10-04 | Keep detailed, dated session logs with registers of decisions, findings and open questions, outside version control. | "we should be keeping more detailed session logs that chronicle our discussions and decisions" | Active | D-20261004-10 |
| 2026-10-04 | Pose every decision or open question that waits on the owner as a multiple-choice form: a short description of what it refers to, then options A to D. | "each decision waiting on me should be posed to me as a multiple choice form with a-d reasonable alternatives and a brief description of what the question refers to. this should be a workspace standard for handling these types of things (decisions and open questions)" | Active. Added to `AGENTS.md` and `CLAUDE.md` in this change. | D-20261004-44 |

## Game design and defense

| Date | Decision | Why | Status | ID |
|---|---|---|---|---|
| 2026-09-29 | Remove the instant counter-kill mode. The parry, counter and finisher chain becomes the only counter model. | Part of a dead-code review. | Done (PR #130) | D-20260929-05 |
| 2026-10-02 | Check critically whether the defense setup is friendly to designers, and run a usability pass. | "make sure you are critically thinking about whether the implementation of the defense stuff is the most usable designer friendly approach" | Done. It led to the field traces, the tooltips and the defense data redesign. | D-20261002-12 |
| 2026-10-03 | Defense data should not be a separate asset that duplicates other configs. It should fold into existing assets "or just be a basic block and parry animation selector". | "the overall defense configuration itself is poorly made and diverges and duplicates alot of the logic from the other config files" | Active. Carried out by the defense data model decision (D-20261004-20). | D-20261003-05 |
| 2026-10-03 | New features: strafing while blocking, with the camera deciding the target; a target lock for duels; and an engagement mode. | Asked for by the owner. | Queued behind the defense data redesign | D-20261003-09 |
| 2026-10-04 | Engagement covers both parts: the player's combat mode first, then enemy coordination. | Picked "Both, player mode first". | Queued | D-20261004-01 |
| 2026-10-04 | Guard aim is "direction, with magnetism". The camera aims the guard, a bounded pull draws it toward the best threat, and a hard lock replaces the camera direction. | Picked over "which enemy you guard", which had been recommended. | Design queued | D-20261004-02 |
| 2026-10-04 | In the counter window after a parry, the player is free once the parry animation ends and the attacker stays held. A press in the window counters. | Picked "Player free, attacker held". | In review (PR #137) | D-20261004-03 |
| 2026-10-04 | Parry readability: distinct contact feedback, a telegraph, an input buffer and a contact grace, plus a parry animation distinct from the block-hit animation. | "the parry animation should be different than the normal block hit animation" | Not built. Part of the defense data redesign. | D-20261004-06 |
| 2026-10-04 | Parry assists are real designer settings, not debug switches. | The owner wanted them to be "actual configurable settings". | Not built. Part of the defense data redesign. | D-20261004-07 |
| 2026-10-04 | Block and parry facing is a code-driven, rate-limited turn owned by the combat code. Motion warping stays for attacks and the two-character animations. | Picked "Code-driven turn". | Not built | D-20261004-21 |
| 2026-10-04 | Guard reach. A perfect parry works from any direction, with directional deflect animations; the code-driven turn covers them until those clips exist. A held block covers the front arc. Block and parry data are keyed by direction from the start, so full-circle directional blocks later need only content and one setting. | Picked after asking "what are the different implications of these different choices and are there additional options we havent considered?" | Not built | D-20261004-23 |
| 2026-10-04 | Retire the old 70° block cone (`CanBlockHit`, `CanBlockAttackFrom`, `BlockFacingConeHalfAngle` and the check in `ApplyDamage_Implementation`), so only the defense resolver answers whether a guard blocks. `BeginBlock` is refused during two-character animations. | Picked "Retire it now". | In review (PR #136) | D-20261004-24 |
| 2026-10-04 | Decide the shape of the parry chain, and with it the game's identity, by writing design pillars first. | "this is a critical decision as it decides the soul of the game" | Done: the [design pillars](DESIGN_PILLARS.md) | D-20261004-25 |
| 2026-10-04 | The design pillars: mixed fights of small crowds and duels; a free camera with an optional lock; a whirlwind in crowds and a precise duelist in duels; mixed answers on skill, readability, pacing and finishers. | On skill: "i am uncertain". On readability: "I am uncertain". On finishers: "if that is possible". | Active, partial. Skill and readability are uncertain. | D-20261004-26 |
| 2026-10-04 | Defense's role is set per enemy type. Basic enemies are counter-led; duelists and elites get a lighter parry-centric kit; brutes and guard-breakers come later. | Picked "Per enemy type", after a cost and content analysis of four options. | Active | D-20261004-27 |
| 2026-10-04 | The parry chain differs by enemy type: an instant parry-counter for basic enemies, and a parry followed by a contextual press for duelists and elites. | "I lean towards c [both, by enemy type] but i dont want to cheapen the feel of the parry + counter included branch so i want to ensure we have animations for every direction. maybe i can buy some packs that have such animations?" | **Leaning, conditional** on counter animations for every direction | D-20261004-28 |
| 2026-10-04 | Before buying any counter or finisher packs, check the content already owned. | Picked "Check owned content first". The owner widened the search: "they can also be normal two handed and one handed sword we cant afford to be picky". | Done. The owned counter pairs are front-only. | D-20261004-29 |
| 2026-10-04 | Parry-counters from the side or behind use an authored turn-deflect that flows into a front counter pair, with motion warping hiding the leftover angle. | "a lot of those animations refer to finishers from behind (ie attacker is behind the victim) and not ones where the victim attacks the attacker and the attacker parries behind them" | Not built. Content depends on what is owned or bought. | D-20261004-30 |
| 2026-10-04 | Prototype directional parry-counters with owned clips before buying anything. | Picked "Prototype with owned clips". | Done. A throwaway rendered prototype, not merged. | D-20261004-31 |
| 2026-10-04 | In the counter window, the player's aim decides. A press aimed at the parried enemy, within a broad cone that prefers that enemy, does the follow-up. A press aimed elsewhere ends the window and becomes a normal attack. The state a parry causes is called "Staggered". This applies to the parry-then-follow-up chain only. | "I think [aim decides] but it should prefer the parried enemy or at least relax aim to a broader cone preferring parried enemies. We should call the state that being parryed causes "Staggered" for simplicity." | Not built | D-20261004-35 |
| 2026-10-04 | Off-front parries get longer windows and retimed turns where needed. Recorded as two designer settings per direction: a parry window lead (the window opens earlier by the turn's duration and still closes at contact) and a turn play rate. | "Why dont we make parry windows longer as needed and also retime the turns as needed for the best feel?" | Not built. Values will come from a rendered sweep of turn speeds, which is running. | D-20261004-36 |
| 2026-10-04 | Breaking defenders open: keep contextual stagger, and add a simple guard break for guarding enemy types, after a number of blocked hits or by a heavy attack. No posture meter. | Picked "Contextual + guard break". | Not built. Revises the defense design's non-goal on guard-break gameplay, for guarding enemy types only. | D-20261004-43 |

## Data and configuration

| Date | Decision | Why | Status | ID |
|---|---|---|---|---|
| 2026-09-29 | From a field-by-field review of unused data: wire the finisher trigger config, knockback, charge scaling, the guard enter and exit montages, and the contact socket and bone overrides. Delete the hold cap and `StaggerPower`. | "Wire it" on each field. | Knockback wired (PR #131). Hold cap and `StaggerPower` deleted. The rest is open. Weapon reach changed (D-20260930-05). | D-20260929-07 |
| 2026-09-29 | Delete `UHitReactionData`, with a restore note in the data-asset audit. | "delete it as long as its not something that will be difficult to recreate if needed" | Done (PR #130) | D-20260929-08 |
| 2026-09-30 | Bake weapon reach from a reference rig, and delete the `WeaponReach` field. | Folded into the knockback and charge work. | Not started. `WeaponReach` is still in `WeaponData.h:204`. | D-20260930-05 |
| 2026-09-30 | Derive AI attack ranges from the baked reach. | — | Not started | D-20260930-09 |
| 2026-10-02 | Reaction variants become an array only. The single slot migrates into the array, and death reads the array too. | Accepted after the owner asked why the slot shouldn't stay as a fallback. | Not built | D-20261002-07 |
| 2026-10-02 | Log a warning when a character's defense configuration slot is empty, and point the default maps at `Lvl_ThirdPerson1`. | Picked the recommendation. | Approved, not started. `DefaultEngine.ini:2-3` still names `Lvl_ThirdPerson`, which doesn't exist. | D-20261002-10 |
| 2026-10-03 | Hit pressure, resistance by attack phase and lock duration use per-type defaults in the combat settings, with per-field overrides on each attack. | Picked "Type defaults + overrides". | Not built | D-20261003-02 |
| 2026-10-04 | Hold all of the owner's working content (tuning, trimmed reactions, new animation assets, AnimBP and blend space edits) for the defense data redesign, rather than a separate content PR. | Picked holding everything. | Active | D-20261004-09 |
| 2026-10-04 | The proof-only defense content assets get descriptive names inside the defense data redesign, which moves and replaces them anyway. | Picked over a separate rename pass. | Active | D-20261004-19 |
| 2026-10-04 | The defense data model. Each attack's feedback and parried-stagger length resolve from per-attack-type defaults with per-attack overrides. Each character's reaction profile starts from a base profile. Global rules go in the combat settings. The defense configuration asset is removed. | "Approve as proposed but perhaps these should be as overrides per attack so we can have defaults and not have to configure everything per every attack if we want to have only a few unique attacks or if we want to have a base profile to start with?" | Not built | D-20261004-20 |
| 2026-10-04 | The parry fallback is explicit in the reaction profile: each parry entry has an optional two-character animation plus the solo animations to use when it can't play. The unused fallback re-pick and "Requires Bridge Preflight" are deleted. | Picked "Explicit fallback in profile". | Not built | D-20261004-22 |
| 2026-10-04 | HeavyAttack_1 to 4 timing: an editor action reads each attack's timing from its montage notifies and writes it back to the data asset. | Picked "Editor action reads montage". | In progress | D-20261004-33 |
| 2026-10-04 | Knockback defaults: the combat settings asset holds the per-type defaults, each attack can override them, and the code values only seed a new asset. A per-weapon layer is noted for later. | "i thought it was like we have default knockback and override knockback. default knockback is set on the settings or weapon perhaps and override is set on the attack with default read from current configuration" | Active. This confirms the model that already exists. | D-20261004-38 |

## Hit reactions and knockback

| Date | Decision | Why | Status | ID |
|---|---|---|---|---|
| 2026-09-29 | Hit direction always points from the victim to the attacker. The field is renamed `DirectionToAttacker`, with a redirect. | — | Done (PR #129) | D-20260929-09 |
| 2026-09-30 | The victim-side knockback multiplier is called `KnockbackScale`. | "knockback resistance would be the wrong name for something that scales knockback by multiplying it since it would really be knockback scale" | Done (PR #131) | D-20260930-01 |
| 2026-09-30 | Knockback distance and duration are both configurable per attack. | "shouldnt knockback duration and knockback distance both be configurable per attack?" | Done (PR #131) | D-20260930-02 |
| 2026-09-30 | Four answers on the knockback and charge design. The per-attack settings include the charge curve, knockback ease-out, knockback direction mode, and charge scaling the knockback. The charge clock uses world time, so it stops when the game pauses. Soft-aim scoring uses the attack's acquisition range. Editor operations that create assets or touch maps can't be cleanly undone, so they are offered in the editor with a warning. | Picks from a form. The owner chose world time after asking about the trade-offs. | Active | D-20260930-03 |
| 2026-09-30 | Knockback moves through the procedural displacement executor: a root-motion modifier under animation root motion, or a movement root-motion source otherwise. | Picked "Procedural displacement executor". | Done (PR #131) | D-20260930-06 |
| 2026-09-30 | The stagger model, contextual stagger or a gauge, must be decided explicitly before the hit-reaction work. | "i mean the other option would be to create an actual stagger stat" | Settled by D-20261004-43 | D-20260930-11 |
| 2026-10-01 | Cancel a suspended push if it has gone stale. | Picked "Cancel if stale". | Done (PR #131) | D-20261001-04 |
| 2026-10-01 | Release a defender's push when a defense chain starts. | Picked "Release at chain start". | Done (PR #131) | D-20261001-05 |
| 2026-10-01 | The along-the-swing push direction becomes continuous. | Picked "Make it continuous now". | Done (PR #131) | D-20261001-06 |
| 2026-10-01 | The default Heavy push is 20 cm; Light stays 25 cm. | Picked "20 cm". | Active as code starting values. The owner's held settings asset uses other values. | D-20261001-07 |
| 2026-10-03 | The action lock during hit reactions applies to the current reaction behaviour now, not only to a planned layered reaction mode. | Picked against the recommendation, which was to gate it. | Not built | D-20261003-01 |
| 2026-10-03 | Inputs pressed during the hit lock are buffered and fire when it releases. | Picked "Buffer, fire at release". | Not built | D-20261003-03 |
| 2026-10-04 | Stun length belongs to the attack: a per-type default, overridable per attack. The stun field on hit-reaction entries is removed. | Picked "Attack owns it". | Not built | D-20261004-41 |
| 2026-10-04 | Hit intensity. The attack type decides whether a hit looks light or heavy. The attack's pressure against the victim's current resistance decides whether it interrupts. Hits that don't interrupt play an additive flinch. This replaces the damage-divided-by-health rule. | Picked "Type looks, pressure interrupts". | Not built | D-20261004-42 |
| 2026-10-04 | Verify knockback on slopes with a rendered capture scenario: pushes up, down and across a ramp, recorded and measured. | Picked "Rendered slope scenario". | Queued. Replaces the earlier manual-only slope check. | D-20261004-40 |

## Animation, rendering and tooling

| Date | Decision | Why | Status | ID |
|---|---|---|---|---|
| 2026-09-29 | Builds refuse to run against a stale generated copy of the AnimationAnalysis plugin, with a message an agent can act on. | Approved by the owner, who asked that it be clear and actionable for agents. | Done (PR #127) | D-20260929-02 |
| 2026-09-29 | Consolidate direction math and target gathering into shared libraries, and delete the four math libraries that had no users. | "you dont think it would be useful to debloat the targeting component and other possible consumers of this logic and create authoritative logic for math used frequently?" | Done (PRs #129 and #130) | D-20260929-06 |
| 2026-09-30 | A reusable editor-operation workflow, with every existing detail-panel button and bake operation moved onto it. | — | Not started | D-20260930-04 |
| 2026-10-02 | Integrate the capture plugins now, as shared infrastructure. | "why dont we just integrate the plugins now and then continue work because knockback can be easily tuned but i want to ensure we are progressing with the overall project" | Done (PR #133) | D-20261002-05 |
| 2026-10-02 | Port AnimationAnalysis to D3D12 in its own repository, done by a separate worker. | — | Handed off | D-20261002-13 |
| 2026-10-04 | The capture plugin gets a private GitHub repository, and the project pins it by URL and commit, the way AnimationAnalysis is pinned. | "we already have a presentation capture repo since it is our plugin (we developed it)" | Done. Keep it private. | D-20261004-04 |
| 2026-10-04 | Keep the charged-heavy fix as built. HeavyAttack_4's earlier tap-or-charge decision is a feel value to tune later. | "this is a feel thing and can be tuned later" | Done (PR #134) | D-20261004-05 |
| 2026-10-04 | The charge-loop snap fix goes in with the defense data redesign: the code honours the authored blend times through inertialization, and the AnimBP gets an Inertialization node. | Picked "All with the redesign". | Waiting on the redesign | D-20261004-32 |
| 2026-10-04 | Keep Force Root Lock on all 274 DynamicKatana animation sequences. | Picked "Keep all". | Active. Held with the owner's content. | D-20261004-34 |
| 2026-10-04 | D3D11 stays the default renderer; D3D12 is used per launch with `-dx12`. | Picked "Keep DX11, DX12 per launch". | Active | D-20261004-39 |
| 2026-10-04 | One documentation PR: the findings, this decision log, the design pillars and the `CLAUDE.md` corrections. | Picked "One docs PR now". | This change | D-20261004-37 |

## Order of work

| Date | Decision | Status | ID |
|---|---|---|---|
| 2026-09-29 | Work in this order: shared math, then deletions, then knockback, charge scaling and reach, then the finisher config, guard montages and contact overrides, then finisher authoring. One PR each, each with a green baseline. | Active as a principle. The order was revised by the entries below. | D-20260929-10 |
| 2026-09-30 | Paired-entry migration becomes its own piece of work, after knockback, charge scaling and reach. | Not started | D-20260930-07 |
| 2026-09-30 | Knockback, charge scaling, the editor-operation workflow and reach baking are separate PRs. | Knockback merged (PR #131). The rest are not started. | D-20260930-10 |
| 2026-10-02 | The hit-reaction work moves ahead of charge scaling and reach baking. | Active. It now waits on the defense data redesign. | D-20261002-08 |
| 2026-10-04 | Clear the PRs that were ready before starting new large work. | Done. PRs #131 to #135 merged. | D-20261004-08 |

## One-off actions, done

- **2026-09-29:**
  - merge PR #126 and run the baseline (D-20260929-01); moving the UEMCP work out into its own repository is still open;
  - workspace hygiene before continuing (D-20260929-11).
- **2026-09-30:** the revised knockback specs were approved (D-20260930-12) and executed with sub-agents (D-20260930-13).
- **2026-10-01:** push the knockback branch and open PR #131 (D-20261001-01), then answer and resolve its review threads
  (D-20261001-02).
- **2026-10-02:**
  - run the rendered-capture check first, starting with a D3D12 stability check on driver 617.14 (D-20261002-06);
  - record that float equality assertions in the test suite treat NaN as equal (D-20261002-09);
  - ask the automated reviewers for another pass (D-20261002-14).
- **2026-10-03:**
  - trace every defense and paired-animation field and every montage notify, and give every defense field a tooltip
    (D-20261003-06, PR #132);
  - open the tooltip PR, and fix the counter-window teardown and the charged-heavy phase order as separate PRs
    (D-20261003-07; PR #134 merged, PR #137 in review);
  - answer all automated reviewers (D-20261003-10);
  - survey another of the owner's projects for ideas (D-20261003-11).
- **2026-10-04:** reword PR #133's pushed commit messages to describe each change, then force-push with lease; the content
  was unchanged (D-20261004-18).

## Superseded or confirmed by a later decision

| Earlier | What it said | Replaced by |
|---|---|---|
| D-20260930-08 | Hit-reaction work after the knockback work | D-20261002-08 |
| D-20260930-14 | Plan changes, including making slopes a manual check only. Never answered, but shipped in PR #131. | The slope part: D-20261004-40. The other changes were never confirmed. |
| D-20261002-15, D-20261002-16 | Stun from one source; interruption by pressure. Both were proposed but never confirmed at the time. | Confirmed by D-20261004-41 and D-20261004-42 |
| D-20261002-17 | The knockback defaults' source of truth (proposed) | D-20261004-38 |
| D-20261003-04 | A separate small PR for the owner's tuning | D-20261004-09 |
| D-20261003-08, D-20261004-11 | Consolidating the defense data (read as approved but never named by the owner), and its first data model (proposed) | Confirmed and refined by D-20261004-20 |
| D-20261003-12 | Keep D3D11 as the default (recommended, never answered) | D-20261004-39 |
| D-20261004-12 | The charge-snap fix (proposed) | D-20261004-32 |
| D-20261004-13 | Block facing as a programmatic rotation (proposed) | D-20261004-21 |
| D-20261004-14 | A documentation PR (proposed) | D-20261004-37 |
| D-20261004-15 | A standalone debug parry assist | D-20261004-07 |
| D-20261004-16 | Keep the Force Root Lock resaves (proposed) | D-20261004-34 |

## Open

- **Exact rules before code.** The owner wants the rules for guard breaks, finishers, parries and counters specified first:
  what causes what, how, and why. They don't want to discover how something is calculated or arbitrated after the code is
  written.
- **Uncertain pillars:** skill and readability.
- **The parry chain by enemy type** waits on directional counter content (D-20261004-28).
- **The diagonal tie-break** in the direction classification has been unanswered since 29 September.
- **Listed as approved without an explicit owner approval in the record:**
  - adopting the other project's test-fidelity rules;
  - a test-only fix to the rendered parry, counter and finisher test.
