<#
.SYNOPSIS
    Release check: drives Immersive Flashlight VR in a running Fallout 4 VR through devbench and checks every
    light location, gesture, restriction and menu gate it can reach without a headset.

.DESCRIPTION
    Talks to devbench's REST API (the `immersiveflashlightvr` tool, plus `console`, `menu`, `nodes`, `log`).
    Gestures fire through the tool's `gesture` action: the real gating and handler, without the zone and button.
    What needs hands or eyes is printed as a manual checklist at the end.

    Before running, in game:
    - a save loaded, the Pip-Boy worn, not in power armor, no weapon drawn, no menu open;
    - wearing headgear the headgear restriction allows (the Army Helmet works);
    - for the power armor part: a frame within -PowerArmorRadius. KabutoVR blocks the scripted exit: disable it.

    The run moves the light and saves its locations as the gestures do; it puts back the locations and on/off it
    started with. It equips the test weapon (adding one if there is none) and holsters it again.

.EXAMPLE
    pwsh tools/release-check.ps1
    pwsh tools/release-check.ps1 -SkipPowerArmor
#>
param(
    [int]$Port = 8931,
    # a ranged weapon without a modeled lamp: AssaultRifle, Fallout4.esm
    [string]$WeaponFormId = '0000463F',
    [double]$PowerArmorRadius = 1000,
    [switch]$SkipPowerArmor,
    [double]$FrameUpdateP95BudgetMs = 0.5
)

$ErrorActionPreference = 'Stop'
$api = "http://127.0.0.1:$Port/api"
$results = [System.Collections.Generic.List[object]]::new()
$script:section = ''

# ---------------------------------------------------------------- devbench calls

function Invoke-DevBench([string]$tool, [hashtable]$body) {
    $json = $body | ConvertTo-Json -Compress -Depth 5
    Invoke-RestMethod "$api/tool/$tool" -Method Post -ContentType application/json -Body $json -TimeoutSec 15
}

function Invoke-ImFl([hashtable]$body) {
    $reply = Invoke-DevBench immersiveflashlightvr $body
    if (-not $reply.ok) {
        throw "immersiveflashlightvr $($body.action): $($reply.error)"
    }
    $reply
}

function Invoke-Console([string]$command) {
    $reply = Invoke-DevBench console @{ command = $command }
    if (-not $reply.compiled) {
        throw "console '$command': $($reply.lines -join '; ')"
    }
    $reply
}

function Get-State { Invoke-ImFl @{ action = 'state' } }

function Get-Config([string]$key, [string]$section) {
    $body = @{ action = 'config'; key = $key }
    if ($section) { $body.section = $section }
    (Invoke-ImFl $body).value
}

function Set-Override([string]$key, [string]$value, [string]$section) {
    $body = @{ action = 'set'; key = $key; value = $value }
    if ($section) { $body.section = $section }
    Invoke-ImFl $body | Out-Null
}

# Poll the state until $condition holds or the timeout passes; returns the last state either way.
function Wait-State([scriptblock]$condition, [double]$timeoutSec = 5) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    do {
        $state = Get-State
        if (& $condition $state) { return $state }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    $state
}

function Get-EventHead { (Invoke-RestMethod "$api/events?since=0" -TimeoutSec 15).headSeq }

function Get-EventsSince([long]$seq) { @((Invoke-RestMethod "$api/events?since=$seq" -TimeoutSec 15).events) }

function Get-LogLineCount { (Invoke-DevBench log @{ action = 'tail'; file = 'ImmersiveFlashlightVR.log'; tail = 1 }).linesScanned }

# ---------------------------------------------------------------- results

function Start-Section([string]$name) {
    $script:section = $name
    Write-Host "`n== $name" -ForegroundColor Cyan
}

function Add-Result([string]$name, [string]$result, [string]$detail = '') {
    $results.Add([pscustomobject]@{ Section = $script:section; Check = $name; Result = $result; Detail = $detail })
    $color = @{ PASS = 'Green'; FAIL = 'Red'; WARN = 'Yellow'; SKIP = 'DarkGray' }[$result]
    Write-Host ("  {0,-4} {1}{2}" -f $result, $name, ($detail ? "  ($detail)" : '')) -ForegroundColor $color
}

function Test-That([string]$name, [bool]$pass, [string]$detail = '') {
    Add-Result $name ($pass ? 'PASS' : 'FAIL') $detail
}

