# Attack Creation Workflow

Complete guide to authoring attacks for the KatanaCombat system.

---

## Table of Contents

1. [Animation Requirements](#animation-requirements)
2. [Montage Setup & Sections](#montage-setup--sections)
3. [AnimNotify Placement](#animnotify-placement)
4. [AttackData Configuration](#attackdata-configuration)
5. [Combo Chains](#combo-chains)
6. [Directional Follow-ups](#directional-follow-ups)
7. [Heavy Attack Charging](#heavy-attack-charging)
8. [Motion Warping](#motion-warping)
9. [Common Patterns](#common-patterns)
10. [Testing & Tuning](#testing--tuning)

---

## Animation Requirements

### Source Animation

**Requirements**:
- Root motion enabled (for motion warping)
- Clean looping or end pose
- Clear windup/attack/recovery sections
- 30-60 FPS animation data

**Recommended Length**:
- Light attacks: 0.8s - 1.2s
- Heavy attacks: 1.2s - 2.0s
- Finishers: 2.0s - 4.0s

### Root Motion Setup

1. Open animation sequence
2. **Animation** → **Asset Details** → **Root Motion**
3. Enable `Force Root Lock` (optional, for stationary attacks)
4. Enable `Enable Root Motion` for motion warping

---

## Montage Setup & Sections

### Creating Montages

**One Montage Per Move Type** (recommended):
```
Montages/
├── AM_LightAttacks (contains sections: Light1, Light2, Light3, Light4)
├── AM_HeavyAttacks (contains sections: Heavy1, Heavy2, HeavyFinisher)
├── AM_DirectionalAttacks (contains sections: Forward, Back, Left, Right)
└── AM_Finishers (contains sections: Finisher1, Finisher2)
```

**Benefits of Section-Based Approach**:
- Fewer asset files
- Easier combo management
- Smoother blend between similar attacks
- Shared AnimNotify templates

### Section Setup

1. Open montage in Montage Editor
2. **Sections** panel → **Add Section**
3. Name section clearly (e.g., `LightCombo2`)
4. Set section range via markers
5. Repeat for each attack variant

**Example**:
```
AM_LightAttacks Montage:
┌────────┬────────┬────────┬────────┐
│ Light1 │ Light2 │ Light3 │ Light4 │
│ 0.0-   │ 1.0-   │ 2.0-   │ 3.0-   │
│ 1.0s   │ 2.0s   │ 3.0s   │ 4.2s   │
└────────┴────────┴────────┴────────┘
```

---

## AnimNotify Placement

### Required Notifies (Minimum)

Every attack needs these 5 notifies:

#### 1. Windup Phase
```
AnimNotifyState_AttackPhase
├─ Start: 0.0s
├─ End: 0.3s
└─ Phase: Windup
```

**Purpose**: Startup frames, motion warping active, **parryable**

#### 2. Active Phase
```
AnimNotifyState_AttackPhase
├─ Start: 0.3s
├─ End: 0.5s
└─ Phase: Active
```

**Purpose**: Damage-dealing frames, hit detection enabled

#### 3. Hit Detection Toggle (Enable)
```
AnimNotify_ToggleHitDetection
├─ Time: 0.3s (Active start)
└─ bEnable: true
```

#### 4. Hit Detection Toggle (Disable)
```
AnimNotify_ToggleHitDetection
├─ Time: 0.5s (Active end)
└─ bEnable: false
```

#### 5. Recovery Phase
```
AnimNotifyState_AttackPhase
├─ Start: 0.5s
├─ End: 1.0s
└─ Phase: Recovery
```

**Purpose**: End lag, combo window opens here

### Optional Notifies

#### Combo Window (for combo chains)
```
AnimNotifyState_ComboWindow
├─ Start: 0.4s (late Active)
└─ End: 0.9s (mid-to-late Recovery)
```

**Purpose**: Early combo execution ("snappy" feel)

**Placement Tips**:
- Start during late Active or early Recovery
- Extends through most of Recovery
- Shorter = tighter timing, Longer = more forgiving

#### Parry Window (for parryable attacks)
```
AnimNotifyState_ParryWindow
├─ Start: 0.15s (late Windup)
└─ End: 0.30s (end of Windup)
```

**Purpose**: Marks when attacker is vulnerable to parry

**Placement Tips**:
- Last 150-300ms of Windup
- Before Active phase begins
- Too early = impossible to parry, Too late = no telegraph

#### Hold Window (for hold-and-release attacks)
```
AnimNotifyState_HoldWindow
├─ Start: 0.7s (late Recovery)
└─ End: 0.9s (end of Recovery)
```

**Purpose**: Animation manipulation zone for directional follow-ups

**Placement Tips**:
- During Recovery, after main attack completes
- Duration = how long player can hold
- System checks if button STILL HELD when window starts

#### Cancel Window (for cancellable moves)
```
AnimNotifyState_CancelWindow
├─ Start: 0.1s (early Windup)
├─ End: 0.4s (late Windup)
└─ AllowedCancelInputs: LightAttack | Evade | Block (bitmask)
```

**Purpose**: Interrupt heavy attacks with specific inputs

---

## AnimNotify Timeline Examples

### Basic Light Attack
```
0.0s ──────────────────────────────────────────────► 1.0s
│       │              │                  │
│ Windup│    Active    │     Recovery    │
│       │  [Hit Detect]│                 │
└───────┴──────────────┴─────────────────┘
```

### Combo-able Light Attack
```
0.0s ──────────────────────────────────────────────► 1.0s
│       │              │                  │
│ Windup│    Active    │     Recovery    │
│       │  [Hit Detect]│                 │
│       │              ├─────────────┐   │
│       │              │ComboWindow  │   │
└───────┴──────────────┴─────────────┴───┘
```

### Parryable Enemy Attack
```
0.0s ──────────────────────────────────────────────► 1.0s
│       │              │                  │
│ Windup│    Active    │     Recovery    │
│ ├──┐  │  [Hit Detect]│                 │
│ │PW│  │              │                 │
└─┴──┴──┴──────────────┴─────────────────┘
PW = Parry Window
```

### Hold-and-Release Light Attack
```
0.0s ──────────────────────────────────────────────► 1.0s
│       │              │                  │
│ Windup│    Active    │     Recovery    │
│       │  [Hit Detect]│              ├──┤
│       │              │              │HW│
│       │              ├──────────────┴──┤
│       │              │ComboWindow     │
└───────┴──────────────┴────────────────┘
HW = Hold Window
```

### Cancellable Heavy Attack
```
0.0s ──────────────────────────────────────────────► 1.5s
│       │              │                     │
│ Windup│    Active    │      Recovery      │
├───────┤  [Hit Detect]│                    │
│Cancel │              │                    │
│Window │              │                    │
└───────┴──────────────┴────────────────────┘
```

---

## AttackData Configuration

### Creating AttackData Asset

1. Right-click in Content Browser → **Miscellaneous** → **Data Asset**
2. Choose `AttackData` as parent class
3. Name clearly: `DA_LightCombo2`, `DA_HeavyFinisher`, etc.

### Basic Properties

```
Attack Type: Light / Heavy / Special
Direction: None / Forward / Backward / Left / Right
AttackMontage: AM_LightAttacks
MontageSection: LightCombo2
```

**Direction Guidelines**:
- `None`: Neutral attack, faces current target
- `Forward`: Lunge/dash attack
- `Backward`: Retreat/backstep
- `Left/Right`: Side slash

### Damage Configuration

```
BaseDamage: 25.0 (Light), 50.0 (Heavy), 100.0 (Finisher)
PostureDamage: 10.0 (Light), 25.0 (Heavy), 40.0 (Charged)
HitStunDuration: 0.0 (Light), 0.5 (Heavy), 1.0 (Finisher)
CounterDamageMultiplier: 1.5 (bonus during counter window)
```

### Timing Mode

**Option 1: AnimNotify-Driven** (Recommended)
```
bUseAnimNotifyTiming: true
TimingFallbackMode: AutoCalculate
```

System reads timing from `AnimNotifyState_AttackPhase` in montage.

**Option 2: Manual Timing**
```
bUseAnimNotifyTiming: false
ManualTiming:
  WindupDuration: 0.3
  ActiveDuration: 0.2
  RecoveryDuration: 0.5
  HoldWindowStart: 0.7 (if applicable)
  HoldWindowDuration: 0.2
```

Use when:
- Prototyping without AnimNotifies
- Procedural attacks
- Runtime-generated moves

### Motion Warping

```
MotionWarpingConfig:
  bUseMotionWarping: true
  MotionWarpingTargetName: "AttackTarget"
  MinWarpDistance: 50.0
  MaxWarpDistance: 400.0
  WarpRotationSpeed: 720.0
  bWarpTranslation: true
  bRequireLineOfSight: true
```

**When to Disable**:
- Stationary attacks (no approach needed)
- AOE attacks
- Finishers (camera control instead)

### Combo Blending (Added 2025-11-11)

```
ComboBlendOutTime: 0.1 (Blend-out when FROM this attack)
ComboBlendInTime:  0.1 (Blend-in when this attack is TARGET)
```

**Purpose**: Smooth transitions between combo attacks.

**How It Works**:
- **ComboBlendOutTime**: When this attack transitions to a combo follow-up, blend out over this duration (default: 0.1s)
- **ComboBlendInTime**: When this attack is the target of a combo transition, blend in over this duration (default: 0.1s)
- Both can be tuned independently per attack
- Applies to ALL combo types: light→light, light→heavy, directional follow-ups, etc.

**Tuning Guidelines**:
```
Instant/Snappy (0.0s):           Cancel-heavy attacks, frame-tight combos
Quick (0.05s - 0.1s):            Most light attacks, responsive chains
Smooth (0.1s - 0.2s):            Standard combo transitions
Deliberate (0.2s - 0.3s):        Heavy attacks, finishers
```

**Example**:
```
Light1 → Light2 → Heavy Finisher

Light1->ComboBlendOutTime = 0.05f  (Fast exit)
Light2->ComboBlendOutTime = 0.1f   (Standard exit)
HeavyFinisher->ComboBlendInTime = 0.3f  (Slow, deliberate entry for impact)
```

---

## Combo Chains

### Linear Combo Chain

```
DA_LightCombo1
├─ NextComboAttack: DA_LightCombo2
└─ HeavyComboAttack: null

DA_LightCombo2
├─ NextComboAttack: DA_LightCombo3
└─ HeavyComboAttack: null

DA_LightCombo3
├─ NextComboAttack: DA_LightCombo4
└─ HeavyComboAttack: null

DA_LightCombo4 (Finisher)
├─ NextComboAttack: null (chain ends)
└─ HeavyComboAttack: null
```

**Result**: Light → Light → Light → Light (4-hit combo)

### Branching Combo Chain

```
DA_LightCombo1
├─ NextComboAttack: DA_LightCombo2
└─ HeavyComboAttack: DA_HeavyLauncher

DA_LightCombo2
├─ NextComboAttack: DA_LightCombo3
└─ HeavyComboAttack: DA_HeavySlam

DA_LightCombo3
├─ NextComboAttack: null
└─ HeavyComboAttack: DA_HeavyFinisher
```

**Result**:
- Light → Light → Light (3-hit light chain)
- Light → Heavy (launcher)
- Light → Light → Heavy (slam)
- Light → Light → Light → Heavy (finisher)

### Best Practices

**Combo Count Limits**:
- Light chains: 3-5 hits max
- Heavy enders: 1-2 hits
- Total combo length: 6-8 hits max (prevents infinite loops)

**Variety**:
- Mix speeds (fast → slow or slow → fast)
- Vary damage (build up to finisher)
- Different hit reactions per stage

**Balance**:
- Longer combos = more damage, more commitment
- Heavy enders = high damage, long recovery
- Allow counter opportunities (don't make airtight)

---

## Directional Follow-ups

### Setup

**1. Mark Attack as Holdable**:
```
DA_LightCombo3:
  bCanHoldAtEnd: true
  bEnforceMaxHoldTime: false (or true with MaxHoldTime: 1.5)
```

**2. Add Hold Window to Montage**:
```
AnimNotifyState_HoldWindow
├─ Start: 0.7s (late Recovery)
└─ End: 0.9s
```

**3. Configure Directional Follow-ups**:
```
DA_LightCombo3:
  DirectionalFollowUps:
    ├─ Forward: DA_DashStrike
    ├─ Backward: DA_BackStep
    ├─ Left: DA_SideSlashLeft
    └─ Right: DA_SideSlashRight
```

**4. Optional: Heavy Directional Variants**:
```
DA_LightCombo3:
  HeavyDirectionalFollowUps:
    ├─ Forward: DA_HeavyLunge
    └─ Backward: DA_HeavyBackflip
```

### Player Experience

```
Player holds Light Attack:
    ↓
Light combo executes normally
    ↓
Hold Window reached
    ↓
Animation slows/freezes
    ↓
Player releases + direction input
    ↓
System finds target in that direction
    ↓
Executes DirectionalFollowUps[Direction]
    ↓
Motion warps to new target
```

### Design Tips

**Forward Attacks**:
- Dash strikes
- Lunges
- Gap closers
- Use longer motion warp distance (600.0+)

**Backward Attacks**:
- Retreating slashes
- Backflips with slash
- Create space
- Disable motion warping (retreat, not chase)

**Left/Right Attacks**:
- Side slashes
- Circular strikes
- Position adjustments
- Medium motion warp distance (300.0)

---

## Heavy Attack Charging

### Setup

**1. Mark as Chargeable**:
```
DA_HeavyAttack:
  AttackType: Heavy
  bCanBeCharged: true
  MaxChargeTime: 2.0
  ChargeTimeScale: 0.3 (animation playback speed during charge)
  MaxChargeDamageMultiplier: 2.5
  ChargedPostureDamage: 40.0
```

**2. Add Hold Window** (charge accumulates here):
```
AnimNotifyState_HoldWindow
├─ Start: 0.2s (early in Windup)
└─ End: 2.2s (extends Windup for charging)
```

**3. Configure Charge Sections** (optional, advanced):
```cpp
ChargeLoopSection:            "ChargeLoop"     // Section that loops during hold (NAME_None = use default animation)
ChargeReleaseSection:         "Release"        // Section to play on release (NAME_None = blend to idle)
ChargeLoopBlendTime:          0.3f             // Blend time when entering charge loop
ChargeReleaseBlendTime:       0.2f             // Blend time when transitioning to release
```

**Purpose**: Use montage sections to create distinct charge loop and release animations.

**Example Montage Structure**:
```
AM_HeavyAttack:
┌────────┬────────────┬────────┐
│ Windup │ ChargeLoop │Release │
│ 0.0-   │ 0.5-       │ 2.0-   │
│ 0.5s   │ 2.0s       │ 3.0s   │
└────────┴────────────┴────────┘
         (loops)
```

**4. Configure VFX Timing** (optional):
```
Add AnimNotifies for charge VFX:
├─ 25% charge: Glow starts
├─ 50% charge: Sparks appear
├─ 75% charge: Intense glow
└─ 100% charge: Full charge flash
```

### Player Experience

```
Player holds Heavy Attack:
    ↓
Heavy attack starts
    ↓
Windup plays at 0.3x speed (ChargeTimeScale)
    ↓
Damage scales: 1.0x → 2.5x (MaxChargeDamageMultiplier)
    ↓
Player releases (or hits MaxChargeTime)
    ↓
Animation resumes at 1.0x speed
    ↓
Executes with scaled damage
```

### Design Tips

- **Short charge time** (0.5s - 1.0s): Quick power-up, tactical
- **Long charge time** (1.5s - 2.5s): Commitment, high risk/reward
- **Visual feedback**: Particle effects scale with charge %
- **Audio feedback**: Charge-up sound, crescendo at full charge
- **Tuning**: Balance with light attacks (2x charge ≈ 3-4 light hits)

---

## Motion Warping

### Basic Setup

**1. Enable in AttackData**:
```
MotionWarpingConfig:
  bUseMotionWarping: true
  MotionWarpingTargetName: "AttackTarget"
  MinWarpDistance: 50.0
  MaxWarpDistance: 400.0
```

**2. Add Motion Warping Notify to Montage**:
```
AnimNotifyState_MotionWarping
├─ Start: 0.0s (Windup start)
├─ End: 0.3s (Windup end, before Active)
├─ RootMotionModifier: Scale
├─ WarpTargetName: "AttackTarget" (matches AttackData)
└─ Settings:
    ├─ bWarpTranslation: true
    ├─ bWarpRotation: true
    └─ bIgnoreZAxis: true (don't warp vertically)
```

### Distance Tuning

**Short Range** (50.0 - 150.0):
- Quick stabs
- Stationary attacks
- Minimal approach

**Medium Range** (150.0 - 400.0):
- Standard attacks
- Typical melee range
- Balanced

**Long Range** (400.0 - 800.0):
- Dash attacks
- Gap closers
- High commitment

### Advanced: Directional Warping

**Example: Forward Dash Attack**:
```
MotionWarpingConfig:
  MaxWarpDistance: 800.0  // Can chase far
  MinWarpDistance: 200.0  // Don't use if too close
  bRequireLineOfSight: true
```

**Example: AOE Slam (No Warping)**:
```
MotionWarpingConfig:
  bUseMotionWarping: false  // Stationary
```

---

## Common Patterns

### Pattern 1: Basic 4-Hit Light Combo
```
Light1 → Light2 → Light3 → Light4
├─ Each: 25 damage
├─ Total: 100 damage
├─ Combo Windows: 0.6s each
└─ No heavy branches
```

### Pattern 2: Light Chain with Heavy Enders
```
Light1 → Light2 → Light3
  ↓       ↓        ↓
Heavy1  Heavy2  HeavyFinisher
50dmg   65dmg    100dmg
```

### Pattern 3: Hold-and-Release Directional
```
Light1 → Light2 → Light3 (Hold)
                    ↓
         ┌──────────┼──────────┐
         ↓          ↓          ↓
    DashStrike  SideSlash  BackStep
```

### Pattern 4: Charged Heavy Finisher
```
Light1 → Light2 → Light3 → Heavy(Charge)
                            ↓
                      Hold to charge
                      Release at peak
                      = 125 damage (2.5x multiplier)
```

### Pattern 5: Cancellable Heavy Startup
```
Heavy (Windup)
  ↓
Player presses Light during Cancel Window
  ↓
Cancels into Light attack
(Feint heavy → punish parry attempt)
```

---

## Testing & Tuning

### Enable Debug Visualization

```cpp
CombatComponent->bDebugDraw = true;
TargetingComponent->bDebugDraw = true;
WeaponComponent->bDebugDraw = true;
```

**Shows**:
- Combat state
- Current phase
- Active windows
- Motion warp targets
- Weapon traces
- Hit detection

### Testing Checklist

**Basic Functionality**:
- [ ] Attack animation plays
- [ ] Weapon traces appear during Active phase
- [ ] Hits register on enemy
- [ ] Animation returns to Idle

**Combo System**:
- [ ] Input during combo window → chains
- [ ] Input outside combo window → resets
- [ ] Combo count increments
- [ ] Max combo reached → stops

**Motion Warping**:
- [ ] Character rotates toward target during Windup
- [ ] Stops at MinWarpDistance
- [ ] Doesn't warp beyond MaxWarpDistance
- [ ] Smooth rotation (no snapping)

**Hold Mechanics**:
- [ ] Animation freezes during Hold Window
- [ ] Release + direction → executes follow-up
- [ ] Finds target in specified direction
- [ ] Motion warps to new target

**Parry System**:
- [ ] `AnimNotifyState_ParryWindow` on attacker's montage
- [ ] Defender blocks during window → parry success
- [ ] Defender blocks outside window → normal block
- [ ] Counter window opens after parry

### Tuning Values

**Feel Too Slow**:
- Decrease Windup duration
- Increase animation playback rate
- Tighten combo window (more responsive)

**Feel Too Fast**:
- Increase Recovery duration
- Add anticipation to Windup
- Widen combo window (more deliberate)

**Combos Too Easy**:
- Shorten combo window (0.4s instead of 0.6s)
- Add more recovery between hits
- Require precise timing

**Combos Too Hard**:
- Lengthen combo window (0.8s instead of 0.6s)
- Start combo window earlier (during Active)
- Add buffer time

**Motion Warping Too Aggressive**:
- Decrease MaxWarpDistance
- Increase MinWarpDistance
- Reduce WarpRotationSpeed

**Not Tracking Targets**:
- Increase DirectionalConeHalfAngle (90° = 180° total cone)
- Increase MaxTargetDistance
- Disable bRequireLineOfSight

---

## Advanced: Editor Tools (Optional)

### Using AttackDataTools (C++)

```cpp
// Auto-calculate timing from montage length
UAttackDataTools::AutoCalculateTiming(AttackData);

// Generate all AnimNotifies automatically
UAttackDataTools::GenerateAllNotifies(AttackData);

// Validate configuration
TArray<FText> Warnings, Errors;
bool bValid = UAttackDataTools::ValidateAttackData(AttackData, Warnings, Errors);

// Find conflicts (multiple attacks using same section)
TArray<UAttackData*> Conflicts = UAttackDataTools::FindSectionConflicts(AttackData);
```

### Custom Details Panel (Future)

Visual timeline editor showing:
- Phase layout
- Window overlaps
- Timing conflicts
- One-click notify generation

---

## Summary: Attack Creation Checklist

**1. Animation**:
- [ ] Root motion enabled
- [ ] Clean start/end poses
- [ ] Appropriate length for attack type

**2. Montage**:
- [ ] Created with section(s)
- [ ] `AnimNotifyState_AttackPhase` (Windup, Active, Recovery)
- [ ] `AnimNotify_ToggleHitDetection` (Enable/Disable)
- [ ] Optional: ComboWindow, ParryWindow, HoldWindow, CancelWindow

**3. AttackData**:
- [ ] Created and named clearly
- [ ] Montage + section assigned
- [ ] Damage values configured
- [ ] Timing mode set
- [ ] Motion warping configured

**4. Combo Chain**:
- [ ] `NextComboAttack` linked (if combo-able)
- [ ] `HeavyComboAttack` linked (if branches)
- [ ] `DirectionalFollowUps` configured (if hold-and-release)

**5. Testing**:
- [ ] Attack executes correctly
- [ ] Combos chain as expected
- [ ] Motion warping feels smooth
- [ ] Hit detection works
- [ ] Debug visualization reviewed

**6. Tuning**:
- [ ] Timing feels right
- [ ] Damage is balanced
- [ ] Motion warping distance appropriate
- [ ] Combo windows feel responsive

---

**Now go create amazing attacks!** ⚔️