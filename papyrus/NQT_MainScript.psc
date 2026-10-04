Scriptname NQT_MainScript extends Quest
{Nearest Quest Tracker. MCM hotkeys: TrackNearest tracks the Pip-Boy quest whose marker is closest (by the
engine's path through doors, see NearestQuestTracker.dll) and optionally untracks the others; ExcludeCurrent
puts the tracked quest on the exclusion list and tracks the next nearest; RestoreLast takes the last one back.
Auto mode re-tracks the nearest quest whenever the Pip-Boy objectives change.
Quests not started yet (QuestStarts.json, read by the DLL): ShowNearestStart puts a marker on the nearest
start point (NQT_StartQuest, a Miscellaneous quest with one objective on alias StartRef); with "Include quests
not started" on, TrackNearest compares start points and Pip-Boy quests together. With "Include quests without a
marker" on, Pip-Boy quests whose objectives have no target are tracked too, after the quests with a marker in the
same world (the DLL orders by world: the player's, Commonwealth, Far Harbor, Nuka-World).}

GlobalVariable Property NQT_UntrackOthers Auto Const Mandatory
{1 = untrack every other Pip-Boy quest (MCM).}
GlobalVariable Property NQT_Notifications Auto Const Mandatory
{1 = show a notification with the tracked quest (MCM).}
GlobalVariable Property NQT_AutoTrack Auto Const Mandatory
{1 = track the nearest quest by itself when an objective is completed or a quest starts (MCM).}
FormList Property NQT_ExcludedList Auto Const Mandatory
{Copy of Excluded for the DLL: these quests are never picked and always untracked.}
Quest Property NQT_StartQuest Auto Const Mandatory
{Start marker: objective StartObjective points at alias StartAlias; the DLL sets the objective text.}
GlobalVariable Property NQT_IncludeStarts Auto Const Mandatory
{1 = TrackNearest and the auto mode also consider quests not started yet (MCM).}
GlobalVariable Property NQT_NoSpoilers Auto Const Mandatory
{1 = the start marker and notifications do not name the quest (MCM).}
GlobalVariable Property NQT_IncludeNoMarker Auto Const Mandatory
{1 = TrackNearest and the auto mode also consider Pip-Boy quests without a marker (MCM).}

; MCM "Excluded quests" dropdown (listFromProperty): element 0 is a hint, then the names of Excluded.
String[] Property ExcludedNames Auto Hidden
; Value of that dropdown (PropertyValueInt). MCM writes the picked index here, RestoreFromMenu resets it to 0.
Int Property MCMRestoreChoice = 0 Auto Hidden

Int SetupVersion = 0
Quest[] Excluded        ; excluded quests, oldest first (the save keeps them)
String[] Names          ; Pip-Boy names of Excluded, taken when each was excluded
Quest LastTracked       ; the quest the mod tracked last
Int LastSignature = 0   ; NQT_Native.GetObjectiveSignature at the previous auto tick
Bool AutoPending = false
Quest ShownStart        ; the quest whose start marker is shown; None if none
Int ShownMarkerID = 0   ; FormID of the ref in StartRef
Int LastSources = 1     ; sources of the last Track call (ExcludeCurrent goes on with the same kind)

Int Property AutoTimer = 1 AutoReadOnly Hidden
Float Property AutoInterval = 2.0 AutoReadOnly Hidden
Int Property MaxExcluded = 100 AutoReadOnly Hidden
Int Property MenuNameLength = 32 AutoReadOnly Hidden
Int Property StartPollTimer = 2 AutoReadOnly Hidden
Float Property StartPollInterval = 3.0 AutoReadOnly Hidden
; Must match tools/gen_esp.py (START_OBJECTIVE, START_ALIAS).
Int Property StartObjective = 0 AutoReadOnly Hidden
Int Property StartAlias = 0 AutoReadOnly Hidden
; Sources for NQT_Native.FindNearestTarget.
Int Property SourceQuests = 1 AutoReadOnly Hidden
Int Property SourceStarts = 2 AutoReadOnly Hidden
Int Property SourceNoMarker = 4 AutoReadOnly Hidden
; Flags in element 2 of NQT_Native.FindNearestTarget's result.
Int Property FlagNoMarker = 1 AutoReadOnly Hidden

Event OnInit()
    Setup()
EndEvent

; Registered in Setup. Later versions put their migrations into Setup (SetupVersion), and this runs them.
Event Actor.OnPlayerLoadGame(Actor akSender)
    Setup()
    Cleanup()
    StartAuto()
    If ShownStart
        SetStartText()      ; the DLL's objective text is not saved
        StartTimer(StartPollInterval, StartPollTimer)
    EndIf
    NQT_Native.Log("OnPlayerLoadGame: " + Excluded.Length + " excluded, auto " + (NQT_AutoTrack.GetValue() != 0.0) \
        + ", start marker " + ShownStart)
EndEvent

Event OnTimer(Int aiTimerID)
    If aiTimerID == StartPollTimer
        PollStart()
        Return
    EndIf
    If aiTimerID != AutoTimer || NQT_AutoTrack.GetValue() == 0.0
        Return
    EndIf
    ; A change is acted on one tick later, once the list is stable again: the engine builds the paths
    ; to new targets asynchronously, and a stage often completes one objective and shows the next.
    Int signature = NQT_Native.GetObjectiveSignature()
    If signature != LastSignature
        NQT_Native.Log("auto: Pip-Boy objectives changed (" + LastSignature + " -> " + signature + ")")
        LastSignature = signature
        AutoPending = true
    ElseIf AutoPending
        AutoPending = false
        Track(true, Sources())
    EndIf
    StartTimer(AutoInterval, AutoTimer)
EndEvent

; Runs once per save: from OnInit, and from every entry point for saves made before 2.0 (there the quest
; already ran OnInit, so nothing is registered until a hotkey or an MCM action calls this).
Function Setup()
    If SetupVersion >= 1
        Return
    EndIf
    SetupVersion = 1
    RegisterForRemoteEvent(Game.GetPlayer(), "OnPlayerLoadGame")
    Excluded = new Quest[1]
    Excluded.Clear()
    Names = new String[1]
    Names.Clear()
    NQT_ExcludedList.Revert()
    SyncExcluded()
    StartAuto()
EndFunction

; ---------------------------------------------------------------------------------------------------
; MCM hotkeys (keybinds.json)

Function TrackNearest()
    Setup()
    Track(false, Sources())
EndFunction

Function ShowNearestStart()
    Setup()
    Track(false, SourceStarts)
EndFunction

Function ExcludeCurrent()
    Setup()
    Cleanup()
    Bool russian = IsRussian()
    Quest target = PickToExclude()
    If !target
        NQT_Native.Log("ExcludeCurrent: no tracked quest")
        Notify(russian, "Нет отмеченного квеста", "No tracked quest")
        Return
    EndIf
    Bool wasStart = target == ShownStart
    String name
    If wasStart
        name = StartName(target, russian)
    Else
        name = NQT_Native.GetQuestDisplayName(target)
    EndIf
    If Excluded.Length >= MaxExcluded
        Excluded.Remove(0)
        Names.Remove(0)
    EndIf
    Excluded.Add(target)
    Names.Add(name)
    SyncExcluded()
    Int sources = Sources()
    If wasStart
        sources = LastSources   ; the next start point when the start hotkey was used
        ClearStart()
    Else
        target.SetActive(false)
    EndIf
    If LastTracked == target
        LastTracked = None
    EndIf
    NQT_Native.Log("ExcludeCurrent: " + target + " '" + name + "' (start " + wasStart + "), " + Excluded.Length + " excluded")
    Notify(russian, "Исключён: " + name, "Excluded: " + name)
    Track(false, sources)
EndFunction

Function RestoreLast()
    Setup()
    Cleanup()
    Bool russian = IsRussian()
    If Excluded.Length == 0
        Notify(russian, "Нет исключённых квестов", "No excluded quests")
        Return
    EndIf
    Int last = Excluded.Length - 1
    String name = Names[last]
    NQT_Native.Log("RestoreLast: " + Excluded[last] + " '" + name + "'")
    RemoveExcludedAt(last)
    Notify(russian, "Возвращён: " + name, "Restored: " + name)
    Track(false, Sources())
EndFunction

; ---------------------------------------------------------------------------------------------------
; MCM page (config.json)

; Dropdown of excluded quests: aiChoice is the picked element of ExcludedNames (0 = the hint).
; Like the restore hotkey, a restored quest takes part in the search at once: the nearest quest is re-tracked.
Function RestoreFromMenu(Int aiChoice)
    Setup()
    MCMRestoreChoice = 0
    Int index = aiChoice - 1
    If index < 0 || index >= Excluded.Length
        MCM.RefreshMenu()
        Return
    EndIf
    NQT_Native.Log("RestoreFromMenu: " + Excluded[index] + " '" + Names[index] + "'")
    RemoveExcludedAt(index)
    MCM.RefreshMenu()
    Track(false, Sources())
EndFunction

Function RestoreAll()
    Setup()
    NQT_Native.Log("RestoreAll: " + Excluded.Length + " quests")
    Excluded.Clear()
    Names.Clear()
    SyncExcluded()
    MCM.RefreshMenu()
    Track(false, Sources())
EndFunction

; "Automatic" switcher: its action runs after MCM has set NQT_AutoTrack.
Function OnAutoTrackChanged()
    Setup()
    StartAuto()
EndFunction

; "No spoilers" switcher: rewrite the text of a start marker already shown.
Function OnNoSpoilersChanged()
    Setup()
    If ShownStart
        SetStartText()
    EndIf
EndFunction

; ---------------------------------------------------------------------------------------------------

; Tracks the nearest non-excluded quest and untracks the others (see UntrackOthers). aiSources: SourceQuests
; (Pip-Boy quests) and/or SourceStarts (start points of quests not started yet): if a start point wins, its
; marker is shown instead of SetActive; otherwise a start marker shown before is removed. abAuto: called by the
; auto mode, which stays silent when nothing changes. Returns the tracked quest or None.
Quest Function Track(Bool abAuto, Int aiSources)
    Cleanup()
    LastSources = aiSources
    Int[] found = NQT_Native.FindNearestTarget(NQT_ExcludedList, aiSources)
    Quest nearest = None
    If found[0] != 0
        nearest = Game.GetForm(found[0]) as Quest
    EndIf
    Bool isStart = nearest && found[1] != 0
    Bool noMarker = nearest && found[2] == FlagNoMarker   ; the only flag so far
    Bool alreadyTracked
    If isStart
        alreadyTracked = nearest == ShownStart && NQT_StartQuest.IsActive()
        If !ShowStart(nearest, found[1])
            nearest = None
            isStart = false
        EndIf
    Else
        ClearStart()
        alreadyTracked = nearest && nearest.IsActive()
        If nearest
            nearest.SetActive(true)
        EndIf
    EndIf
    Quest keep = None
    If !isStart
        keep = nearest
    EndIf
    Int untracked = UntrackOthers(keep)
    LastTracked = nearest

    Bool russian = IsRussian()
    If !nearest
        NQT_Native.Log("Track(auto " + abAuto + ", sources " + aiSources + "): nothing found; untracked " + untracked)
        If abAuto
            ; silent
        ElseIf aiSources == SourceStarts
            Notify(russian, "Нет доступных новых квестов", "No new quests available")
        Else
            Notify(russian, "Нет квестов с маркером", "No quests with a marker")
        EndIf
        Return None
    EndIf

    String name
    If isStart
        name = StartName(nearest, russian)
    Else
        name = NQT_Native.GetQuestDisplayName(nearest)
    EndIf
    NQT_Native.Log("Track(auto " + abAuto + ", sources " + aiSources + "): " + nearest + " '" + name + "' start " + isStart + " noMarker " + noMarker \
        + " at " + NQT_Native.GetQuestDistance(nearest) + "; untracked " + untracked)
    If !abAuto || !alreadyTracked
        If isStart
            String label = StartLabel(nearest, russian)
            Notify(russian, label, label)
        ElseIf noMarker
            Notify(russian, "Отслеживается: " + name + " (без маркера)", "Tracking: " + name + " (no marker)")
        Else
            Notify(russian, "Отслеживается: " + name, "Tracking: " + name)
        EndIf
    EndIf
    Return nearest
EndFunction

Int Function Sources()
    Int sources = SourceQuests
    If NQT_IncludeStarts.GetValue() != 0.0
        sources += SourceStarts
    EndIf
    If NQT_IncludeNoMarker.GetValue() != 0.0
        sources += SourceNoMarker
    EndIf
    Return sources
EndFunction

; ---------------------------------------------------------------------------------------------------
; Start marker

; Points StartRef at the ref aiMarkerID (the quest giver, or a door / map marker on the way to it) and shows
; the objective. The objective is shown again when the quest changes, so the HUD names the new one.
Bool Function ShowStart(Quest akQuest, Int aiMarkerID)
    ObjectReference target = Game.GetForm(aiMarkerID) as ObjectReference
    ReferenceAlias startRef = NQT_StartQuest.GetAlias(StartAlias) as ReferenceAlias
    If !target || !startRef
        NQT_Native.Log("ShowStart: " + akQuest + ": no ref " + aiMarkerID + " or alias " + startRef)
        Return false
    EndIf
    If !NQT_StartQuest.IsRunning()
        NQT_StartQuest.Start()
    EndIf
    Bool changed = akQuest != ShownStart
    ShownStart = akQuest
    ShownMarkerID = aiMarkerID
    startRef.ForceRefTo(target)
    SetStartText()
    If changed || !NQT_StartQuest.IsObjectiveDisplayed(StartObjective)
        NQT_StartQuest.SetObjectiveDisplayed(StartObjective, false)
        NQT_StartQuest.SetObjectiveDisplayed(StartObjective, true, true)
    EndIf
    NQT_StartQuest.SetActive(true)
    NQT_Native.Log("ShowStart: " + akQuest + " marker on " + target + " (changed " + changed + ")")
    CancelTimer(StartPollTimer)
    StartTimer(StartPollInterval, StartPollTimer)
    Return true
EndFunction

Function ClearStart()
    If !ShownStart && !NQT_StartQuest.IsObjectiveDisplayed(StartObjective)
        Return
    EndIf
    NQT_Native.Log("ClearStart: " + ShownStart)
    CancelTimer(StartPollTimer)
    ShownStart = None
    ShownMarkerID = 0
    NQT_StartQuest.SetObjectiveDisplayed(StartObjective, false)
    ReferenceAlias startRef = NQT_StartQuest.GetAlias(StartAlias) as ReferenceAlias
    If startRef
        startRef.Clear()
    EndIf
    NQT_StartQuest.SetActive(false)
EndFunction

; While a start marker is shown: move it onto the giver once the giver is in memory (its cell loaded), and
; remove it once the quest has started or the giver is dead or disabled (the auto mode then re-tracks).
Function PollStart()
    If !ShownStart
        Return
    EndIf
    Int markerID = NQT_Native.GetStartMarkerID(ShownStart)
    If markerID == 0
        NQT_Native.Log("PollStart: " + ShownStart + " started or no longer available")
        ClearStart()
        If NQT_AutoTrack.GetValue() != 0.0
            Track(true, Sources())
        EndIf
        Return
    EndIf
    If markerID != ShownMarkerID
        ObjectReference target = Game.GetForm(markerID) as ObjectReference
        ReferenceAlias startRef = NQT_StartQuest.GetAlias(StartAlias) as ReferenceAlias
        If target && startRef
            NQT_Native.Log("PollStart: " + ShownStart + " marker moved to " + target)
            startRef.ForceRefTo(target)
            ShownMarkerID = markerID
        EndIf
    EndIf
    StartTimer(StartPollInterval, StartPollTimer)
EndFunction

Function SetStartText()
    NQT_Native.SetObjectiveText(NQT_StartQuest, StartObjective, StartLabel(ShownStart, IsRussian()))
EndFunction

; Objective text and notification of a start marker: "New quest: <name>", or just "New quest" with "No spoilers".
; A radio start adds what to do: "New quest: <name> - listen to the radio: <station>".
String Function StartLabel(Quest akQuest, Bool abRussian)
    String label = Choose(abRussian, "Новый квест", "New quest")
    If NQT_NoSpoilers.GetValue() == 0.0
        String name = NQT_Native.GetStartName(akQuest)
        If name != ""
            label += ": " + name
        EndIf
    EndIf
    If NQT_Native.IsRadioStart(akQuest)
        String station = NQT_Native.GetStartRadioName(akQuest)
        If station != ""
            label += Choose(abRussian, " — послушать радио «" + station + "»", " - listen to the radio: " + station)
        Else
            label += Choose(abRussian, " — поймать радиосигнал", " - pick up the radio signal")
        EndIf
    EndIf
    Return label
EndFunction

; Name of a quest not started yet for the log and the exclusion list; "new quest" with "No spoilers" (or no name).
String Function StartName(Quest akQuest, Bool abRussian)
    String name = ""
    If NQT_NoSpoilers.GetValue() == 0.0
        name = NQT_Native.GetStartName(akQuest)
    EndIf
    If name == ""
        name = Choose(abRussian, "новый квест", "new quest")
    EndIf
    Return name
EndFunction

; Untracks the Pip-Boy quests other than akKeep: all of them if NQT_UntrackOthers is on, otherwise only
; the excluded ones. Returns how many were untracked.
Int Function UntrackOthers(Quest akKeep)
    Bool all = NQT_UntrackOthers.GetValue() != 0.0
    Quest[] quests = NQT_Native.GetTrackedQuestList()
    Int count = 0
    Int i = 0
    While i < quests.Length
        Quest q = quests[i]
        If q && q != akKeep && q.IsActive() && (all || Excluded.Find(q) >= 0)
            q.SetActive(false)
            count += 1
        EndIf
        i += 1
    EndWhile
    Return count
EndFunction

; The quest the exclude hotkey means: the one the mod tracked last, if it is still tracked and in the
; Pip-Boy; otherwise the nearest of the tracked Pip-Boy quests. None if no quest is tracked.
Quest Function PickToExclude()
    Bool startShown = ShownStart && NQT_StartQuest.IsActive() && Excluded.Find(ShownStart) < 0
    If startShown && LastTracked == ShownStart
        Return ShownStart
    EndIf
    Quest[] quests = NQT_Native.GetTrackedQuestList()
    If LastTracked && LastTracked.IsActive() && quests.Find(LastTracked) >= 0 && Excluded.Find(LastTracked) < 0
        Return LastTracked
    EndIf
    Quest best = None
    Float bestDistance = 0.0
    Int i = 0
    While i < quests.Length
        Quest q = quests[i]
        If q && q.IsActive() && Excluded.Find(q) < 0
            Float distance = NQT_Native.GetQuestDistance(q)
            If distance == -2.0          ; another worldspace
                distance = 1000000000.0
            ElseIf distance < 0.0        ; no target with a position
                distance = 2000000000.0
            EndIf
            If !best || distance < bestDistance
                best = q
                bestDistance = distance
            EndIf
        EndIf
        i += 1
    EndWhile
    If !best && startShown
        Return ShownStart
    EndIf
    Return best
EndFunction

; Completed or stopped quests leave the exclusion list (the exclusion was for that run of the quest). A quest
; with a start point (QuestStarts.json) is often not running before it starts, so only completion counts there.
Function Cleanup()
    Bool changed = false
    Int i = Excluded.Length - 1
    While i >= 0
        Quest q = Excluded[i]
        If !q || q.IsCompleted() || (!q.IsRunning() && !NQT_Native.HasQuestStart(q))
            NQT_Native.Log("Cleanup: " + q + " '" + Names[i] + "' is no longer running")
            Excluded.Remove(i)
            Names.Remove(i)
            changed = true
        EndIf
        i -= 1
    EndWhile
    If changed
        SyncExcluded()
    EndIf
EndFunction

Function RemoveExcludedAt(Int aiIndex)
    Excluded.Remove(aiIndex)
    Names.Remove(aiIndex)
    SyncExcluded()
EndFunction

; Copies Excluded to the FormList the DLL reads and rebuilds the MCM dropdown list.
Function SyncExcluded()
    NQT_ExcludedList.Revert()
    Bool russian = IsRussian()
    ExcludedNames = new String[1]
    If Excluded.Length == 0
        ExcludedNames[0] = Choose(russian, "(пусто)", "(empty)")
    Else
        ExcludedNames[0] = Choose(russian, "— выберите —", "— pick one —")
    EndIf
    Int i = 0
    While i < Excluded.Length
        NQT_ExcludedList.AddForm(Excluded[i])
        ExcludedNames.Add(NQT_Native.Shorten(Names[i], MenuNameLength))  ; MCM shrinks long lines to tiny text
        i += 1
    EndWhile
EndFunction

Function StartAuto()
    CancelTimer(AutoTimer)
    AutoPending = false
    If NQT_AutoTrack.GetValue() != 0.0
        LastSignature = NQT_Native.GetObjectiveSignature()
        StartTimer(AutoInterval, AutoTimer)
    EndIf
EndFunction

Bool Function IsRussian()
    Return NQT_Native.GetLanguage() == "ru"
EndFunction

String Function Choose(Bool abRussian, String asRussian, String asEnglish)
    If abRussian
        Return asRussian
    EndIf
    Return asEnglish
EndFunction

Function Notify(Bool abRussian, String asRussian, String asEnglish)
    If NQT_Notifications.GetValue() != 0.0
        Debug.Notification(Choose(abRussian, asRussian, asEnglish))
    EndIf
EndFunction