function Format-Light($reply) { "on=$($reply.lightOn) location=$($reply.location)" }

# Run a light action and check where the light ends up.
function Test-Light([string]$name, [hashtable]$body, [bool]$on, [string]$location) {
    $reply = Invoke-ImFl (@{ action = 'light' } + $body)
    Test-That $name (($reply.lightOn -eq $on) -and ($reply.location -eq $location)) (Format-Light $reply)
}

# Fire a gesture and check whether it fired and where the light ends up. An inert gesture must leave the light as it was.
function Test-Gesture([string]$name, [hashtable]$body, [bool]$fired, [bool]$on, [string]$location) {
    $reply = Invoke-ImFl (@{ action = 'gesture' } + $body)
    $pass = ($reply.fired -eq $fired) -and ($reply.lightOn -eq $on) -and ($reply.location -eq $location)
    $detail = "fired=$($reply.fired) $(Format-Light $reply)$($reply.inert ? " inert: $($reply.inert)" : '')"
    Test-That $name $pass $detail
}

$locationArg = @{ '0' = 'head'; '1' = 'offhand'; '2' = 'primaryHand' }

# ---------------------------------------------------------------- preflight

Start-Section 'Preflight'
try {
    $health = Invoke-ImFl @{ action = 'health' }
} catch {
    Write-Host "devbench doesn't answer on port ${Port}: is the game running with devbench and the mod? ($_)" -ForegroundColor Red
    exit 2
}
$repoVersion = (Select-String -Path (Join-Path $PSScriptRoot '..\CMakeLists.txt') -Pattern '^set\(VERSION ([^)]+)\)').Matches[0].Groups[1].Value
Test-That 'mod version matches CMakeLists.txt' ($health.version -eq $repoVersion) "game $($health.version), repo $repoVersion"
Test-That 'gesture action present (current build loaded)' ($health.actions -contains 'gesture') ($health.actions -join ',')

$state = Get-State
$blockers = @()
if (-not $state.liveness.ready) { $blockers += 'no player 3D yet: load a save' }
if (-not $state.gating.flashlightAvailable) { $blockers += 'the Pip-Boy is not worn' }
if ($state.gating.inPowerArmor) { $blockers += 'in power armor: step out' }
if ($state.restrictions.weaponEquipped) { $blockers += 'a weapon is drawn: holster it' }
if ($state.gating.gesturesBlockedByMenu) { $blockers += "a menu is open: $($state.gating.blockingMenu)" }
if (-not $state.restrictions.headAllowed) { $blockers += 'the head is not allowed: wear a helmet the headgear restriction accepts' }
if ($blockers) {
    $blockers | ForEach-Object { Add-Result 'precondition' 'FAIL' $_ }
    exit 2
}

Invoke-ImFl @{ action = 'clear'; all = $true } | Out-Null
$snapshot = @{
    lightOn = [bool]$state.light.on
    location = Get-Config 'iFlashlightLocation'
    locationInPA = Get-Config 'iFlashlightLocationInPA'
}
$radius = @{
    head = [int](Get-Config 'iFlashlightOnHeadRadius')
    paHead = [int](Get-Config 'iFlashlightOnPAHeadRadius')
    hand = [int](Get-Config 'iFlashlightInHandRadius')
}
$fallback = (Get-Config 'bRestrictionFallback') -eq 'true'
$headgear = (Invoke-ImFl @{ action = 'restrictions' }).headgear
$helmetId = if ($headgear.worn -match '\[([0-9A-F]{8})\]$') { $Matches[1] }
$logStart = Get-LogLineCount
Add-Result 'snapshot' 'PASS' "light on=$($snapshot.lightOn), iFlashlightLocation=$($snapshot.location), InPA=$($snapshot.locationInPA), helmet=$($headgear.worn)"

$weaponAdded = $false
$weaponEquipped = $false
$inPowerArmor = $false

