$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dir = Join-Path $root 'model/실바르/포이즌 슬라임'
$mob = Get-Content -Raw -LiteralPath (Join-Path $dir '포이즌 슬라임_몹.yml')
$skills = Get-Content -Raw -LiteralPath (Join-Path $dir '포이즌 슬라임_스킬.yml')
$model = Get-Content -Raw -LiteralPath (Join-Path $dir 'rpg_poison_slime_cube.bbmodel') | ConvertFrom-Json
$walk = @($model.animations | Where-Object name -eq 'walk')
if ($walk.Count -ne 1 -or $walk[0].length -ne 1 -or $walk[0].loop -ne 'loop') { throw 'Walk animation contract changed' }
$frames = @($walk[0].animators.effects.keyframes | Where-Object channel -eq 'timeline')
if ($frames.Count -ne 1 -or $frames[0].time -ne 0.75 -or $frames[0].data_points[0].script -ne 'mm:PoisonSlime_TryShootMucus') { throw 'Landing callback must run once at 0.75 seconds' }
if ($mob -notmatch '(?m)^  Type: ZOMBIE\r?$' -or $mob -match '~onTimer') { throw 'Invalid base or independent firing timer' }
if ($skills -match 'chance=0\.5(?!\})|Cooldown:|@target\b') { throw 'Unsynchronized firing or overridden hit target' }
$shots = @($skills -split '\r?\n' | Where-Object { $_ -match '^\s+- projectile\{' })
if ($shots.Count -ne 1 -or $shots[0] -notmatch '@PlayersInRadius\{r=15;limit=3;sort=NEAREST\}$') { throw 'Expected one projectile mechanic capped at three distinct nearby players' }
$definitions = @([regex]::Matches($skills,'(?m)^(PoisonSlime_\w+):') | ForEach-Object { $_.Groups[1].Value })
foreach ($ref in [regex]::Matches($skills,'(?:s|onTick|onHit)=(PoisonSlime_\w+)')) {
    if ($ref.Groups[1].Value -notin $definitions) { throw "Missing skill: $($ref.Groups[1].Value)" }
}
foreach ($pair in @(
    @('포이즌 슬라임_몹.yml','bukkit/plugins/MythicMobs/mobs/포이즌 슬라임_몹.yml'),
    @('포이즌 슬라임_스킬.yml','bukkit/plugins/MythicMobs/skills/포이즌 슬라임_스킬.yml'),
    @('rpg_poison_slime_cube.bbmodel','bukkit/plugins/ModelEngine/blueprints/rpg_poison_slime_cube.bbmodel')
)) {
    if ((Get-FileHash -LiteralPath (Join-Path $dir $pair[0])).Hash -ne (Get-FileHash -LiteralPath (Join-Path $root $pair[1])).Hash) { throw "Runtime mismatch: $($pair[0])" }
}
'PASS: landing callback, target cap, inherited hit targets, skill references, runtime hashes (static checks only).'
if (-not $skills.Contains('skill{s=PoisonSlime_ShootMucus} @self ?chance{chance=0.5}')) { throw 'Expected one 50% roll before the volley' }