# Secrets Configuration Template

This file serves as a template for GitHub Actions secrets that may be needed for the KatanaCombat CI/CD pipeline.

**⚠️ NEVER commit actual secret values to the repository!**

This file contains placeholders only. Actual secrets should be added through:
**GitHub Repository Settings → Secrets and variables → Actions → New repository secret**

---

## Current Secrets (Not Required for Basic Pipeline)

The basic CI/CD pipeline works without any secrets if using a self-hosted runner with pre-installed Unreal Engine.

---

## Optional Secrets for Future Features

### Epic Games Authentication

Used for automatic Unreal Engine installation on GitHub-hosted runners or accessing Marketplace assets.

```
Secret Name: EPIC_GAMES_USERNAME
Description: Epic Games account email
Example Value: developer@example.com
Used In: UE auto-installation step (future enhancement)
```

```
Secret Name: EPIC_GAMES_PASSWORD
Description: Epic Games account password
Example Value: <your-secure-password>
Used In: UE auto-installation step (future enhancement)
Security Note: Use an Epic Games account dedicated for CI/CD
```

```
Secret Name: EPIC_GAMES_API_KEY
Description: Epic Games API key for marketplace access
Example Value: eg1~xxxxxxxxxxxxxxxxxxxxxxxxxxxx
Used In: Marketplace asset downloads (future enhancement)
```

---

### Code Signing

Used for signing release builds before distribution.

```
Secret Name: CODE_SIGNING_CERTIFICATE
Description: Base64-encoded code signing certificate (PFX)
Example Value: MIIKfQIBAzCCCjcGCSqGSIb3DQEHAaCCCigEggo... (truncated)
Used In: Release build signing step (future enhancement)
How to Generate:
  1. Export certificate as PFX
  2. Convert to Base64:
     $bytes = [System.IO.File]::ReadAllBytes("cert.pfx")
     $base64 = [Convert]::ToBase64String($bytes)
     Write-Host $base64
```

```
Secret Name: CODE_SIGNING_PASSWORD
Description: Password for the code signing certificate
Example Value: <certificate-password>
Used In: Release build signing step (future enhancement)
```

```
Secret Name: WINDOWS_CERTIFICATE_THUMBPRINT
Description: Thumbprint of the Windows certificate
Example Value: A1B2C3D4E5F6G7H8I9J0K1L2M3N4O5P6Q7R8S9T0
Used In: Windows Store packaging (future enhancement)
```

---

### Steam Deployment

Used for deploying builds to Steam.

```
Secret Name: STEAM_USERNAME
Description: Steam partner account username
Example Value: steamdevaccount
Used In: Steam deployment step (future enhancement)
```

```
Secret Name: STEAM_PASSWORD
Description: Steam partner account password
Example Value: <your-secure-password>
Used In: Steam deployment step (future enhancement)
Security Note: Use a dedicated CI/CD Steam account with minimal permissions
```

```
Secret Name: STEAM_APP_ID
Description: Steam application ID
Example Value: 1234567
Used In: Steam deployment configuration (future enhancement)
```

```
Secret Name: STEAM_CONFIG_VDF
Description: Base64-encoded Steam configuration file
Example Value: <base64-encoded-config>
Used In: SteamCMD deployment (future enhancement)
How to Generate:
  1. Create app_build_config.vdf
  2. Convert to Base64:
     $bytes = [System.IO.File]::ReadAllBytes("config.vdf")
     $base64 = [Convert]::ToBase64String($bytes)
     Write-Host $base64
```

---

### Epic Games Store Deployment

Used for deploying builds to Epic Games Store.

```
Secret Name: EGS_CLIENT_ID
Description: Epic Games Store client ID
Example Value: xyza7891xxxxxxxxxxxxxxxxxxxxx
Used In: EGS deployment step (future enhancement)
```

```
Secret Name: EGS_CLIENT_SECRET
Description: Epic Games Store client secret
Example Value: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
Used In: EGS deployment step (future enhancement)
```

```
Secret Name: EGS_ARTIFACT_ID
Description: Epic Games Store artifact ID
Example Value: abc123-def456-ghi789
Used In: EGS deployment configuration (future enhancement)
```

---

### Notification Services

Used for sending build status notifications.

```
Secret Name: SLACK_WEBHOOK_URL
Description: Slack webhook URL for notifications
Example Value: https://hooks.slack.com/services/T00000000/B00000000/XXXXXXXXXXXXXXXXXXXX
Used In: Slack notification step (future enhancement)
```

```
Secret Name: DISCORD_WEBHOOK_URL
Description: Discord webhook URL for notifications
Example Value: https://discord.com/api/webhooks/123456789/abcdefghijklmnopqrstuvwxyz
Used In: Discord notification step (future enhancement)
```

