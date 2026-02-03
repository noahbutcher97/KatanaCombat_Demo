# 🔧 Self-Hosted Runner Fix - Action Required

## Problem Summary

Your self-hosted runner is **online and listening** but **not picking up jobs** because it's missing the required `ue5` label.

### What's Happening

```
✅ Runner Status: Online (Listening for Jobs)
❌ Workflow Status: Waiting for a runner to pick up this job... → TIMEOUT
```

**Why?** The workflow file specifies:
```yaml
runs-on: [self-hosted, Windows, ue5]
```

But your runner only has:
```
Labels: self-hosted, Windows
```

The missing `ue5` label means your runner doesn't match the job requirements.

---

## ✅ The Fix (5 minutes)

### Step 1: Get a Removal Token

1. Go to: https://github.com/noahbutcher97/KatanaCombat_Demo/settings/actions/runners
2. Click on your runner name
3. Click **"Remove"** button in the top right
4. Copy the removal token shown in the command

### Step 2: Reconfigure Your Runner

Open PowerShell **as Administrator** on your runner machine:

```powershell
# Navigate to your runner directory
cd D:\actions-runner

# Stop the service
.\svc.cmd stop
.\svc.cmd uninstall

# Remove the old configuration
.\config.cmd remove --token YOUR_REMOVAL_TOKEN_FROM_STEP1
```

### Step 3: Get a New Registration Token

1. Go back to: https://github.com/noahbutcher97/KatanaCombat_Demo/settings/actions/runners
2. Click **"New self-hosted runner"**
3. Select **"Windows"**
4. Copy the token from the configuration command shown

### Step 4: Register with Correct Labels

```powershell
# Configure with ALL required labels (notice the --labels flag!)
.\config.cmd --url https://github.com/noahbutcher97/KatanaCombat_Demo --token YOUR_NEW_TOKEN_FROM_STEP3 --labels self-hosted,Windows,ue5

# Reinstall as service
.\svc.cmd install
.\svc.cmd start

# Verify it's running
.\svc.cmd status
```

### Step 5: Verify Labels

1. Go to: https://github.com/noahbutcher97/KatanaCombat_Demo/settings/actions/runners
2. Click on your runner name
3. Confirm you see these labels:
   - `self-hosted` ✅
   - `Windows` ✅
   - `ue5` ✅

---

## 🧪 Test It

1. Go to: https://github.com/noahbutcher97/KatanaCombat_Demo/actions/workflows/ue5-ci.yml
2. Click **"Run workflow"**
3. Select branch: `main`
4. Runner type: `self-hosted`
5. Click **"Run workflow"**

**Expected Result:** Your runner should pick up the job within seconds!

---

## 📋 Command Summary (Copy-Paste Ready)

```powershell
# All commands in one block - update YOUR_TOKENS
cd D:\actions-runner
.\svc.cmd stop
.\svc.cmd uninstall
.\config.cmd remove --token YOUR_REMOVAL_TOKEN
.\config.cmd --url https://github.com/noahbutcher97/KatanaCombat_Demo --token YOUR_NEW_TOKEN --labels self-hosted,Windows,ue5
.\svc.cmd install
.\svc.cmd start
.\svc.cmd status
```

---

## ❓ Why Does This Happen?

When you configure a GitHub Actions runner **without** the `--labels` parameter:

```powershell
# ❌ This gives you only default labels
.\config.cmd --url https://... --token ...
```

GitHub automatically assigns:
- `self-hosted` (always added)
- `Windows` (based on OS detection)

But **custom labels** like `ue5` must be **explicitly specified**:

```powershell
# ✅ This gives you ALL required labels
.\config.cmd --url https://... --token ... --labels self-hosted,Windows,ue5
```

---

## 🎯 Alternative: Simplify the Workflow (Optional)

If you don't need the `ue5` label distinction (e.g., you only have one Windows runner), you can modify the workflow instead:

**File:** `.github/workflows/ue5-ci.yml` (line 195)

**Change:**
```yaml
runs-on: [self-hosted, Windows, ue5]
```

**To:**
```yaml
runs-on: [self-hosted, Windows]
```

This makes the workflow less strict about labels. However, the recommended approach is to **configure the runner correctly** as shown above.

---

## 📚 More Information

- **Setup Guide:** `.github/SETUP_GUIDE.md` - Complete runner setup instructions
- **Troubleshooting:** `.github/workflows/README.md` - Detailed troubleshooting section
- **Quick Reference:** `.github/QUICK_REFERENCE.md` - Quick fixes for common issues

---

**Questions?** Check the troubleshooting sections in the documentation or open an issue.
