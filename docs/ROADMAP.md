# KatanaCombat Roadmap

Current implementation status and planned features for the combat system.

---

## Current Status (As of 2025-01-29)

### v3.0.0 - Unified Combat System Complete

| Feature | Status | Notes |
|---------|--------|-------|
| Input System | Complete | Timestamped queue with press/release matching |
| Action Queue | Complete | FIFO execution with snap/responsive/immediate modes |
| Phase Management | Complete | Event-driven transitions (Windup→Active→Recovery→None) |
| Combo System | Complete | Light→Light, Light→Heavy, Heavy branching |
| Hold Mechanics | Complete | Light (ease slowdown), Heavy (charge loop) |
| Directional Follow-ups | Complete | Context-aware input, consumption tracking |
| Graceful Fallback Chain | Complete | 5-tier system that NEVER breaks combat |
| Blending | Complete | Universal crossfade with per-attack blend times |
| Debug Visualization | Complete | CVar-based (Combat.Debug.* commands) |
| Montage Utilities | Complete | 27 functions |
| Editor Tools | Complete | Custom AttackData panel with validation |
| Context System | Complete | GameplayTag resolution with cycle detection |
| **Character Hierarchy** | **Complete** | BaseCombatCharacter → PlayerCharacter/EnemyCharacter |
| **Modular Settings** | **Complete** | TargetingSettings, MotionWarpingSettings data assets |
| **Unified Motion Warping** | **Complete** | SetupAttackWarp() + AnimNotifyState_CombatWarp |
| **Team System** | **Complete** | ITeamMemberInterface for friend/foe detection |
| **Hit Reaction System** | **Complete** | Directional reactions, i-frames, stun, data-driven settings |
| **Death System** | **Complete** | Directional death animations, ragdoll transition, bIsDead flag |

### Architecture Consolidation (v3.0.0)

- **V1 Removed**: Single unified `CombatComponent` (no more V1/V2 distinction)
- **Character Hierarchy**: `ABaseCombatCharacter` → `APlayerCharacter` / `AEnemyCharacter`
- **Modular Settings**: Three-tier configuration (Override → CombatSettings → Fallback)
- **AnimNotifyState_CombatWarp**: Auto-selects translation+rotation vs rotation-only

### Robustness Features

- Circular references → falls back to default attack (not nullptr)
- Missing tags → falls back to normal combo chain
- Missing default attacks → emergency fallback repeats current attack
- Validation in BeginPlay catches configuration errors early
- On-screen warnings in editor for missing defaults

### Performance

- Timer-based easing (60Hz), not tick-based
- Event-driven queue processing (at checkpoints only)
- Minimal tick overhead

### Test Coverage

- 14 test suites, 126 tests, all passing
- Comprehensive coverage: State transitions, input buffering, hold mechanics, parry detection, attack execution, phases vs windows, targeting, weapon, hit reactions, damage application, death system, integration, debug visualization, memory safety
- See `Source/KatanaCombatTest/README.md` for full test documentation

---

## Phase 2: AttackData Designer QoL (HIGH PRIORITY)

**Estimated Effort**: 6-8 hours

### Tasks

1. **Visual Tag Preview Widget**
   - Show active tags, context, combo chain at a glance in details panel
   - Visual representation of attack connections

2. **Comprehensive Tooltips**
   - Add detailed tooltips to 22+ properties with examples
   - Include expected value ranges and common configurations

3. **Improved Validation**
   - Visual indicators for circular references
   - Missing reference warnings
   - Tag consistency checks with suggestions

4. **Test Combo Chain Button**
   - Simulate resolution without playing to verify fallbacks
   - Show resolution path (Priority 1→2→3→4)

5. **Property Organization**
   - Categorize into subcategories
   - Hide irrelevant fields with EditCondition
   - Better grouping of related properties

---

## Phase 6: Parry & Evade Systems

### Parry System