try {
    # ------------------------------------------------------------ light action

    Start-Section 'Light action'
    Test-Light 'head on' @{ location = 'head'; op = 'on' } $true 'OnHead'
    $s = Wait-State { param($s) $s.light.beam.radius -eq $radius.head } 2
    Test-That 'head beam radius' ($s.light.beam.radius -eq $radius.head) "$($s.light.beam.radius), config $($radius.head)"
    Test-Light 'offhand' @{ location = 'offhand' } $true 'InOffhand'
    $s = Wait-State { param($s) $s.light.beam.radius -eq $radius.hand } 2
    Test-That 'in-hand beam radius' ($s.light.beam.radius -eq $radius.hand) "$($s.light.beam.radius), config $($radius.hand)"
    Test-Light 'primary hand (no weapon)' @{ location = 'primaryHand' } $true 'InPrimaryHand'
    Test-Light 'off' @{ op = 'off' } $false 'InPrimaryHand'
    Test-Light 'toggle on' @{ op = 'toggle' } $true 'InPrimaryHand'
    Test-Light 'toggle off' @{ op = 'toggle' } $false 'InPrimaryHand'

    # ------------------------------------------------------------ gestures

    Start-Section 'Gestures (no weapon)'
    Test-Light 'start: off, on head' @{ location = 'head'; op = 'off' } $false 'OnHead'
    $eventsFrom = Get-EventHead
    Test-Gesture 'head tap: on at the head' @{ gesture = 'head' } $true $true 'OnHead'
    Test-Gesture 'head tap: off' @{ gesture = 'head' } $true $false 'OnHead'
    Test-Gesture 'head tap: on again' @{ gesture = 'head' } $true $true 'OnHead'
    Test-Gesture 'head long: to the offhand' @{ gesture = 'head'; press = 'long' } $true $true 'InOffhand'
    Test-Gesture 'head long: inert off the head' @{ gesture = 'head'; press = 'long' } $false $true 'InOffhand'
    Test-Gesture 'primaryHand tap: offhand to primary hand' @{ gesture = 'primaryHand' } $true $true 'InPrimaryHand'
    Test-Gesture 'primaryHand tap: primary hand to offhand' @{ gesture = 'primaryHand' } $true $true 'InOffhand'
    Test-Gesture 'primaryHand long: inert off the weapon' @{ gesture = 'primaryHand'; press = 'long' } $false $true 'InOffhand'
    Test-Gesture 'bodyGrab offhand: returns it (off)' @{ gesture = 'bodyGrab'; hand = 'offhand' } $true $false 'InOffhand'
    Test-Gesture 'primaryHand tap: inert from off without a weapon' @{ gesture = 'primaryHand' } $false $false 'InOffhand'
    Test-Gesture 'weaponToggle: inert without a two-handed grip' @{ gesture = 'weaponToggle' } $false $false 'InOffhand'
    Test-Gesture 'bodyGrab primary hand: grabs it (on)' @{ gesture = 'bodyGrab'; hand = 'primaryHand' } $true $true 'InPrimaryHand'
    Test-Gesture 'bodyGrab offhand: inert while the other hand holds it' @{ gesture = 'bodyGrab'; hand = 'offhand' } $false $true 'InPrimaryHand'
    Test-Gesture 'bodyGrab primary hand: returns it (off)' @{ gesture = 'bodyGrab'; hand = 'primaryHand' } $true $false 'InPrimaryHand'
    $gestureEvents = @(Get-EventsSince $eventsFrom | Where-Object topic -EQ 'immersiveflashlightvr.gesture')
    Test-That 'a gesture event per fired gesture' ($gestureEvents.Count -eq 9) "$($gestureEvents.Count) events, 9 fired"

    # ------------------------------------------------------------ session overrides

    Start-Section 'Config overrides'
    Test-Light 'head on' @{ location = 'head'; op = 'on' } $true 'OnHead'
    Set-Override 'iFlashlightOnHeadRadius' '1234'
    $s = Wait-State { param($s) $s.light.beam.radius -eq 1234 } 3
    Test-That 'override applies live' ($s.light.beam.radius -eq 1234) "radius $($s.light.beam.radius)"
    Invoke-ImFl @{ action = 'clear'; key = 'iFlashlightOnHeadRadius' } | Out-Null
    $s = Wait-State { param($s) $s.light.beam.radius -eq $radius.head } 3
    Test-That 'clear restores the file value' ($s.light.beam.radius -eq $radius.head) "radius $($s.light.beam.radius)"
    $overrides = Invoke-ImFl @{ action = 'overrides' }
    Test-That 'no overrides left' (@($overrides.overrides).Count -eq 0) ($overrides.overrides | ConvertTo-Json -Compress -Depth 4)

    # ------------------------------------------------------------ headgear restriction

    Start-Section 'Headgear restriction'
    if (-not $helmetId) {
        Add-Result 'helmet off / on' 'SKIP' "no helmet form id in '$($headgear.worn)'"
    } else {
        Test-Light 'head on' @{ location = 'head'; op = 'on' } $true 'OnHead'
        Invoke-Console "player.unequipitem $helmetId" | Out-Null
        $s = Wait-State { param($s) -not $s.restrictions.headAllowed -and $s.light.location -ne 'OnHead' } 5
        if ($fallback) {
            Test-That 'helmet off: the light moves to the offhand' ($s.light.on -and $s.light.location -eq 'InOffhand') "on=$($s.light.on) location=$($s.light.location)"
        } else {
            Test-That 'helmet off: the light turns off' (-not $s.light.on) "on=$($s.light.on) location=$($s.light.location)"
        }
        Test-Gesture 'head tap: inert without a helmet' @{ gesture = 'head' } $false $s.light.on $s.light.location
        Invoke-Console "player.equipitem $helmetId" | Out-Null
        $s = Wait-State { param($s) $s.restrictions.headAllowed -and $s.light.location -eq 'OnHead' } 5
        Test-That 'helmet on: the light returns to the head' ($s.restrictions.headAllowed -and $s.light.location -eq 'OnHead') "headAllowed=$($s.restrictions.headAllowed) location=$($s.light.location)"
    }

    # ------------------------------------------------------------ menus

    Start-Section 'Menus'
    Test-Light 'head on' @{ location = 'head'; op = 'on' } $true 'OnHead'
    Invoke-DevBench menu @{ action = 'open'; name = 'PauseMenu' } | Out-Null
    $s = Wait-State { param($s) $s.gating.gesturesBlockedByMenu } 3
    Test-That 'pause menu blocks the gestures' ($s.gating.blockingMenu -eq 'PauseMenu') "blockingMenu=$($s.gating.blockingMenu)"
    Test-That 'the light stays on behind the menu' ([bool]$s.light.on)
    $reply = Invoke-ImFl @{ action = 'gesture'; gesture = 'head' }
    Test-That 'head tap: inert behind the menu' (-not $reply.fired -and $reply.inert -like '*menu*') "inert: $($reply.inert)"
    Invoke-DevBench menu @{ action = 'close'; name = 'PauseMenu' } | Out-Null
    $s = Wait-State { param($s) -not $s.gating.gesturesBlockedByMenu } 3
    Test-That 'closing it unblocks the gestures' (-not $s.gating.gesturesBlockedByMenu)

    $reply = Invoke-ImFl @{ action = 'ui'; op = 'open' }
    Test-That 'config UI opens' ([bool]$reply.configOpen)
    $reply = Invoke-ImFl @{ action = 'ui'; op = 'close' }
    Test-That 'config UI closes' (-not $reply.configOpen)

    # ------------------------------------------------------------ weapon

    Start-Section 'Weapon mount'
    $count = (Invoke-Console "player.getitemcount $WeaponFormId").lines -join ' '
    if ($count -match '>>\s*0(\.0+)?\b') {
        Invoke-Console "player.additem $WeaponFormId 1" | Out-Null
        $weaponAdded = $true
    }
    Invoke-Console "player.equipitem $WeaponFormId" | Out-Null
    Invoke-Console 'player.cf "Actor.DrawWeapon"' | Out-Null
    $weaponEquipped = $true
    Set-Override 'iWeaponFlashlightMeshRequired' '1' 'ImFl_WeaponMount'
    $s = Wait-State { param($s) $s.restrictions.weaponEquipped -and -not $s.restrictions.weaponFlashlightAllowed } 5
    Test-That 'weapon drawn, lamp required, no lamp: not allowed' ($s.restrictions.weaponEquipped -and -not $s.restrictions.weaponFlashlightAllowed) "weaponEquipped=$($s.restrictions.weaponEquipped) allowed=$($s.restrictions.weaponFlashlightAllowed)"
    Test-Light 'offhand on' @{ location = 'offhand'; op = 'on' } $true 'InOffhand'
    Test-Gesture 'primaryHand tap: inert, the weapon cannot carry it' @{ gesture = 'primaryHand' } $false $true 'InOffhand'

    Set-Override 'iWeaponFlashlightMeshRequired' '0' 'ImFl_WeaponMount'
    $s = Wait-State { param($s) $s.restrictions.weaponFlashlightAllowed } 3
    Test-That 'lamp not required: allowed' ([bool]$s.restrictions.weaponFlashlightAllowed)
    Test-Gesture 'primaryHand tap: offhand to weapon' @{ gesture = 'primaryHand' } $true $true 'OnWeapon'
    Test-Gesture 'primaryHand long: weapon to offhand' @{ gesture = 'primaryHand'; press = 'long' } $true $true 'InOffhand'
    Test-Gesture 'primaryHand tap: back on the weapon' @{ gesture = 'primaryHand' } $true $true 'OnWeapon'
    Test-Gesture 'primaryHand tap: off on the weapon' @{ gesture = 'primaryHand' } $true $false 'OnWeapon'
    Test-Gesture 'primaryHand tap: on at the weapon from off' @{ gesture = 'primaryHand' } $true $true 'OnWeapon'
    Test-Light 'light action: primaryHand lands on the weapon' @{ location = 'primaryHand'; op = 'on' } $true 'OnWeapon'

    Set-Override 'iWeaponFlashlightMeshRequired' '1' 'ImFl_WeaponMount'
    $s = Wait-State { param($s) $s.light.location -ne 'OnWeapon' -or -not $s.light.on } 3
    if ($fallback) {
        Test-That 'lamp required again: the light leaves the weapon' ($s.light.on -and $s.light.location -ne 'OnWeapon') "on=$($s.light.on) location=$($s.light.location)"
    } else {
        Test-That 'lamp required again: the light turns off' (-not $s.light.on) "on=$($s.light.on) location=$($s.light.location)"
    }
    Invoke-ImFl @{ action = 'clear'; all = $true } | Out-Null
    Invoke-Console "player.unequipitem $WeaponFormId" | Out-Null
    $s = Wait-State { param($s) -not $s.restrictions.weaponEquipped } 5
    $weaponEquipped = [bool]$s.restrictions.weaponEquipped
    Test-That 'weapon holstered' (-not $weaponEquipped)

    # ------------------------------------------------------------ power armor

    Start-Section 'Power armor'
    $frame = if (-not $SkipPowerArmor) { (Invoke-ImFl @{ action = 'powerArmor'; radius = $PowerArmorRadius }).frame }
    if ($SkipPowerArmor) {
        Add-Result 'power armor' 'SKIP' '-SkipPowerArmor'
    } elseif (-not $frame) {
        Add-Result 'power armor' 'SKIP' "no frame within $PowerArmorRadius units"
    } else {
        Test-Light 'head on' @{ location = 'head'; op = 'on' } $true 'OnHead'
        Invoke-Console "player.cf `"Actor.SwitchToPowerArmor`" $($frame.formId)" | Out-Null
        $s = Wait-State { param($s) $s.gating.inPowerArmor } 10
        $inPowerArmor = [bool]$s.gating.inPowerArmor
        Test-That "entered the frame $($frame.formId)" $inPowerArmor "distance $([int]$frame.distance)"
        if ($inPowerArmor) {
            $s = Wait-State { param($s) $s.light.location -eq 'OnPAHead' -and $s.light.beam.radius -eq $radius.paHead } 3
            Test-That 'light on the PA head with its beam' ($s.light.on -and $s.light.location -eq 'OnPAHead' -and $s.light.beam.radius -eq $radius.paHead) "on=$($s.light.on) location=$($s.light.location) radius=$($s.light.beam.radius)"
            Test-Gesture 'head long: to the offhand in PA' @{ gesture = 'head'; press = 'long' } $true $true 'InOffhand'
            Test-Gesture 'head tap: back on the PA head' @{ gesture = 'head' } $true $true 'OnPAHead'
            Invoke-ImFl @{ action = 'light'; location = $locationArg[$snapshot.locationInPA] } | Out-Null

            Invoke-Console 'player.cf "Actor.SwitchToPowerArmor" none' | Out-Null
            $s = Wait-State { param($s) -not $s.gating.inPowerArmor } 10
            $inPowerArmor = [bool]$s.gating.inPowerArmor
            Test-That 'stepped out' (-not $inPowerArmor)
            if (-not $inPowerArmor) {
                # the out-of-PA location is still the head, set before stepping in
                $s = Wait-State { param($s) $s.light.on -and $s.light.location -eq 'OnHead' } 3
                Test-That 'light restored on the head out of PA' ($s.light.on -and $s.light.location -eq 'OnHead') "on=$($s.light.on) location=$($s.light.location)"
            }
        }
    }

    # ------------------------------------------------------------ health

    Start-Section 'Health'
    $body = (Invoke-DevBench nodes @{ action = 'find'; name = 'ImmersiveFlashlightBody' }).matches
    Test-That 'stowed body model in the scene' (@($body).Count -ge 1)
    $perf = Invoke-ImFl @{ action = 'perf'; format = 'flat' }
    $p95 = $perf.sites.'Flashlight::onFrameUpdate'.p95Ms
    Test-That "Flashlight::onFrameUpdate p95 <= $FrameUpdateP95BudgetMs ms" ($null -ne $p95 -and $p95 -le $FrameUpdateP95BudgetMs) ("p95 {0:n3} ms over {1} frames" -f $p95, $perf.frames)
} catch {
    Add-Result 'run aborted' 'FAIL' "$_"
} finally {
    # ------------------------------------------------------------ cleanup

    Start-Section 'Cleanup'
    try {
        Invoke-ImFl @{ action = 'clear'; all = $true } | Out-Null
        Invoke-DevBench menu @{ action = 'close'; name = 'PauseMenu' } | Out-Null
        Invoke-ImFl @{ action = 'ui'; op = 'close' } | Out-Null
        if ($weaponEquipped) { Invoke-Console "player.unequipitem $WeaponFormId" | Out-Null }
        if ($weaponAdded) { Invoke-Console "player.removeitem $WeaponFormId 1" | Out-Null }
        if ($inPowerArmor) {
            Add-Result 'restore' 'WARN' 'still in power armor: the out-of-PA location was not restored'
        } else {
            $reply = Invoke-ImFl @{ action = 'light'; location = $locationArg[$snapshot.location]; op = ($snapshot.lightOn ? 'on' : 'off') }
            Add-Result 'restore' 'PASS' "iFlashlightLocation=$($snapshot.location), $(Format-Light $reply)"
        }
    } catch {
        Add-Result 'restore' 'FAIL' "$_"
    }

    # the log lines this run wrote, from the line count at the start
    try {
        $newLines = (Get-LogLineCount) - $logStart
        $lines = if ($newLines -gt 0) { @((Invoke-DevBench log @{ action = 'tail'; file = 'ImmersiveFlashlightVR.log'; tail = [Math]::Min($newLines, 5000) }).lines) } else { @() }
        $errors = @($lines | Where-Object { $_ -match '^\S+ (error|critical) ' })
        $warnings = @($lines | Where-Object { $_ -match '^\S+ warn(ing)? ' })
        Test-That 'no errors in the log' ($errors.Count -eq 0) (($errors | Select-Object -First 3) -join ' | ')
        if ($warnings.Count) { Add-Result 'log warnings' 'WARN' (($warnings | Select-Object -First 3) -join ' | ') }
    } catch {
        Add-Result 'log scan' 'FAIL' "$_"
    }
}

# ---------------------------------------------------------------- summary

$failed = @($results | Where-Object Result -EQ 'FAIL')
Write-Host ''
$results | Group-Object Result | ForEach-Object { Write-Host ("{0}: {1}" -f $_.Name, $_.Count) }
if ($failed) {
    Write-Host "`nFailed:" -ForegroundColor Red
    $failed | Format-Table Section, Check, Detail -AutoSize -Wrap | Out-String | Write-Host
}

Write-Host @'

Manual checklist (needs the headset):
  [ ] Gestures for real: belt grab / return with each hand, offhand to head (tap, long-press), hands together (tap, long-press): zones, icons, haptics
  [ ] Two-handed weapon toggle with a FRIK / ROCK two-handed grip; ROCK weapon in the offhand
  [ ] Pip-Boy open: the light stays on
  [ ] Power armor: step in and out with the animation (the light is restored)
  [ ] A weapon with a modeled lamp: the beam leaves the lamp, the glow cone lines up
  [ ] Beam look per location, gobos, colors; the beam tuning screen from Mods Config (Save / Discard)
  [ ] A hostile NPC notices the beam (state npc.lastEvent)
'@

exit ($failed ? 1 : 0)
