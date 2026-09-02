$repo = (Get-Location).Path
"repo=$repo"
docker run --rm `
  -e DEVKITPRO=/opt/devkitpro `
  -e PORTLIBS_PATH=/opt/devkitpro/portlibs `
  -v "${repo}:/workspace" -w /workspace `
  devkitpro/devkita64:latest bash scripts/docker-build-nro.sh 2>&1
"BUILD_EXIT=$LASTEXITCODE"