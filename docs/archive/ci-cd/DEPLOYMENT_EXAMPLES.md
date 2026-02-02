# Example: Adding Deployment to CI/CD Pipeline

This document provides examples of how to extend the CI/CD pipeline with deployment capabilities.

## Table of Contents
- [Steam Deployment](#steam-deployment)
- [Epic Games Store Deployment](#epic-games-store-deployment)
- [Custom Server Deployment](#custom-server-deployment)
- [Itch.io Deployment](#itchio-deployment)

---

## Steam Deployment

### Prerequisites

1. **SteamCMD** installed on runner
2. Steam credentials configured as secrets
3. Steam app ID

### Workflow Addition

Add this job to `.github/workflows/ue5-ci.yml`:

```yaml
  deploy-steam:
    name: Deploy to Steam
    needs: [build-self-hosted, build-github-hosted]
    if: github.ref == 'refs/heads/main' && (needs.build-self-hosted.result == 'success' || needs.build-github-hosted.result == 'success')
    runs-on: ubuntu-latest
    
    steps:
      - name: Download Build Artifacts
        uses: actions/download-artifact@v4
        with:
          name: binaries-self-hosted
          path: ./build/
      
      - name: Setup SteamCMD
        run: |
          sudo add-apt-repository multiverse
          sudo dpkg --add-architecture i386
          sudo apt-get update
          sudo apt-get install -y lib32gcc-s1 steamcmd
      
      - name: Upload to Steam
        env:
          STEAM_USERNAME: ${{ secrets.STEAM_USERNAME }}
          STEAM_PASSWORD: ${{ secrets.STEAM_PASSWORD }}
          STEAM_APP_ID: ${{ secrets.STEAM_APP_ID }}
        run: |
          steamcmd +login $STEAM_USERNAME $STEAM_PASSWORD \
                   +app_build ./steam_build_script.vdf \
                   +quit
```

### Required Secrets

Configure in **Settings → Secrets**:
- `STEAM_USERNAME`: Your Steam account username
- `STEAM_PASSWORD`: Your Steam account password
- `STEAM_APP_ID`: Your Steam application ID

---

## Epic Games Store Deployment

### Prerequisites

1. Epic Games account with publishing rights
2. BuildPatch Tool (Epic's deployment tool)

### Workflow Addition

```yaml
  deploy-epic:
    name: Deploy to Epic Games Store
    needs: [build-self-hosted, build-github-hosted]
    if: github.ref == 'refs/heads/main'
    runs-on: windows-latest
    
    steps:
      - name: Download Build Artifacts
        uses: actions/download-artifact@v4
        with:
          name: binaries-self-hosted
          path: ./build/
      
      - name: Setup BuildPatch Tool
        shell: pwsh
        run: |
          # Download BuildPatch Tool from Epic
          Invoke-WebRequest -Uri "https://epicgames.com/buildpatchtool" -OutFile "BuildPatchTool.zip"
          Expand-Archive -Path "BuildPatchTool.zip" -DestinationPath "BuildPatchTool"
      
      - name: Upload to Epic
        shell: pwsh
        env:
          EPIC_CLIENT_ID: ${{ secrets.EPIC_CLIENT_ID }}
          EPIC_CLIENT_SECRET: ${{ secrets.EPIC_CLIENT_SECRET }}
        run: |
          .\BuildPatchTool\BuildPatchTool.exe `
            -mode=UploadBinary `
            -OrganizationId=$env:EPIC_ORG_ID `
            -ProductId=$env:EPIC_PRODUCT_ID `
            -ArtifactId=$env:EPIC_ARTIFACT_ID `
            -ClientId=$env:EPIC_CLIENT_ID `
            -ClientSecret=$env:EPIC_CLIENT_SECRET `
            -BuildRoot=./build `
            -CloudDir=./cloud `
            -BuildVersion=${{ github.run_number }}
```

### Required Secrets

- `EPIC_CLIENT_ID`: Epic API client ID
- `EPIC_CLIENT_SECRET`: Epic API client secret
- `EPIC_ORG_ID`: Organization ID
- `EPIC_PRODUCT_ID`: Product ID
- `EPIC_ARTIFACT_ID`: Artifact ID

---

## Custom Server Deployment

### Via SFTP/SCP

```yaml
  deploy-server:
    name: Deploy to Custom Server
    needs: [build-self-hosted, build-github-hosted]
    if: github.ref == 'refs/heads/main'
    runs-on: ubuntu-latest
    
    steps:
      - name: Download Build Artifacts
        uses: actions/download-artifact@v4
        with:
          name: binaries-self-hosted
          path: ./build/
      
      - name: Install SSH Key
        uses: shimataro/ssh-key-action@v2
        with:
          key: ${{ secrets.SERVER_SSH_KEY }}
          known_hosts: ${{ secrets.SERVER_KNOWN_HOSTS }}
      
      - name: Deploy via SCP
        run: |
          rsync -avz --delete ./build/ \
            ${{ secrets.SERVER_USER }}@${{ secrets.SERVER_HOST }}:/var/www/builds/latest/
      
      - name: Restart Server Service
        run: |
          ssh ${{ secrets.SERVER_USER }}@${{ secrets.SERVER_HOST }} \
            "sudo systemctl restart katanacombat-server"
```

### Required Secrets

- `SERVER_SSH_KEY`: Private SSH key for server access
- `SERVER_KNOWN_HOSTS`: Server's known_hosts entry
- `SERVER_USER`: SSH username
- `SERVER_HOST`: Server hostname or IP

### Via AWS S3

```yaml
  deploy-s3:
    name: Deploy to AWS S3
    needs: [build-self-hosted]
    if: github.ref == 'refs/heads/main'
    runs-on: ubuntu-latest
    
    steps:
      - name: Download Build Artifacts
        uses: actions/download-artifact@v4
        with:
          name: binaries-self-hosted
          path: ./build/
      
      - name: Configure AWS Credentials
        uses: aws-actions/configure-aws-credentials@v4
        with:
          aws-access-key-id: ${{ secrets.AWS_ACCESS_KEY_ID }}
          aws-secret-access-key: ${{ secrets.AWS_SECRET_ACCESS_KEY }}
          aws-region: us-east-1
      
      - name: Upload to S3
        run: |
          aws s3 sync ./build/ s3://${{ secrets.S3_BUCKET }}/builds/${{ github.sha }}/ \
            --delete
      
      - name: Update CloudFront
        run: |
          aws cloudfront create-invalidation \
            --distribution-id ${{ secrets.CLOUDFRONT_DIST_ID }} \
            --paths "/*"
```

### Required Secrets

- `AWS_ACCESS_KEY_ID`: AWS access key
- `AWS_SECRET_ACCESS_KEY`: AWS secret key
- `S3_BUCKET`: S3 bucket name
- `CLOUDFRONT_DIST_ID`: CloudFront distribution ID (optional)

---

## Itch.io Deployment

### Using Butler (Itch.io CLI)

```yaml
  deploy-itch:
    name: Deploy to Itch.io
    needs: [build-self-hosted, build-github-hosted]
    if: github.ref == 'refs/heads/main'
    runs-on: ubuntu-latest
    
    steps:
      - name: Download Build Artifacts
        uses: actions/download-artifact@v4
        with:
          name: binaries-self-hosted
          path: ./build/
      
      - name: Setup Butler
        run: |
          curl -L -o butler.zip https://broth.itch.ovh/butler/linux-amd64/LATEST/archive/default
          unzip butler.zip
          chmod +x butler
          sudo mv butler /usr/local/bin/
      
      - name: Upload to Itch.io
        env:
          BUTLER_API_KEY: ${{ secrets.BUTLER_API_KEY }}
        run: |
          butler push ./build/ \
            ${{ secrets.ITCH_USER }}/${{ secrets.ITCH_GAME }}:windows-x64 \
            --userversion ${{ github.run_number }}
```

### Required Secrets

- `BUTLER_API_KEY`: Itch.io API key
- `ITCH_USER`: Your Itch.io username
- `ITCH_GAME`: Game slug on Itch.io

---

## Conditional Deployment

### Deploy Only on Tagged Releases

```yaml
  deploy-release:
    name: Deploy Release Build
    needs: [build-self-hosted]
    if: startsWith(github.ref, 'refs/tags/v')
    runs-on: ubuntu-latest
    
    steps:
      - name: Download Build Artifacts
        uses: actions/download-artifact@v4
        with:
          name: binaries-self-hosted
          path: ./build/
      
      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            ./build/**/*.exe
            ./build/**/*.dll
          draft: false
          prerelease: false
```

### Deploy to Different Environments

```yaml
  deploy:
    name: Deploy
    needs: [build-self-hosted]
    runs-on: ubuntu-latest
    
    steps:
      - name: Determine Environment
        id: env
        run: |
          if [[ "${{ github.ref }}" == "refs/heads/main" ]]; then
            echo "environment=production" >> $GITHUB_OUTPUT
          elif [[ "${{ github.ref }}" == "refs/heads/develop" ]]; then
            echo "environment=staging" >> $GITHUB_OUTPUT
          else
            echo "environment=dev" >> $GITHUB_OUTPUT
          fi
      
      - name: Deploy to Environment
        run: |
          echo "Deploying to ${{ steps.env.outputs.environment }}"
          # Add deployment logic here
```

---

## Notifications

### Slack Notification on Deployment

```yaml
  notify-slack:
    name: Notify Slack
    needs: [deploy-steam]
    if: always()
    runs-on: ubuntu-latest
    
    steps:
      - name: Send Slack Notification
        uses: slackapi/slack-github-action@v1
        with:
          payload: |
            {
              "text": "Deployment ${{ needs.deploy-steam.result }}",
              "blocks": [
                {
                  "type": "section",
                  "text": {
                    "type": "mrkdwn",
                    "text": "*KatanaCombat Deployment*\nStatus: ${{ needs.deploy-steam.result }}\nVersion: ${{ github.run_number }}"
                  }
                }
              ]
            }
        env:
          SLACK_WEBHOOK_URL: ${{ secrets.SLACK_WEBHOOK_URL }}
```

### Discord Notification

```yaml
  notify-discord:
    name: Notify Discord
    needs: [deploy-steam]
    if: always()
    runs-on: ubuntu-latest
    
    steps:
      - name: Send Discord Notification
        uses: sarisia/actions-status-discord@v1
        with:
          webhook: ${{ secrets.DISCORD_WEBHOOK }}
          status: ${{ needs.deploy-steam.result }}
          title: "KatanaCombat Deployment"
          description: "Build ${{ github.run_number }} deployed to Steam"
```

---

## Security Considerations

### Secret Management Best Practices

1. **Never commit secrets** to repository
2. **Use GitHub Secrets** for all sensitive data
3. **Rotate credentials** regularly
4. **Limit secret access** to necessary workflows only
5. **Use environment-specific secrets** for staging/production

### Recommended Secrets Structure

```
Production:
  - STEAM_USERNAME_PROD
  - STEAM_PASSWORD_PROD
  - AWS_ACCESS_KEY_PROD

Staging:
  - STEAM_USERNAME_STAGING
  - STEAM_PASSWORD_STAGING
  - AWS_ACCESS_KEY_STAGING
```

---

## Testing Deployment

### Dry-Run Mode

Add a dry-run flag to test without actually deploying:

```yaml
- name: Deploy (Dry Run)
  if: github.event_name == 'pull_request'
  run: |
    echo "Would deploy to: ${{ steps.env.outputs.environment }}"
    echo "Files to deploy:"
    ls -lh ./build/
```

### Manual Approval

Require manual approval for production deployments:

```yaml
  deploy-production:
    name: Deploy to Production
    needs: [build-self-hosted]
    if: github.ref == 'refs/heads/main'
    runs-on: ubuntu-latest
    environment:
      name: production
      url: https://katanacombat.example.com
    
    steps:
      # Manual approval required before these steps run
      - name: Deploy
        run: |
          # Deployment steps
```

Configure environment protection rules in **Settings → Environments**.

---

## Troubleshooting

### Deployment Fails: "Authentication Error"

**Cause**: Invalid or expired credentials

**Fix**:
1. Verify secrets are set correctly
2. Test credentials manually
3. Rotate credentials if needed
4. Update secrets in GitHub

### Deployment Succeeds but Build Not Updated

**Cause**: Cache or CDN not invalidated

**Fix**:
1. Add cache invalidation step
2. Use versioned URLs
3. Check CDN configuration

### Partial Upload

**Cause**: Network interruption or timeout

**Fix**:
1. Increase timeout values
2. Implement retry logic
3. Use chunked uploads for large files

---

## Additional Resources

- [GitHub Actions Deployment Documentation](https://docs.github.com/en/actions/deployment)
- [Steam Partner Documentation](https://partner.steamgames.com/)
- [Epic Developer Portal](https://dev.epicgames.com/)
- [Itch.io Butler Documentation](https://itch.io/docs/butler/)

---

**Note**: These are examples. Adapt them to your specific deployment requirements and security policies.
