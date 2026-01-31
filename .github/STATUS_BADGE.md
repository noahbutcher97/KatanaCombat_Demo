# GitHub Actions Status Badge

Add this badge to your project README to show the CI/CD pipeline status.

## Badge Markdown

Copy and paste this into your `docs/README.md`:

```markdown
[![UE5 CI](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg)](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml)
```

## Badge HTML

For more control over styling:

```html
<a href="https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml">
  <img src="https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg" alt="UE5 CI Status">
</a>
```

## Badge with Branch

To show status for a specific branch:

```markdown
[![UE5 CI](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg?branch=main)](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml)
```

## Multiple Badges

Show status for different branches:

```markdown
**Main Branch:**
[![UE5 CI - Main](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg?branch=main)](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml)

**Develop Branch:**
[![UE5 CI - Develop](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg?branch=develop)](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml)
```

## Custom Badge Styles

GitHub Actions badges support these styles:

### Default Style
```markdown
![UE5 CI](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg)
```

### Flat Style
```markdown
![UE5 CI](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg?style=flat)
```

### For-the-badge Style
```markdown
![UE5 CI](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg?style=for-the-badge)
```

## Example README Section

Here's a complete example section you can add to your README:

```markdown
## Build Status

| Branch | Status | Coverage |
|--------|--------|----------|
| Main | [![UE5 CI](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg?branch=main)](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml) | Coming Soon |
| Develop | [![UE5 CI](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg?branch=develop)](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml) | Coming Soon |

### CI/CD Pipeline

The project uses GitHub Actions for continuous integration and deployment:

- ✅ **Automated Build**: Win64 Development Editor configuration
- ✅ **Static Analysis**: clang-tidy for code quality
- ✅ **Unit Tests**: Unreal Engine Automation Framework
- ✅ **Editor Validation**: Headless asset verification
- ✅ **Artifacts**: Build logs, test results, and binaries

For more information, see the [CI/CD Documentation](.github/workflows/README.md).
```

## Badge Colors

Badges automatically change color based on status:

- 🟢 **Green (Passing)**: All checks passed
- 🔴 **Red (Failing)**: Build or tests failed
- 🟡 **Yellow (Running)**: Pipeline is currently running
- ⚪ **Gray (No Status)**: No recent runs

## Shields.io Alternatives

For more customization, use [shields.io](https://shields.io/):

```markdown
![Build](https://img.shields.io/github/actions/workflow/status/noahbutcher97/KatanaCombat_Demo/ue5-ci.yml?label=UE5%20Build&style=flat-square)
```

Custom shields with additional info:

```markdown
![UE Version](https://img.shields.io/badge/Unreal%20Engine-5.6-blueviolet?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Win64-blue?style=flat-square)
![Tests](https://img.shields.io/badge/Tests-47%20Passing-brightgreen?style=flat-square)
```

## Adding to Project

To add the badge to the main project README:

1. Open `docs/README.md`
2. Add the badge markdown near the top of the file (typically after the title)
3. Commit and push the changes

Example placement:

```markdown
# KatanaCombat

[![UE5 CI](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg)](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml)
![UE Version](https://img.shields.io/badge/Unreal%20Engine-5.6-blueviolet)
![Platform](https://img.shields.io/badge/Platform-Win64-blue)

**Ghost of Tsushima-inspired melee combat system for Unreal Engine 5.6**

... rest of README ...
```

## Troubleshooting

### Badge Shows "No Status"

**Cause**: Workflow hasn't run yet or workflow file name is incorrect.

**Solution**:
1. Trigger the workflow manually from the Actions tab
2. Verify the workflow file name matches: `ue5-ci.yml`

### Badge Shows Wrong Branch

**Cause**: Badge is pointing to a different branch.

**Solution**: Update the branch parameter in the badge URL:
```markdown
![UE5 CI](https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml/badge.svg?branch=main)
```

### Badge Not Updating

**Cause**: GitHub's CDN cache may be serving an old version.

**Solution**: 
1. Clear your browser cache
2. Add a cache-busting parameter: `?cache=false`
3. Wait a few minutes for CDN to update

---

*For more information, see the [GitHub Actions Documentation](https://docs.github.com/en/actions)*