```
Secret Name: TEAMS_WEBHOOK_URL
Description: Microsoft Teams webhook URL for notifications
Example Value: https://outlook.office.com/webhook/xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx@...
Used In: Teams notification step (future enhancement)
```

---

### Cloud Storage

Used for storing build artifacts in cloud storage.

```
Secret Name: AWS_ACCESS_KEY_ID
Description: AWS access key for S3 storage
Example Value: AKIAIOSFODNN7EXAMPLE
Used In: Artifact storage in S3 (future enhancement)
```

```
Secret Name: AWS_SECRET_ACCESS_KEY
Description: AWS secret access key
Example Value: wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY
Used In: Artifact storage in S3 (future enhancement)
```

```
Secret Name: AWS_S3_BUCKET
Description: S3 bucket name for artifacts
Example Value: katanacombat-ci-artifacts
Used In: Artifact storage configuration (future enhancement)
```

```
Secret Name: AZURE_STORAGE_CONNECTION_STRING
Description: Azure Storage connection string
Example Value: DefaultEndpointsProtocol=https;AccountName=...;AccountKey=...;EndpointSuffix=core.windows.net
Used In: Artifact storage in Azure (future enhancement)
```

---

### Performance Monitoring

Used for tracking build performance and metrics.

```
Secret Name: DATADOG_API_KEY
Description: Datadog API key for metrics
Example Value: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
Used In: Performance monitoring (future enhancement)
```

```
Secret Name: NEW_RELIC_LICENSE_KEY
Description: New Relic license key
Example Value: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
Used In: Application performance monitoring (future enhancement)
```

---

### Static Analysis

Used for advanced static analysis tools.

```
Secret Name: SONARQUBE_TOKEN
Description: SonarQube authentication token
Example Value: sqp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
Used In: SonarQube analysis step (future enhancement)
```

```
Secret Name: COVERITY_TOKEN
Description: Coverity Scan token
Example Value: xxxxxxxxxxxxxxxxxxxx
Used In: Coverity static analysis (future enhancement)
```

---

## Security Best Practices

### Secret Management
1. **Never commit secrets to the repository**
   - Use `.gitignore` to exclude sensitive files
   - Use GitHub Secrets for all sensitive values

2. **Use dedicated accounts for CI/CD**
   - Create separate accounts for automation
   - Limit permissions to minimum required
   - Enable 2FA where possible

3. **Rotate secrets regularly**
   - Change passwords every 90 days
   - Regenerate API keys annually
   - Update certificates before expiration

4. **Audit secret access**
   - Review who has access to repository secrets
   - Monitor secret usage in workflow logs
   - Remove unused secrets

### Accessing Secrets in Workflows

```yaml
# Example: Using a secret in a workflow step
- name: Deploy to Steam
  env:
    STEAM_USER: ${{ secrets.STEAM_USERNAME }}
    STEAM_PASS: ${{ secrets.STEAM_PASSWORD }}
  run: |
    # Use the secret here
    echo "Deploying to Steam..."
    # Secrets are automatically masked in logs
```

### Secret Scoping

Secrets can be scoped at different levels:

1. **Repository Secrets**: Available to all workflows in the repository
2. **Environment Secrets**: Available only to specific environments (e.g., production)
3. **Organization Secrets**: Shared across multiple repositories

**Recommendation**: Use environment secrets for deployment credentials to add an extra layer of control.

---

## Adding Secrets to GitHub

### Via Web Interface:

1. Navigate to repository settings
2. Go to: **Settings → Secrets and variables → Actions**
3. Click **New repository secret**
4. Enter the secret name (e.g., `EPIC_GAMES_USERNAME`)
5. Paste the secret value
6. Click **Add secret**

### Via GitHub CLI:

```bash
# Set a secret using gh CLI
gh secret set SECRET_NAME -b"secret-value"

# Set a secret from a file
gh secret set SECRET_NAME < secret-file.txt

# List all secrets
gh secret list
```

---

## Testing Secret Configuration

To verify secrets are properly configured without exposing values:

```yaml
- name: Test Secret Configuration
  run: |
    # Check if secret is set (without displaying value)
    if [ -z "${{ secrets.EPIC_GAMES_USERNAME }}" ]; then
      echo "❌ EPIC_GAMES_USERNAME not configured"
    else
      echo "✅ EPIC_GAMES_USERNAME is configured"
    fi
```

---

## Documentation

For more information:
- [GitHub Secrets Documentation](https://docs.github.com/en/actions/security-guides/encrypted-secrets)
- [Security Hardening for GitHub Actions](https://docs.github.com/en/actions/security-guides/security-hardening-for-github-actions)

---

*Last Updated: 2026-01-31*
*Template Version: 1.0*
