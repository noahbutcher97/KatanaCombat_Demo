# After-edit hook - Documentation update reminders
# Triggers when significant files are edited

param(
    [string]$FilePath = $env:FILE_PATH,
    [string]$ChangeType = $env:CHANGE_TYPE  # "new", "modified", "deleted"
)

# Check if disabled
if (Test-Path ".claude/.skip-after-edit-enabled") { exit 0 }

if (-not $FilePath) { exit 0 }

# Normalize path
$FilePath = $FilePath -replace '/', '\'

# Track what needs updating
$docUpdates = @()
$reminders = @()

# Component files → Update ARCHITECTURE.md and API_REFERENCE.md
if ($FilePath -match 'CombatComponent.*\.(h|cpp)') {
    $docUpdates += @{
        file = "docs/ARCHITECTURE.md"
        section = "Component Architecture section"
        reason = "CombatComponent interface or behavior changed"
    }
    $docUpdates += @{
        file = "docs/API_REFERENCE.md"
        section = "CombatComponent API section"
        reason = "Public API may have changed"
    }
    $reminders += "Consider updating CLAUDE.md if major changes"
}

# AttackData files → Update ATTACK_CREATION.md and ARCHITECTURE_QUICK.md
if ($FilePath -match 'AttackData.*\.(h|cpp)') {
    $docUpdates += @{
        file = "docs/ATTACK_CREATION.md"
        section = "Attack property descriptions"
        reason = "AttackData structure changed"
    }

    if ($FilePath -match '\.h$') {
        $docUpdates += @{
            file = "docs/ARCHITECTURE_QUICK.md"
            section = "Default values table"
            reason = "New properties may need default values documented"
        }
    }
}

# AnimNotify files → Update PHASE_SYSTEM_MIGRATION.md
if ($FilePath -match 'AnimNotify.*\.(h|cpp)') {
    $docUpdates += @{
        file = "docs/PHASE_SYSTEM_MIGRATION.md"
        section = "Notify setup instructions"
        reason = "AnimNotify interface or behavior changed"
    }
}

# MontageUtilityLibrary → Update API_REFERENCE.md
if ($FilePath -match 'MontageUtilityLibrary.*\.(h|cpp)') {
    $docUpdates += @{
        file = "docs/API_REFERENCE.md"
        section = "MontageUtilityLibrary functions"
        reason = "Utility functions added or changed"
    }
    $reminders += "Update function count in CLAUDE.md if functions added/removed"
}

# CombatTypes.h → Multiple docs need updates
if ($FilePath -match 'CombatTypes\.h') {
    $docUpdates += @{
        file = "docs/ARCHITECTURE.md"
        section = "Enums and Structs section"
        reason = "Core type definitions changed"
    }
    $docUpdates += @{
        file = "docs/API_REFERENCE.md"
        section = "Type Definitions"
        reason = "New types or delegates added"
    }
    $reminders += "⚠️ CRITICAL: CombatTypes.h changes affect entire system"
}

# Test files → Update test README
if ($FilePath -match 'Test\.cpp$') {
    $docUpdates += @{
        file = "Source/KatanaCombatTest/README.md"
        section = "Test coverage list"
        reason = "Test files added or modified"
    }
}

# CLAUDE.md itself → Update Recent Changes section
if ($FilePath -match 'CLAUDE\.md') {
    $reminders += "Remember to date changes (YYYY-MM-DD format)"
}

# Major component additions → Update SYSTEM_PROMPT.md
if ($ChangeType -eq "new" -and $FilePath -match 'Component.*\.h$') {
    $docUpdates += @{
        file = "docs/SYSTEM_PROMPT.md"
        section = "Component Architecture"
        reason = "New component added to system"
    }
    $reminders += "Update component count in CLAUDE.md"
}

# Output reminders if any
if ($docUpdates.Count -gt 0 -or $reminders.Count -gt 0) {
    $output = "<system-reminder>`n"
    $output += "📝 Documentation Update Check`n`n"

    $output += "**Edited File**: $($FilePath -replace '.*\\Source\\', 'Source\')`n`n"

    if ($docUpdates.Count -gt 0) {
        $output += "📚 **Documentation to Review**:`n`n"
        foreach ($doc in $docUpdates) {
            $output += "  📄 **$($doc.file)**`n"
            $output += "     Section: $($doc.section)`n"
            $output += "     Reason: $($doc.reason)`n`n"
        }
    }

    if ($reminders.Count -gt 0) {
        $output += "💡 **Additional Reminders**:`n`n"
        foreach ($reminder in $reminders) {
            $output += "  - $reminder`n"
        }
        $output += "`n"
    }

    $output += "🔧 **Quick Actions**:`n"
    $output += "  - Run: /sync-docs (check doc alignment)`n"
    $output += "  - Skip: Use '/hooks skip after-edit'`n`n"

    $output += "</system-reminder>"

    Write-Output $output
}