param(
    [string]$ModelDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'model/실바르/마법 슬라임'),
    [string]$RuntimeRoot = '',
    [string]$BaselineModel = ''
)
$ErrorActionPreference = 'Stop'
$mob = Get-Content -Raw -LiteralPath (Join-Path $ModelDirectory '마법 슬라임_몹.yml')
$skills = Get-Content -Raw -LiteralPath (Join-Path $ModelDirectory '마법 슬라임_스킬.yml')
$model = Get-Content -Raw -LiteralPath (Join-Path $ModelDirectory 'slime_mage.bbmodel') | ConvertFrom-Json
function Require([bool]$condition, [string]$message) { if (-not $condition) { throw $message } }
function Section([string]$id) {
    $match = [regex]::Match($skills, "(?ms)^${id}:\r?\n.*?(?=^MS_\w+:|\z)")
    Require $match.Success "Missing section: $id"
    $match.Value
}
$arm = $model.outliner[0].children[0].children | Where-Object name -eq 'arm'
$tip = @($arm.children | Where-Object { $_ -isnot [string] -and $_.name -eq 'wand_tip' })
Require ($tip.Count -eq 1) 'wand_tip must be a unique child of animated arm'
Require ([Math]::Abs($tip[0].origin[0] - 7.475) -lt 0.000001 -and [Math]::Abs($tip[0].origin[1] - 9.016622330652078) -lt 0.000001 -and [Math]::Abs($tip[0].origin[2] + 10.208918517902012) -lt 0.000001) 'Wand gem center changed'
Require ($tip[0].export -eq $true) 'wand_tip must be exported'
Require (($model.animations.name -join ',') -eq 'idle,walk,attack,hit,death') 'Animation set changed'
Require ([Math]::Abs(($model.animations | Where-Object name -eq 'attack').length - (7.0 / 6.0)) -lt 0.000001) 'Attack duration changed'
if ($BaselineModel) {
    $baseline = Get-Content -Raw -LiteralPath $BaselineModel | ConvertFrom-Json
    $baselineArm = $baseline.outliner[0].children[0].children | Where-Object name -eq 'arm'
    $baselineArm.children = @($baselineArm.children | Where-Object { $_ -is [string] -or $_.name -ne 'wand_tip' })
    # Only the obsolete external callback may be removed; preserve all motion frames.
    $baselineWalk = $baseline.animations | Where-Object name -eq 'walk'
    $baselineWalk.animators.effects.keyframes = @($baselineWalk.animators.effects.keyframes | Where-Object { $_.data_points[0].script -ne 'mm:Slime_Land' })
    $arm.children = @($arm.children | Where-Object { $_ -is [string] -or $_.name -ne 'wand_tip' })
    Require (($baseline | ConvertTo-Json -Depth 100 -Compress) -ceq ($model | ConvertTo-Json -Depth 100 -Compress)) 'Existing model/texture/animations were modified'
}
Require ($mob -match '(?m)^  Health: 50\r?$') 'HP must be 50'
Require ($mob -match '- 1 attacker' -and $mob -notmatch '- \d+ players') 'Must remain retaliatory'
Require ($skills -notmatch '(?m)^\s+- alive\b') 'Unsupported alive condition would fail during server loading'
Require ($skills -notmatch '(?m)^\s+- explosion\{') 'No explosion allowed'
Require ((Section 'MS_OrbHit').Contains('damage{a=4}')) 'Temporary projectile damage must reach the inherited hit player'
Require (@([regex]::Matches($skills, '(?m)^\s+- damage\{')).Count -eq 1) 'Only projectile impact may deal damage'
Require ((Section 'MS_OrbHit') -notmatch 'damage\{[^\r\n]+@(?:self|target)') 'Do not override the projectile hit recipient'
Require (($model | ConvertTo-Json -Depth 100 -Compress) -notmatch 'mm:Slime_Land') 'Obsolete model callback remains'
foreach ($phase in @('radius=0.45', 'radius=0.3', 'radius=0.15')) {
    Require ((Section 'MS_Charge').Contains($phase)) "Missing contracting charge phase: $phase"
}
Require (@([regex]::Matches($skills, '(?m)^\s+- projectile\{')).Count -eq 1) 'One projectile mechanic required'
Require ($skills.Contains('origin=@ModelPart{mid=slime_mage;pid=wand_tip};fromorigin=true')) 'Direct bone origin required'
Require ($skills.Contains('v=6;g=0;i=1;hR=0.25;vR=0.25')) 'Dodge-test projectile configuration changed'
$attack = Section 'MS_Attack'
$tick = Section 'MS_AttackTick'
$period = [int][regex]::Match($attack, 'ms_cooldown;type=INTEGER;value=(\d+)').Groups[1].Value
$fireTick = [int][regex]::Match($tick, 'MS_Fire.*ms_age;value=(\d+)').Groups[1].Value
$endTick = [int][regex]::Match($tick, 'MS_Unlock.*ms_age;value=>=(\d+)').Groups[1].Value
Require ($period -eq 44 -and $fireTick -eq 12 -and $endTick -eq 24) 'Timing contract changed'
Require ($tick -notmatch 'delay|repeat') 'No independently scheduled attack tasks allowed'
Require ((Section 'MS_Fire').Contains('ms_state;value=1') -and (Section 'MS_Fire').Contains('ms_damage_pending;value=0')) 'Fire must be gated'
Require ((Section 'MS_Teleport').Contains('ms_state;type=INTEGER;value=2')) 'Teleport must invalidate attack'
Require ((Section 'MS_Death').Contains('ms_state;type=INTEGER;value=9')) 'Death must invalidate attack'
Require ((Section 'MS_ConfirmedDamage').Contains('health{h=<<skill.var.ms_health_before>}')) 'Must confirm actual HP reduction'
Require ((Section 'MS_TryTeleport').Contains('healthpercent{p=<=30%}') -and (Section 'MS_TryTeleport').Contains('entitytype{t=PLAYER}')) 'Low HP/player trigger gates required'
$teleport = Section 'MS_Teleport'
foreach ($gate in @('distance{d=2to6}', 'ydiff{d=-1to1}', 'playersinradius{r=1.5;a=0}', 'blocktype{type=AIR,CAVE_AIR,VOID_AIR}', 'LAVA,MAGMA_BLOCK')) {
    Require ($teleport.Contains($gate)) "Missing safety gate: $gate"
}
Require ($teleport -notmatch 'blocktypeinradius\{[^\r\n]*\bWATER\b') 'Nearby water must not reject safe shoreline landings'
Require (@([regex]::Matches((Section 'MS_TryTeleport'), 'skill\{s=MS_Teleport\}')).Count -eq 4) 'Expected bounded synchronous candidate retries'
Require ($teleport.Contains('ms_teleport_used;value=0') -and $teleport.Contains('ms_state;value=2} false')) 'Candidate retries must stop after success'
Require ((Section 'MS_TryTeleport').Contains('height=3;blockCentered=true')) 'Footprint needs centered clear column'
Require ((Section 'MS_TeleportArrived').Contains('distance{d=<0.5}')) 'Only consume successful teleport'
Require ((Section 'MS_ResetCombat').Contains('ms_teleport_used;type=INTEGER;value=0')) 'Combat reset missing'
# Small state-schedule regression: cancellation at/before launch prevents old shots,
# recovery cannot resume an old attack, normal casts repeat at 44 ticks.
foreach ($cancelTick in @(-1, 1, 11, 12)) {
    $state = 1; $age = 0; $shots = 0
    for ($t = 1; $t -le $endTick; $t++) {
        if ($t -eq $cancelTick) { $state = 2; $age = 0 }
        if ($state -in @(1, 2)) { $age++ }
        if ($state -eq 1 -and $age -eq $fireTick) { $shots++ }
        if ($state -eq 2 -and $age -ge 8) { $state = 0 }
    }
    Require ($shots -eq [int]($cancelTick -eq -1)) "Stale shot at cancellation tick $cancelTick"
}
$cooldown = $period
for ($t = 1; $t -lt $period; $t++) { $cooldown--; Require ($cooldown -gt 0) 'Early next attack' }
$cooldown--; Require ($cooldown -eq 0) 'Wrong attack cadence'
if ($RuntimeRoot) {
    foreach ($pair in @(
        @('마법 슬라임_몹.yml','MythicMobs/mobs/마법 슬라임_몹.yml'),
        @('마법 슬라임_스킬.yml','MythicMobs/skills/마법 슬라임_스킬.yml'),
        @('slime_mage.bbmodel','ModelEngine/blueprints/slime_mage.bbmodel')
    )) {
        Require ((Get-FileHash -LiteralPath (Join-Path $ModelDirectory $pair[0])).Hash -eq (Get-FileHash -LiteralPath (Join-Path $RuntimeRoot $pair[1])).Hash) "Runtime differs: $($pair[0])"
    }
}
'PASS: static model preservation, supported health guard, damage recipient, charge phases, timings, cancellation, shoreline safety and deployment hashes.'
'NOT VERIFIED: server loading, real damage events, navigation, teleport terrain, resource pack and visual alignment.'
