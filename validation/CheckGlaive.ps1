# Run: pwsh -File validation/CheckGlaive.ps1
# Static animation/skill contracts only; does not run Minecraft or ModelEngine.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$modelDir = Join-Path $root 'model/바르칸/오크 킹'
$model = Get-Content -LiteralPath (Join-Path $modelDir 'orc_king.bbmodel') -Raw -Encoding UTF8 | ConvertFrom-Json
$skills = Get-Content -LiteralPath (Join-Path $modelDir '오크 킹_스킬.yml') -Raw -Encoding UTF8
$mob = Get-Content -LiteralPath (Join-Path $modelDir '오크 킹_몹.yml') -Raw -Encoding UTF8
function Check($condition, $message) { if (-not $condition) { throw $message } }
function Block($name) {
    $match = [regex]::Match($skills, "(?ms)^${name}:\r?\n.*?(?=^[A-Za-z][A-Za-z0-9_]*:|\z)")
    Check $match.Success "Missing skill: $name"
    $match.Value
}
function Anim($name) {
    $found = @($model.animations | Where-Object name -eq $name)
    Check ($found.Count -eq 1) "Missing/duplicate animation: $name"
    $found[0]
}
function Frame($animation, $bone, $channel, $last) {
    $frames = @($animation.animators.$bone.keyframes | Where-Object channel -eq $channel | Sort-Object time)
    Check ($frames.Count -gt 0) "Missing channel: $bone/$channel"
    if ($last) { $frames[-1] } else { $frames[0] }
}
function Seam($left, $right, $wrapRotation) {
    foreach ($bone in $left.animators.PSObject.Properties.Name) {
        foreach ($channel in ($left.animators.$bone.keyframes.channel | Select-Object -Unique)) {
            $a = (Frame $left $bone $channel $true).data_points[0]
            $b = (Frame $right $bone $channel $false).data_points[0]
            foreach ($axis in 'x','y','z') {
                $delta = [double]$a.$axis - [double]$b.$axis
                if ($wrapRotation -and $channel -eq 'rotation') { $delta -= 360 * [math]::Round($delta / 360) }
                Check ([math]::Abs($delta) -lt 0.00001) "Broken seam: $($left.name) -> $($right.name), $bone/$channel/$axis"
            }
        }
    }
}
$start = Anim 'glaive_start'; $loop = Anim 'glaive'; $smash = Anim 'smash'
Check ($start.loop -eq 'once' -and $loop.loop -eq 'loop' -and $smash.loop -eq 'once') 'Wrong animation loop modes'
Seam $start $loop $false
Seam $loop $loop $true
Seam $loop $smash $false
foreach ($animation in @($start,$loop,$smash)) {
    foreach ($bone in $animation.animators.PSObject.Properties.Value) {
        foreach ($key in $bone.keyframes) {
            Check ($key.time -ge 0 -and $key.time -le $animation.length + 0.00001) "Key outside $($animation.name)"
        }
    }
}
$weapon = @($loop.animators.PSObject.Properties.Value | Where-Object name -eq 'weapon')[0]
$spin = @($weapon.keyframes | Where-Object channel -eq 'rotation' | Sort-Object time)
Check ($spin.Count -ge 2) 'Missing weapon spin'
$angles = foreach ($key in $spin) { [double]$key.data_points[0].x }
Check ([math]::Abs([math]::Abs($angles[-1] - $angles[0]) - 360) -lt 0.00001) 'Spin must complete one turn'
for ($i = 1; $i -lt $spin.Count; $i++) {
    $speed = ($angles[$i] - $angles[$i-1]) / ($spin[$i].time - $spin[$i-1].time)
    Check ([math]::Abs($speed - ($angles[-1] - $angles[0]) / $loop.length) -lt 0.00001) 'Uneven spin speed'
}
Check ($mob -match 'skill\{s=OK_GlaiveTick\} @self ~onTimer:1') 'Missing one-tick dispatcher'
Check ((Block 'OK_GlaiveTick') -match 'var=caster.count;value=6') 'Dispatcher must stop on death/unlock'
Check ((Block 'OK_GlaiveSmash') -match 'value="<target.uuid>";type=STRING\} @target') 'Must capture the player, not self'
Check ((Block 'OK_GlaiveSmash') -notmatch '(?m)^\s+- (delay|skill\{s=OK_GlaiveHit)') 'Old fixed-time impact path returned'
Check ((Block 'OK_GlaiveBegin') -match 's=glaive;.*loop=LOOP') 'Airborne chase must loop'
Check ((Block 'OK_GlaiveSmash') -match 's=glaive_start;.*loop=HOLD') 'Takeoff must hold until the next state is registered'
$handoff = Block 'OK_GlaiveBegin'
Check ($handoff.IndexOf('s=glaive;') -lt $handoff.IndexOf('s=glaive_start;r=true')) 'Next state must exist before removing takeoff'
$arrival = Block 'OK_SmashBegin'
Check ($handoff -notmatch 'setAI|setspeed|goto\{|look\{') 'Transition tick must not start movement'
$chase = Block 'OK_GlaiveChase'
Check ($chase -match 'ok_glaive_age;value=>=1' -and $chase -match 'setAI\{ai=true\}.*ok_glaive_age;value=1' -and $chase -match 'setspeed\{speed=2;type=WALK\}.*ok_glaive_age;value=1') 'Chase must enable movement on the next tick only'
Check ($arrival -match 'clearpath' -and $arrival -match 'spin\{auraName=OK_SmashFacing;d=40;v=0\}' -and $arrival.IndexOf('look{') -lt $arrival.IndexOf('spin{')) 'Landing must capture and hold its final facing'
Check ((Block 'OK_GlaiveClear') -match 'auraremove\{aura=OK_SmashFacing\}' -and (Block 'OK_SmashTick') -notmatch 'look\{|goto\{') 'Landing cancellation must release facing without chasing'
$high = Block 'OK_Router_High'
Check ((Block 'OK_Init') -match 'ok_last_special;value=0' -and (Block 'OK_Summon') -match 'ok_last_special;value=5' -and (Block 'OK_GlaiveSmash') -match 'ok_last_special;value=6') 'Special history must record successful entry'
Check ($high -match 'OK_GlaiveSmash 1,OK_Summon 3.*ok_special_choice;value=6' -and $high -match 'OK_GlaiveSmash 3,OK_Summon 1.*ok_special_choice;value=5') 'Repeat-special weights must favor the other option'
Check ((Block 'OK_Router_Mid') -match 'skill\{s=OK_GlaiveSmash\}' -and $high -match 'skill\{s=OK_Summon\}.*targetnotwithin\{d=24\}') 'Single-option distance bands must remain available'
Check ($arrival -match 'ok_glaive_phase;value=1' -and $arrival -match 'TargetConditions:' -and $arrival -match 'distance\{d=0to3\}' -and $arrival -match 'lineofsight true') 'Landing must require chase phase and reachable melee range'
Check ($arrival -match 's=smash;.*merge=true;li=2') 'Blend must not delay the landing timeline'
$hit = Block 'OK_GlaiveHit'
Check ($hit -match 'ok_glaive_phase;value=2' -and $hit -match 'ok_glaive_age;value=7') 'Impact must be gated to one landing tick'
Check (([regex]::Matches($hit, 'damage\{')).Count -eq 1) 'Landing must have one damage mechanic'
Check ($hit -match 'damage\{a=15;ignoreArmor=false\}' -and $hit -match 'LivingInCone\{a=180;r=4;') 'Existing damage/cone changed'
$smashTick = Block 'OK_SmashTick'
$cue = [int][regex]::Match($smashTick, 'OK_SmashCue.*ok_glaive_age;value=(\d+)').Groups[1].Value
$impact = [int][regex]::Match($smashTick, 'OK_GlaiveHit.*ok_glaive_age;value=(\d+)').Groups[1].Value
Check ($impact - $cue -eq 2 -and $impact -eq [math]::Ceiling($smash.markers[1].time * 20)) 'Cue/impact no longer matches smash marker'
$startState = [regex]::Match((Block 'OK_GlaiveSmash'), 'meg:state\{[^\r\n]*s=glaive_start;[^\r\n]+').Value
$speedMatch = [regex]::Match($startState, 'speed=([\d.]+)')
$startSpeed = if ($speedMatch.Success) { [double]::Parse($speedMatch.Groups[1].Value, [cultureinfo]::InvariantCulture) } else { 1.0 }
Check ($startSpeed -gt 0) 'Invalid takeoff speed'
$startTicks = [math]::Round($start.length * 20 / $startSpeed)
Check ((Block 'OK_GlaiveTick') -match "ok_glaive_age;value=>=$startTicks\}") 'Takeoff playback speed and handoff timing disagree'
Check ($startTicks -eq 16 -and $handoff -match 's=glaive;.*merge=true;li=1') 'Takeoff must finish in 16 ticks with no extra blend delay'
Check ($smashTick -match "ok_glaive_age;value=>=$([math]::Round($smash.length * 20))\}") 'Recovery duration mismatch'
Check ((Block 'OK_GlaiveTick') -match 'OK_GlaiveFinish.*ok_glaive_valid;value=0' -and (Block 'OK_GlaiveTick') -match 'OK_GlaiveFinish.*value=>=200') 'Missing lost-target/timeout cleanup'
foreach ($entry in 'OK_Init','OK_Death','OK_GlaiveFinish') { Check ((Block $entry) -match 'skill\{s=OK_GlaiveClear\}') "Missing cleanup in $entry" }
foreach ($name in 'glaive_start','glaive','smash') { Check ((Block 'OK_GlaiveClear') -match "s=$name;r=true") "Missing animation removal: $name" }
Check ((Block 'OK_GlaiveFinish') -match 'skill\{s=OK_Unlock\}') 'Missing movement unlock'
# Shared spear callbacks: both launch variants must stop steering at the same age.
foreach ($launch in 'OK_ThrowRelease','OK_RunnerRelease') {
    Check ((Block $launch) -match 'onStart=OK_SpearStart;onTick=OK_SpearFlight;onHit=OK_SpearHit' -and (Block $launch) -match ';i=1;') "Wrong spear callback/tick interval: $launch"
}
$flight = Block 'OK_SpearFlight'
Check ((Block 'OK_SpearStart') -match 'var=skill.ok_spear_age;type=INTEGER;value=0') 'Spear age must reset per launch'
Check ($flight -match 'OK_SpearGuide.*var=skill.ok_spear_age;value=1to5' -and $flight -match 'trait=GRAVITY;action=SET;value=0.012.*ok_spear_age;value=5') 'Guidance cutoff and ballistic transition must agree at tick 5'
Check ((Block 'OK_SpearHit') -match 'ok_spear_age;type=INTEGER;value=5.*ok_spear_age;value=0to4') 'Piercing hit must disable subsequent steering'
Check (([regex]::Matches($skills, 'setProjectileDirection\{')).Count -eq 1 -and (Block 'OK_SpearGuide') -match 'setProjectileDirection\{') 'Unexpected steering path bypasses cutoff'
'PASS: animation seams/spin, distance-gated landing, single-impact timing, cancellation, spear cutoff.'
'Not validated: plugin execution, navigation or in-game appearance.'
$archer = Get-Content -LiteralPath (Join-Path $root 'model/바르칸/오크 궁수/오크 궁수_스킬.yml') -Raw -Encoding UTF8
Check ((Block 'OK_SummonSpawn') -notmatch 'copyThreatTable=true' -and ([regex]::Matches((Block 'OK_SummonSpawn'), 'copyThreatTable=false')).Count -eq 2) 'Summoned archers must own independent threat tables'
Check ((Block 'OK_SummonSpawn') -match 'ok_summon_target;value="<target.uuid>".*@target' -and (Block 'OK_ArcherSpawned') -match 'var=target.ok_summon_target;value="<caster.var.ok_summon_target>"') 'Summon target UUID must pass from king to each child before sudo'
Check ($archer -match 'skill\{s=OrcArcher_KingTarget\} @UUID\{u=<caster.var.ok_summon_target>\}' -and $archer -match '(?s)OrcArcher_KingTarget:.*entitytype\{t=PLAYER\} true.*health\{h=>0\} true.*threat\{amount=1\}.*settarget') 'Ready archer must seed its own threat with a valid captured player'
'PASS: independent summoned-archer threat and guarded target handoff.'