- [ ] Parry detection (check enemy's `AnimNotifyState_ParryWindow`)
- [ ] Successful parry → counter window on attacker
- [ ] Counter damage multiplier (1.5x default)
- [ ] Parry feedback (VFX, SFX, camera)
- [ ] Posture recovery on successful parry

### Evade System

- [ ] Dodge with i-frames
- [ ] Directional dodge support (8-way)
- [ ] Stamina cost integration
- [ ] Recovery frames after dodge
- [ ] Cancel into dodge from attacks (window-based)

### Counter Window System

- [ ] Counter window state on parried enemies
- [ ] Increased damage during counter window
- [ ] Special counter attacks (optional)
- [ ] Visual indicator for counter opportunity

---

## Phase 7: Posture Integration

### Core Posture Mechanics

- [ ] Posture damage on block (`AttackData->PostureDamage`)
- [ ] State-based regeneration:
  - Attacking: 50/s (fastest, rewards aggression)
  - Not blocking: 30/s
  - Idle: 20/s
- [ ] Guard break at 0 posture
- [ ] Guard break stun with vulnerability window (2s default)
- [ ] Recovery to 50% posture after guard break

### Guard Break System

- [ ] Guard break animation
- [ ] Vulnerability state during stun
- [ ] Finisher attack availability
- [ ] Camera/slow-mo effects
- [ ] UI indicators

---

## Phase 8+: Polish

### Hit Stop & Hitstun

- [ ] Frame freeze on hit impact (DMC-style)
- [ ] Configurable hitstun duration per attack
- [ ] Hit reactions based on attack type
- [ ] Knockback/knockdown support

### Root Motion

- [ ] Root motion support for attack movement
- [ ] Motion warping refinements
- [ ] Chase attack improvements

### AI Integration

- [ ] AI behavior tree tasks for attacks
- [ ] AI blocking/parrying logic
- [ ] AI combo execution
- [ ] Difficulty scaling

### Advanced Combos

- [ ] Launcher attacks
- [ ] Air combos
- [ ] Ground bounces
- [ ] Wall splat

### UI/UX

- [ ] Posture bars
- [ ] Combo counter
- [ ] Damage numbers
- [ ] Input display (training mode)

---

## Future Features (Long-Term)

### Systems

- **Finisher System**: Guard-broken enemies vulnerable to cinematic finishers
- **Special Attacks**: Resource-based (stamina/meter) powerful moves
- **Weapon Switching**: Multiple weapon types with unique movesets
- **Aerial Combat**: Launchers, air combos, ground bounces
- **Cancels & Links**: Frame-tight canceling into special moves
- **Just-Frame Mechanics**: Timed inputs for enhanced effects
- **Guard Impact**: Parry-like clash system (Soulcalibur style)
- **Revenge Mode**: Low health power-up state

### Technical

- **Multiplayer**: Server-authoritative combat, lag compensation
- **Animation Compression**: Optimize montage memory usage
- **Camera System**: Dynamic camera angles during attacks
- **VFX/SFX Integration**: Niagara particles, audio one-shots
- **Training Mode**: Frame data display, combo recorder

---

## Inspirations & Goals

### Design Goals

1. **Feel First**: Combat should feel responsive, impactful, skill-rewarding
2. **Technical Depth**: High skill ceiling with advanced techniques
3. **Readable Systems**: Designers can understand and modify without code
4. **Reusability**: Components work on any character (player, enemy, boss)
5. **Data-Driven**: Attacks defined in assets, not hardcoded

### Game Inspirations

| Game | Inspiration |
|------|-------------|
| Ghost of Tsushima | Stance system, precise timing, cinematic feel |
| Sekiro | Posture system, perfect parries, guard breaks |
| Devil May Cry | Snappy cancels, long combo chains, style system |
| Sifu | Responsive attack strings, hold-and-release mechanics |
| God of War | Heavy/light attack paradigm, directional attacks |
| For Honor | Guard stance system (future) |
| Monster Hunter | Weighty heavy attacks, commitment to animations |

---

## Version History

See [CHANGELOG.md](CHANGELOG.md) for detailed change history.
