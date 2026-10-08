# Thin wrapper so PowerShell users can run the local act CI without remembering
# the bash invocation. Needs bash on PATH (Git for Windows provides it), plus
# Docker and nektos/act. See docs/LOCAL_CI.md.
#
#   scripts\ci-local.ps1                       run the default test jobs
#   scripts\ci-local.ps1 -j unit-tests.yml:portable-tests
param([Parameter(ValueFromRemainingArguments = $true)] [string[]] $PassThru)

$script = Join-Path $PSScriptRoot "ci-local.sh"
& bash $script @PassThru
exit $LASTEXITCODE
