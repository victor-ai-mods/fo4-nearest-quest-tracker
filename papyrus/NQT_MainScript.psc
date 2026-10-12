Scriptname NQT_MainScript extends Quest
{Nearest Quest Tracker. MCM hotkeys: TrackNearest marks the nearest target among the kinds turned on in MCM
(NQT_IncludeQuests: Pip-Boy quests, by the engine's path through doors, see NearestQuestTracker.dll; with
NQT_IncludeNoMarker also Pip-Boy quests without a marker; NQT_IncludeStarts: start points of quests not started yet
from QuestStarts.json; NQT_IncludeLocations: map markers not discovered yet; NQT_IncludeInteriors: interiors never
entered, by the Pip-Boy local map fog of war), all mixed by distance within a world (the DLL orders by world: the
player's, Commonwealth, Far Harbor, Nuka-World). A Pip-Boy quest is tracked with SetActive; a start point and a
location get a marker of their own (NQT_StartQuest, NQT_LocationQuest: Miscellaneous quests with one objective on one
alias); an interior shares the location marker, put on the door into it. With NQT_UntrackOthers every other marker is removed.
ExcludeCurrent puts the marked target on the exclusion list and marks the next nearest; RestoreLast takes the last
one back. Auto mode marks the nearest target again whenever the Pip-Boy objectives change, a start marker's quest
starts, the marked location is discovered or the marked interior entered.}

GlobalVariable Property NQT_UntrackOthers Auto Const Mandatory
{1 = only one marker: TrackNearest removes all others (MCM).}
GlobalVariable Property NQT_Notifications Auto Const Mandatory
{1 = show a notification with the marked target (MCM).}
GlobalVariable Property NQT_AutoTrack Auto Const Mandatory
{1 = mark the nearest target by itself when an objective is completed, a quest starts or a location is found (MCM).}
FormList Property NQT_ExcludedList Auto Const Mandatory
{Quests of ExcludedItems for the DLL: never picked, always untracked.}
Quest Property NQT_StartQuest Auto Const Mandatory
{Start marker: objective StartObjective points at alias StartAlias; the DLL sets the objective text.}
GlobalVariable Property NQT_IncludeStarts Auto Const Mandatory
GlobalVariable Property NQT_IncludeRadiant Auto Const Mandatory
{1 = TrackNearest picks start points of quests not started yet (MCM "Pick new quests").}
GlobalVariable Property NQT_NoSpoilers Auto Const Mandatory
{1 = the start marker and notifications do not name the quest (MCM).}
GlobalVariable Property NQT_IncludeNoMarker Auto Const Mandatory
{1 = TrackNearest also picks Pip-Boy quests without a marker, with NQT_IncludeQuests only (MCM).}
Quest Property NQT_LocationQuest Auto Const Mandatory
{Location marker: objective LocationObjective points at alias LocationRefAlias (a map marker); the DLL sets the text.}
GlobalVariable Property NQT_AllLocations Auto Const Mandatory
{1 = any location not discovered yet, not only those seen (MCM).}
FormList Property NQT_ExcludedLocationList Auto Const Mandatory
{Map markers and interior doors of ExcludedItems for the DLL: never picked (a door: its interior).}
GlobalVariable Property NQT_IncludeQuests Auto Const Mandatory
{1 = TrackNearest picks Pip-Boy quests (MCM "Pick active quests").}
GlobalVariable Property NQT_IncludeLocations Auto Const Mandatory
{1 = TrackNearest picks map locations not discovered yet (MCM "Pick undiscovered locations").}
GlobalVariable Property NQT_IncludeInteriors Auto Const Mandatory
{1 = TrackNearest picks interiors never entered (MCM "Pick interiors").}
GlobalVariable Property NQT_SkipOwnedLocked Auto Const Mandatory
{1 = interiors behind a locked door that is a crime to pick are skipped (MCM "Skip locked doors of others").}
GlobalVariable Property NQT_ShowQuestInWindow Auto Const Mandatory
{1 = ShowCurrentQuest shows a message window instead of notifications (MCM "Current quest in a window").}
Potion Property NQT_WindowItem Auto Const Mandatory
{The item "Nearest Quest Tracker": using it opens the window (with a gamepad: from the favorites).}

; MCM "Excluded" dropdown (listFromProperty): element 0 is a hint, then the labels of ExcludedItems.
String[] Property ExcludedNames Auto Hidden
; Value of that dropdown (PropertyValueInt). MCM writes the picked index here, RestoreFromMenu resets it to 0.
Int Property MCMRestoreChoice = 0 Auto Hidden

Int SetupVersion = 0
Form[] ExcludedItems    ; excluded quests (Quest), locations (map marker) and interiors (door), oldest first
String[] ExcludedItemNames  ; their names, taken when each was excluded
Int[] ExcludedKinds     ; KindQuest, KindStart, KindLocation or KindInterior for each of ExcludedItems
Int LastKind = 0        ; kind of the target the mod marked last (Kind*)
Quest LastTracked       ; the quest the mod marked last (Pip-Boy quest or start point)
Int LastSignature = 0   ; NQT_Native.GetObjectiveSignature at the previous auto tick
Bool AutoPending = false
Quest ShownStart        ; the quest whose start marker is shown; None if none
Int ShownMarkerID = 0   ; FormID of the ref in StartRef
ObjectReference ShownLocation       ; the map marker (or interior door) under the location marker; None if none
Bool ShownLocationNamed = false     ; its objective names it (else "Undiscovered location: <icon type>")
Bool ShownInterior = false          ; ShownLocation is the door into an interior
; Before SetupVersion 3 (moved into ExcludedItems by Setup, then None):
Quest[] Excluded
String[] Names
ObjectReference[] ExcludedLocations
String[] LocationNames

Int Property AutoTimer = 1 AutoReadOnly Hidden
Float Property AutoInterval = 2.0 AutoReadOnly Hidden
Int Property MaxExcluded = 100 AutoReadOnly Hidden
Int Property MenuNameLength = 32 AutoReadOnly Hidden
Int Property StartPollTimer = 2 AutoReadOnly Hidden
Float Property StartPollInterval = 3.0 AutoReadOnly Hidden
Int Property LocationPollTimer = 3 AutoReadOnly Hidden
Float Property LocationPollInterval = 3.0 AutoReadOnly Hidden
; DIAG 2026-10-13 (HUD messages stop): HudDump(tag) writes the HUD message queue now, after 1 s and after 5 s.
Int Property HudDumpTimer1 = 4 AutoReadOnly Hidden
Int Property HudDumpTimer5 = 5 AutoReadOnly Hidden
String HudDumpTag = ""
; Must match tools/gen_esp.py (START_OBJECTIVE, START_ALIAS; the location quest has the same layout).
Int Property StartObjective = 0 AutoReadOnly Hidden
Int Property StartAlias = 0 AutoReadOnly Hidden
Int Property LocationObjective = 0 AutoReadOnly Hidden
Int Property LocationRefAlias = 0 AutoReadOnly Hidden
; Sources for NQT_Native.FindNearestTarget.
Int Property SourceQuests = 1 AutoReadOnly Hidden
Int Property SourceStarts = 2 AutoReadOnly Hidden
Int Property SourceNoMarker = 4 AutoReadOnly Hidden
Int Property SourceLocations = 8 AutoReadOnly Hidden
Int Property SourceInteriors = 16 AutoReadOnly Hidden
Int Property SourceSkipOwnedLocked = 32 AutoReadOnly Hidden ; with SourceInteriors
Int Property SourceRadiant = 64 AutoReadOnly Hidden ; givers of radiant quests while they offer one
; Target kinds: element 0 of NQT_Native.FindNearestTarget's result, LastKind, ExcludedKinds (KindQuest for both
; Pip-Boy kinds there).
Int Property KindNone = 0 AutoReadOnly Hidden
Int Property KindQuest = 1 AutoReadOnly Hidden
Int Property KindNoMarker = 2 AutoReadOnly Hidden
Int Property KindStart = 3 AutoReadOnly Hidden
Int Property KindLocation = 4 AutoReadOnly Hidden
Int Property KindInterior = 5 AutoReadOnly Hidden

; Own window (menu/NQTMenu.as -> Interface\NearestQuestTracker.swf); the name is the one NQTMenu.MENU_NAME closes.
String Property WindowMenu = "NQT_Menu" AutoReadOnly Hidden
String Property WindowFile = "NearestQuestTracker" AutoReadOnly Hidden
String Property WindowLogEvent = "NQT_MenuLog" AutoReadOnly Hidden
String Property WindowActionEvent = "NQT_MenuAction" AutoReadOnly Hidden
; Menus the window waits for to close before it opens (it would open under the pause menu, MCM lives there).
String Property PipboyMenu = "PipboyMenu" AutoReadOnly Hidden
String Property PauseMenu = "PauseMenu" AutoReadOnly Hidden
Bool WindowPending = false

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
    If ShownLocation
        SetLocationText()   ; neither is this one
        StartTimer(LocationPollInterval, LocationPollTimer)
    EndIf
    NQT_Native.Log("OnPlayerLoadGame: " + ExcludedItems.Length + " excluded, auto " + (NQT_AutoTrack.GetValue() != 0.0) \
        + ", sources " + Sources() + ", start marker " + ShownStart + ", location marker " + ShownLocation)
EndEvent

Event OnTimer(Int aiTimerID)
    If aiTimerID == HudDumpTimer1
        NQT_Native.DumpHudMessages(HudDumpTag + " +1s")
        Return
    EndIf
    If aiTimerID == HudDumpTimer5
        NQT_Native.DumpHudMessages(HudDumpTag + " +5s")
        Return
    EndIf
    If aiTimerID == StartPollTimer
        PollStart()
        Return
    EndIf
    If aiTimerID == LocationPollTimer
        PollLocation()
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
        Track(true)
    EndIf
    StartTimer(AutoInterval, AutoTimer)
EndEvent

; Runs each step once per save: from OnInit, and from every entry point for saves made before the step existed
; (there the quest already ran OnInit, so nothing is set up until a hotkey, an MCM action or a load calls this).
; Version 1: events and auto mode; 2: excluded locations (development builds only); 3: one exclusion list for
; quests and locations (the lists of versions 1 and 2 move into it); 4: the item that opens the window.
Function Setup()
    If SetupVersion < 1
        RegisterForRemoteEvent(Game.GetPlayer(), "OnPlayerLoadGame")
        StartAuto()
    EndIf
    If SetupVersion < 3
        ExcludedItems = new Form[1]
        ExcludedItems.Clear()
        ExcludedItemNames = new String[1]
        ExcludedItemNames.Clear()
        ExcludedKinds = new Int[1]
        ExcludedKinds.Clear()
        Int i = 0
        While Excluded != None && i < Excluded.Length
            Quest q = Excluded[i]
            Int kind = KindQuest
            If q && !q.IsRunning() && NQT_Native.HasQuestStart(q)
                kind = KindStart
            EndIf
            AddExcluded(q, Names[i], kind)
            i += 1
        EndWhile
        i = 0
        While ExcludedLocations != None && i < ExcludedLocations.Length
            AddExcluded(ExcludedLocations[i], LocationNames[i], KindLocation)
            i += 1
        EndWhile
        Excluded = None
        Names = None
        ExcludedLocations = None
        LocationNames = None
        SyncExcluded()
        NQT_Native.Log("Setup: version " + SetupVersion + " -> 3, " + ExcludedItems.Length + " excluded")
    EndIf
    If SetupVersion < 4
        GiveWindowItem(false)
    EndIf
    SetupVersion = 4
EndFunction

; ---------------------------------------------------------------------------------------------------
; MCM hotkeys (keybinds.json)

Function TrackNearest()
    Setup()
    Track(false)
EndFunction

; Excludes the marked target (see PickToExclude) and marks the next nearest one.
Function ExcludeCurrent()
    Setup()
    Cleanup()
    Bool russian = IsRussian()
    Int kind = PickToExclude()
    If kind == KindNone
        NQT_Native.Log("ExcludeCurrent: nothing marked")
        Notify(russian, "Ничего не отмечено", "Nothing is marked")
        Return
    EndIf
    Form target
    If kind == KindLocation || kind == KindInterior
        target = ShownLocation
    ElseIf kind == KindStart
        target = ShownStart
    Else
        target = LastTracked
    EndIf
    LastKind = KindNone
    ExcludeTarget(kind, target)
    Track(false)
EndFunction

; Puts akTarget on the exclusion list: aiKind KindQuest or KindNoMarker (a Pip-Boy quest: untracked), KindStart (a
; quest not started: its start marker goes if shown), KindLocation or KindInterior (a map marker or the door into an
; interior: the location marker goes if it is on it).
Function ExcludeTarget(Int aiKind, Form akTarget)
    If !akTarget
        Return
    EndIf
    Bool russian = IsRussian()
    Int kind = aiKind
    String name
    If kind == KindLocation || kind == KindInterior
        ObjectReference ref = akTarget as ObjectReference
        Bool named = kind == KindInterior || NQT_AllLocations.GetValue() != 0.0 || ref.IsMapMarkerVisible()
        If ref == ShownLocation
            named = ShownLocationNamed
            ClearLocation()
            If LastKind == KindLocation || LastKind == KindInterior
                LastKind = KindNone
            EndIf
        EndIf
        name = LocationName(ref, named, kind == KindInterior, russian)
    ElseIf kind == KindStart
        Quest q = akTarget as Quest
        name = StartName(q, russian)
        If q == ShownStart
            ClearStart()
        EndIf
    Else
        kind = KindQuest
        Quest q = akTarget as Quest
        name = NQT_Native.GetQuestDisplayName(q)
        q.SetActive(false)
    EndIf
    If akTarget == LastTracked
        LastTracked = None
        LastKind = KindNone
    EndIf
    If ExcludedItems.Length >= MaxExcluded
        RemoveExcludedAt(0)
    EndIf
    AddExcluded(akTarget, name, kind)
    SyncExcluded()
    NQT_Native.Log("ExcludeTarget: " + akTarget + " '" + name + "' (kind " + kind + "), " + ExcludedItems.Length + " excluded")
    Notify(russian, "Исключено: " + name, "Excluded: " + name)
EndFunction

Function RestoreLast()
    Setup()
    Cleanup()
    Bool russian = IsRussian()
    If ExcludedItems.Length == 0
        Notify(russian, "Список исключённого пуст", "Nothing is excluded")
        Return
    EndIf
    Int last = ExcludedItems.Length - 1
    String name = ExcludedItemNames[last]
    NQT_Native.Log("RestoreLast: " + ExcludedItems[last] + " '" + name + "'")
    RemoveExcludedAt(last)
    SyncExcluded()
    Notify(russian, "Возвращено: " + name, "Restored: " + name)
    Track(false)
EndFunction

; DIAG 2026-10-13: the HUD message queue before a hotkey's notifications, 1 s and 5 s later.
Function HudDump(String asTag)
    HudDumpTag = asTag
    NQT_Native.DumpHudMessages(asTag + " before")
    StartTimer(1.0, HudDumpTimer1)
    StartTimer(5.0, HudDumpTimer5)
EndFunction

; Turns the auto mode (NQT_AutoTrack, MCM "Automatic") on or off. The notification is shown even with
; NQT_Notifications off: it is the only sign the key worked.
Function ToggleAutoTrack()
    Setup()
    HudDump("ToggleAutoTrack")
    Bool russian = IsRussian()
    If NQT_AutoTrack.GetValue() != 0.0
        NQT_AutoTrack.SetValue(0.0)
        Debug.Notification(Choose(russian, "Автоматический режим выключен", "Automatic mode off"))
    Else
        NQT_AutoTrack.SetValue(1.0)
        Debug.Notification(Choose(russian, "Автоматический режим включён", "Automatic mode on"))
    EndIf
    NQT_Native.Log("ToggleAutoTrack: auto " + NQT_AutoTrack.GetValue())
    StartAuto()
EndFunction

; Names what is marked now and what to do there: the tracked Pip-Boy quests and the mod's own start and location
; markers (NQT_StartQuest, NQT_LocationQuest - GetTrackedQuestList leaves them out), the target the mod marked last
; first. One notification per line, or with NQT_ShowQuestInWindow one message window. A Miscellaneous quest (the
; markers too) has no name of its own: only its objectives. Shown even with NQT_Notifications off: showing is what
; the key is for.
Function ShowCurrentQuest()
    Setup()
    HudDump("ShowCurrentQuest")
    Bool russian = IsRussian()
    Bool window = NQT_ShowQuestInWindow.GetValue() != 0.0
    Quest[] quests = new Quest[1]
    quests.Clear()
    Bool startShown = NQT_StartQuest.IsActive() && NQT_StartQuest.IsObjectiveDisplayed(StartObjective)
    Bool locationShown = NQT_LocationQuest.IsActive() && NQT_LocationQuest.IsObjectiveDisplayed(LocationObjective)
    If (LastKind == KindLocation || LastKind == KindInterior) && locationShown
        quests.Add(NQT_LocationQuest)
    ElseIf LastKind == KindStart && startShown
        quests.Add(NQT_StartQuest)
    ElseIf (LastKind == KindQuest || LastKind == KindNoMarker) && LastTracked && LastTracked.IsActive()
        quests.Add(LastTracked)
    EndIf
    Quest[] tracked = NQT_Native.GetTrackedQuestList()
    Int i = 0
    While i < tracked.Length
        If tracked[i] && tracked[i].IsActive() && quests.Find(tracked[i]) < 0
            quests.Add(tracked[i])
        EndIf
        i += 1
    EndWhile
    If startShown && quests.Find(NQT_StartQuest) < 0
        quests.Add(NQT_StartQuest)
    EndIf
    If locationShown && quests.Find(NQT_LocationQuest) < 0
        quests.Add(NQT_LocationQuest)
    EndIf

    String text = ""
    Int count = 0
    i = 0
    While i < quests.Length
        Quest q = quests[i]
        String[] objectives = NQT_Native.GetObjectiveTexts(q)
        String name = NQT_Native.GetQuestDisplayName(q)
        Bool misc = objectives.Find(name) >= 0    ; a Miscellaneous quest is named by its objective
        NQT_Native.Log("ShowCurrentQuest: " + q + " '" + name + "', " + objectives.Length + " objectives, misc " + misc)
        If objectives.Length > 0
            If window && count > 0
                text += "\n\n"
            EndIf
            If !misc
                If window
                    text += name
                Else
                    Debug.Notification(name)
                EndIf
            EndIf
            Int j = 0
            While j < objectives.Length
                String line = objectives[j]
                If !misc
                    line = "- " + line
                EndIf
                If window
                    If !misc || j > 0
                        text += "\n"
                    EndIf
                    text += line
                Else
                    Debug.Notification(line)
                EndIf
                j += 1
            EndWhile
            count += 1
        EndIf
        i += 1
    EndWhile
    If count == 0
        NQT_Native.Log("ShowCurrentQuest: no tracked quest")
        Debug.Notification(Choose(russian, "Нет отслеживаемых квестов", "No quest is tracked"))
    ElseIf window
        Debug.MessageBox(text)
    EndIf
EndFunction

; Opens the window (closes it if open). F4SE keeps custom menus only for the game session, so the menu is
; registered on first use after each game start. Flags: F4SE's default (= MessageBoxMenu's plus the cursor:
; pauses the game, mouse cursor, menu controls, modal) without kCustomRendering 0x8000 and
; kAssignCursorToRenderer 0x10000 - with them the menu went into the screen model texture, which in power armor
; is the armor HUD's (the window flickered, half drawn); PauseMenu draws straight to the screen without them too.
; Extended flags as F4SE's: the player's interface color, no cursor with a gamepad. Depth not F4SE's 6
; (kStandard, PowerArmorRenderer's) but 9 (kTerminal), like TerminalMenu, LevelUpMenu and BookMenu.
Function ToggleWindow()
    If UI.IsMenuOpen(WindowMenu)
        UI.CloseMenu(WindowMenu)
        NQT_Native.Log("ToggleWindow: closed")
        Return
    EndIf
    If !UI.IsMenuRegistered(WindowMenu)
        UI:MenuData data = new UI:MenuData
        data.menuFlags = 0x800049D
        data.depth = 9
        Bool registered = UI.RegisterCustomMenu(WindowMenu, WindowFile, "root1", data)
        NQT_Native.Log("ToggleWindow: RegisterCustomMenu " + registered + ", flags " + data.menuFlags + ", depth " \
            + data.depth)
    EndIf
    RegisterForExternalEvent(WindowLogEvent, "OnWindowLog")
    RegisterForExternalEvent(WindowActionEvent, "OnWindowAction")
    Bool opened = UI.OpenMenu(WindowMenu)
    NQT_Native.Log("ToggleWindow: OpenMenu " + opened)
EndFunction

; The window's f4se.SendExternalEvent(WindowActionEvent, action, kind, quest, ref) on a click in a list (FormIDs of
; the row, see NearestQuestTracker.dll Window.h): "mark" marks the row's target like the track hotkey, "exclude" puts
; it on the exclusion list like the exclude hotkey (without marking the next one), "restore" takes it back. The window
; then reads its lists again (root1.Refresh).
Function OnWindowAction(String asAction, Int aiKind, Int aiQuestID, Int aiRefID)
    Setup()
    Cleanup()
    Quest q = None
    If aiQuestID != 0
        q = Game.GetForm(aiQuestID) as Quest
    EndIf
    Form target = q
    If aiKind == KindLocation || aiKind == KindInterior
        target = Game.GetForm(aiRefID)
    EndIf
    NQT_Native.Log("OnWindowAction: " + asAction + " kind " + aiKind + " " + q + " ref " + aiRefID + " -> " + target)
    If !target
        ; nothing to act on (a plugin no longer loaded)
    ElseIf asAction == "mark"
        Mark(aiKind, q, aiRefID, false)
    ElseIf asAction == "exclude"
        ExcludeTarget(aiKind, target)
    ElseIf asAction == "restore"
        Int index = ExcludedItems.Find(target)
        If index >= 0
            String name = ExcludedItemNames[index]
            RemoveExcludedAt(index)
            SyncExcluded()
            Notify(IsRussian(), "Возвращено: " + name, "Restored: " + name)
        EndIf
    EndIf
    If UI.IsMenuOpen(WindowMenu)
        UI.Invoke(WindowMenu, "root1.Refresh")
    EndIf
EndFunction

; From the item (Pip-Boy or favorites) and the MCM button: opens the window now, or once the Pip-Boy or the pause
; menu (MCM) closes - the window would be drawn under the pause menu.
Function OpenWindowWhenFree(String asFrom)
    Setup()
    If UI.IsMenuOpen(PipboyMenu) || UI.IsMenuOpen(PauseMenu)
        WindowPending = true
        RegisterForMenuOpenCloseEvent(PipboyMenu)
        RegisterForMenuOpenCloseEvent(PauseMenu)
        NQT_Native.Log("OpenWindowWhenFree(" + asFrom + "): waiting for the Pip-Boy / pause menu to close")
        Return
    EndIf
    NQT_Native.Log("OpenWindowWhenFree(" + asFrom + ")")
    If !UI.IsMenuOpen(WindowMenu)
        ToggleWindow()
    EndIf
EndFunction

Event OnMenuOpenCloseEvent(String asMenuName, Bool abOpening)
    If abOpening || !WindowPending || UI.IsMenuOpen(PipboyMenu) || UI.IsMenuOpen(PauseMenu)
        Return
    EndIf
    WindowPending = false
    UnregisterForMenuOpenCloseEvent(PipboyMenu)
    UnregisterForMenuOpenCloseEvent(PauseMenu)
    NQT_Native.Log("OnMenuOpenCloseEvent: " + asMenuName + " closed, opening the window")
    If !UI.IsMenuOpen(WindowMenu)
        ToggleWindow()
    EndIf
EndEvent

; MCM button "Open window".
Function OpenWindowFromMenu()
    OpenWindowWhenFree("MCM")
EndFunction

; Puts the item into the player's inventory if it is not there (abNotify: MCM button "Give the item", says so).
Function GiveWindowItem(Bool abNotify)
    Actor player = Game.GetPlayer()
    Bool russian = IsRussian()
    If player.GetItemCount(NQT_WindowItem) == 0
        player.AddItem(NQT_WindowItem, 1, !abNotify)
        NQT_Native.Log("GiveWindowItem: given")
    ElseIf abNotify
        Debug.Notification(Choose(russian, "Предмет «Nearest Quest Tracker» уже есть в инвентаре (Помощь)", \
            "The item \"Nearest Quest Tracker\" is already in the inventory (Aid)"))
    EndIf
EndFunction

; MCM button "Give the item".
Function GiveWindowItemFromMenu()
    Setup()
    GiveWindowItem(true)
EndFunction

; The window's f4se.SendExternalEvent(WindowLogEvent, text): what reached it (events, keys, clicks).
Function OnWindowLog(String asText)
    NQT_Native.Log("window: " + asText)
EndFunction

; ---------------------------------------------------------------------------------------------------
; MCM page (config.json). Like the restore hotkey, a restored target takes part in the search at once.

; "Excluded" dropdown: aiChoice is the picked element of ExcludedNames (0 = the hint).
Function RestoreFromMenu(Int aiChoice)
    Setup()
    MCMRestoreChoice = 0
    Int index = aiChoice - 1
    If index < 0 || index >= ExcludedItems.Length
        MCM.RefreshMenu()
        Return
    EndIf
    NQT_Native.Log("RestoreFromMenu: " + ExcludedItems[index] + " '" + ExcludedItemNames[index] + "'")
    RemoveExcludedAt(index)
    SyncExcluded()
    MCM.RefreshMenu()
    Track(false)
EndFunction

Function RestoreAll()
    RestoreKind(KindNone)
EndFunction

; Buttons "Restore active quests" / "new quests" / "locations" / "interiors": aiKind KindQuest / KindStart /
; KindLocation / KindInterior; KindNone = all.
Function RestoreKind(Int aiKind)
    Setup()
    Int count = 0
    Int i = ExcludedItems.Length - 1
    While i >= 0
        If aiKind == KindNone || ExcludedKinds[i] == aiKind
            RemoveExcludedAt(i)
            count += 1
        EndIf
        i -= 1
    EndWhile
    NQT_Native.Log("RestoreKind(" + aiKind + "): " + count + " restored")
    SyncExcluded()
    MCM.RefreshMenu()
    If count > 0
        Track(false)
    EndIf
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

; Marks the nearest non-excluded target of the kinds turned on (Sources): a Pip-Boy quest is tracked, a start
; point or a location gets its marker. With NQT_UntrackOthers every other marker goes (other Pip-Boy quests,
; the start marker, the location marker); without it only excluded quests are untracked. abAuto: called by the
; auto mode, which stays silent when nothing changes. Returns true if something is marked.
Bool Function Track(Bool abAuto)
    Cleanup()
    Bool russian = IsRussian()
    Int sources = Sources()
    If sources == 0
        NQT_Native.Log("Track(auto " + abAuto + "): no kinds of targets turned on")
        If !abAuto
            Notify(russian, "Ничего не выбрано — включите цели в настройках", "Nothing to pick - turn targets on in the settings")
        EndIf
        Return false
    EndIf
    Int[] found = NQT_Native.FindNearestTarget(NQT_ExcludedList, NQT_ExcludedLocationList, sources, NQT_AllLocations.GetValue() != 0.0)
    Quest q = None
    If found[1] != 0
        q = Game.GetForm(found[1]) as Quest
    EndIf
    NQT_Native.Log("Track(auto " + abAuto + ", sources " + sources + "): found kind " + found[0] + " " + q + " " + found[2])
    Return Mark(found[0], q, found[2], abAuto)
EndFunction

; Marks one target: aiKind KindQuest / KindNoMarker (akQuest is tracked), KindStart (akQuest's start marker goes on
; the ref aiMarkerID), KindLocation / KindInterior (the location marker goes on the map marker / door aiMarkerID).
; The rest as Track: NQT_UntrackOthers, LastKind / LastTracked, the notification. Returns true if something is marked.
Bool Function Mark(Int aiKind, Quest akQuest, Int aiMarkerID, Bool abAuto)
    Bool russian = IsRussian()
    Int kind = aiKind
    Quest q = akQuest
    ObjectReference marker = None
    If aiMarkerID != 0
        marker = Game.GetForm(aiMarkerID) as ObjectReference
    EndIf

    Bool alreadyTracked = false
    Bool shown = false
    If (kind == KindLocation || kind == KindInterior) && marker
        alreadyTracked = marker == ShownLocation && NQT_LocationQuest.IsObjectiveDisplayed(LocationObjective)
        shown = ShowLocation(marker, kind == KindInterior)
    ElseIf kind == KindStart && q
        alreadyTracked = q == ShownStart && NQT_StartQuest.IsActive()
        shown = ShowStart(q, aiMarkerID)
    ElseIf (kind == KindQuest || kind == KindNoMarker) && q
        alreadyTracked = q.IsActive()
        q.SetActive(true)
        shown = true
    EndIf
    If !shown
        kind = KindNone
        q = None
    EndIf

    Quest keep = None
    If kind == KindQuest || kind == KindNoMarker
        keep = q
    EndIf
    Int untracked = UntrackOthers(keep)
    If NQT_UntrackOthers.GetValue() != 0.0
        If kind != KindStart
            ClearStart()
        EndIf
        If kind != KindLocation && kind != KindInterior
            ClearLocation()
        EndIf
    EndIf
    LastKind = kind
    LastTracked = q

    If kind == KindNone
        NQT_Native.Log("Mark(auto " + abAuto + "): nothing to mark; untracked " + untracked)
        If !abAuto
            Notify(russian, "Нет подходящих целей", "No targets found")
        EndIf
        Return false
    EndIf

    String label
    If kind == KindLocation || kind == KindInterior
        label = LocationLabel(marker, ShownLocationNamed, ShownInterior, russian)
    ElseIf kind == KindStart
        label = StartLabel(q, russian)
    ElseIf kind == KindNoMarker
        label = Choose(russian, "Отслеживается: " + NQT_Native.GetQuestDisplayName(q) + " (без маркера)", \
            "Tracking: " + NQT_Native.GetQuestDisplayName(q) + " (no marker)")
    Else
        label = Choose(russian, "Отслеживается: ", "Tracking: ") + NQT_Native.GetQuestDisplayName(q)
    EndIf
    NQT_Native.Log("Mark(auto " + abAuto + "): kind " + kind + " " + q + " " + marker + " '" + label + "'; untracked " + untracked)
    If !abAuto || !alreadyTracked
        Notify(russian, label, label)
    EndIf
    Return true
EndFunction

Int Function Sources()
    Int sources = 0
    If NQT_IncludeQuests.GetValue() != 0.0
        sources += SourceQuests
        If NQT_IncludeNoMarker.GetValue() != 0.0
            sources += SourceNoMarker
        EndIf
    EndIf
    If NQT_IncludeStarts.GetValue() != 0.0
        sources += SourceStarts
    EndIf
    If NQT_IncludeRadiant.GetValue() != 0.0
        sources += SourceRadiant
    EndIf
    If NQT_IncludeLocations.GetValue() != 0.0
        sources += SourceLocations
    EndIf
    If NQT_IncludeInteriors.GetValue() != 0.0
        sources += SourceInteriors
        If NQT_SkipOwnedLocked.GetValue() != 0.0
            sources += SourceSkipOwnedLocked
        EndIf
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
; remove it once the quest has started or the giver is dead or disabled (the auto mode then marks the next).
Function PollStart()
    If !ShownStart
        Return
    EndIf
    Int markerID = NQT_Native.GetStartMarkerID(ShownStart)
    If markerID == 0
        NQT_Native.Log("PollStart: " + ShownStart + " started or no longer available")
        String closedBy = NQT_Native.GetStartClosedBy(ShownStart, IsRussian())
        If closedBy != ""
            Bool russianClosed = IsRussian()
            String closedName = StartName(ShownStart, russianClosed)
            Notify(russianClosed, "«" + closedName + "» недоступен: враждебна фракция «" + closedBy + "»", \
                closedName + " is unavailable: " + closedBy + " is hostile")
        ElseIf NQT_Native.IsStartGiverHostile(ShownStart)
            Bool russianHostile = IsRussian()
            String hostileName = StartName(ShownStart, russianHostile)
            Notify(russianHostile, "«" + hostileName + "» недоступен: квестодатель враждебен", hostileName + " is unavailable: the giver is hostile")
        ElseIf NQT_Native.IsStartNotOffered(ShownStart)
            Bool russian = IsRussian()
            String name = StartName(ShownStart, russian)
            Quest holder = Game.GetForm(NQT_Native.GetStartBlocker(ShownStart)) as Quest
            If holder
                String place = NQT_Native.GetStartBlockedPlace(ShownStart)
                String by = NQT_Native.GetQuestDisplayName(holder)
                If by == ""
                    by = NQT_Native.GetStartName(holder)
                EndIf
                Notify(russian, "«" + name + "» сейчас не предлагают: «" + place + "» держит квест «" + by + "»", \
                    name + " is not offered right now: " + place + " is held by " + by)
            Else
                Notify(russian, "«" + name + "» сейчас не предлагают — загляните позже", name + " is not offered right now - come back later")
            EndIf
        EndIf
        ClearStart()
        If NQT_AutoTrack.GetValue() != 0.0
            Track(true)
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

; ---------------------------------------------------------------------------------------------------
; Location marker

; Points LocationRef at the map marker (abInterior: the door into the interior) and shows the objective (again if
; the target changed, so that the HUD names the new one). Named: an interior, a location on the map (seen), or
; "All locations" is on.
Bool Function ShowLocation(ObjectReference akMarker, Bool abInterior)
    ReferenceAlias locationRef = NQT_LocationQuest.GetAlias(LocationRefAlias) as ReferenceAlias
    If !locationRef
        NQT_Native.Log("ShowLocation: no alias " + LocationRefAlias)
        Return false
    EndIf
    If !NQT_LocationQuest.IsRunning()
        NQT_LocationQuest.Start()
    EndIf
    Bool changed = akMarker != ShownLocation
    ShownLocation = akMarker
    ShownInterior = abInterior
    ShownLocationNamed = abInterior || NQT_AllLocations.GetValue() != 0.0 || akMarker.IsMapMarkerVisible()
    locationRef.ForceRefTo(akMarker)
    SetLocationText()
    If changed || !NQT_LocationQuest.IsObjectiveDisplayed(LocationObjective)
        NQT_LocationQuest.SetObjectiveDisplayed(LocationObjective, false)
        NQT_LocationQuest.SetObjectiveDisplayed(LocationObjective, true, true)
    EndIf
    NQT_LocationQuest.SetActive(true)
    CancelTimer(LocationPollTimer)
    StartTimer(LocationPollInterval, LocationPollTimer)
    Return true
EndFunction

Function ClearLocation()
    If !ShownLocation && !NQT_LocationQuest.IsObjectiveDisplayed(LocationObjective)
        Return
    EndIf
    NQT_Native.Log("ClearLocation: " + ShownLocation)
    CancelTimer(LocationPollTimer)
    ShownLocation = None
    ShownLocationNamed = false
    ShownInterior = false
    NQT_LocationQuest.SetObjectiveDisplayed(LocationObjective, false)
    ReferenceAlias locationRef = NQT_LocationQuest.GetAlias(LocationRefAlias) as ReferenceAlias
    If locationRef
        locationRef.Clear()
    EndIf
    NQT_LocationQuest.SetActive(false)
EndFunction

; While the location marker is shown: remove it once the location is discovered (the game says so itself), the
; interior entered, or the marker or door disabled, and with the auto mode mark the next target; name a location
; once it appears on the map.
Function PollLocation()
    If !ShownLocation
        Return
    EndIf
    Bool done
    If ShownInterior
        done = NQT_Native.IsInteriorVisited(ShownLocation)
    Else
        done = ShownLocation.CanFastTravelToMarker()
    EndIf
    If done || ShownLocation.IsDisabled()
        NQT_Native.Log("PollLocation: " + ShownLocation + " discovered, entered or disabled (interior " + ShownInterior + ")")
        ClearLocation()
        If NQT_AutoTrack.GetValue() != 0.0
            Track(true)
        EndIf
        Return
    EndIf
    If !ShownLocationNamed && ShownLocation.IsMapMarkerVisible()
        ShownLocationNamed = true
        SetLocationText()
    EndIf
    StartTimer(LocationPollInterval, LocationPollTimer)
EndFunction

Function SetLocationText()
    NQT_Native.SetObjectiveText(NQT_LocationQuest, LocationObjective, LocationLabel(ShownLocation, ShownLocationNamed, ShownInterior, IsRussian()))
EndFunction

; Objective text and notification: "Location: <name>", or "Undiscovered location: <icon type>" for a location
; not on the map (the compass shows only its icon); "Interior: <name>" for the door into an interior.
String Function LocationLabel(ObjectReference akMarker, Bool abNamed, Bool abInterior, Bool abRussian)
    If abInterior
        String interior = NQT_Native.GetInteriorName(akMarker)
        If interior != ""
            Return Choose(abRussian, "Интерьер: ", "Interior: ") + interior
        EndIf
        Return Choose(abRussian, "Интерьер", "Interior")
    EndIf
    If abNamed
        String name = NQT_Native.GetMapMarkerName(akMarker)
        If name != ""
            Return Choose(abRussian, "Локация: ", "Location: ") + name
        EndIf
    EndIf
    String type = NQT_Native.GetMapMarkerTypeName(akMarker)
    If type != ""
        Return Choose(abRussian, "Неоткрытая локация: ", "Undiscovered location: ") + type
    EndIf
    Return Choose(abRussian, "Неоткрытая локация", "Undiscovered location")
EndFunction

; Name for the exclusion list and notifications: the location's name, or "undiscovered location (<type>)"; the
; interior's name for a door.
String Function LocationName(ObjectReference akMarker, Bool abNamed, Bool abInterior, Bool abRussian)
    String name = ""
    If abInterior
        name = NQT_Native.GetInteriorName(akMarker)
        If name == ""
            name = Choose(abRussian, "интерьер", "interior")
        EndIf
        Return name
    EndIf
    If abNamed
        name = NQT_Native.GetMapMarkerName(akMarker)
    EndIf
    If name != ""
        Return name
    EndIf
    name = Choose(abRussian, "неоткрытая локация", "undiscovered location")
    String type = NQT_Native.GetMapMarkerTypeName(akMarker)
    If type != ""
        name += " (" + type + ")"
    EndIf
    Return name
EndFunction

; ---------------------------------------------------------------------------------------------------

; Untracks the Pip-Boy quests other than akKeep: all of them if NQT_UntrackOthers is on, otherwise only
; the excluded ones. Returns how many were untracked.
Int Function UntrackOthers(Quest akKeep)
    Bool all = NQT_UntrackOthers.GetValue() != 0.0
    Quest[] quests = NQT_Native.GetTrackedQuestList()
    Int count = 0
    Int i = 0
    While i < quests.Length
        Quest q = quests[i]
        If q && q != akKeep && q.IsActive() && (all || NQT_ExcludedList.HasForm(q))
            q.SetActive(false)
            count += 1
        EndIf
        i += 1
    EndWhile
    Return count
EndFunction

; What the exclude hotkey means: the target the mod marked last, if it is still marked; otherwise the nearest of
; the tracked Pip-Boy quests; otherwise a start or location marker still shown. Returns its kind (the target is
; LastTracked, ShownStart or ShownLocation; KindLocation or KindInterior for the latter); KindNone if nothing is
; marked.
Int Function PickToExclude()
    Bool startShown = ShownStart && NQT_StartQuest.IsActive() && !NQT_ExcludedList.HasForm(ShownStart)
    Bool locationShown = ShownLocation && NQT_LocationQuest.IsObjectiveDisplayed(LocationObjective)
    If (LastKind == KindLocation || LastKind == KindInterior) && locationShown
        Return ShownLocationKind()
    EndIf
    If LastKind == KindStart && startShown && LastTracked == ShownStart
        Return KindStart
    EndIf
    Quest[] quests = NQT_Native.GetTrackedQuestList()
    If (LastKind == KindQuest || LastKind == KindNoMarker) && LastTracked && LastTracked.IsActive() \
            && quests.Find(LastTracked) >= 0 && !NQT_ExcludedList.HasForm(LastTracked)
        Return KindQuest
    EndIf
    Quest best = None
    Float bestDistance = 0.0
    Int i = 0
    While i < quests.Length
        Quest q = quests[i]
        If q && q.IsActive() && !NQT_ExcludedList.HasForm(q)
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
    If best
        LastTracked = best
        Return KindQuest
    EndIf
    If startShown
        LastTracked = ShownStart
        Return KindStart
    EndIf
    If locationShown
        Return ShownLocationKind()
    EndIf
    Return KindNone
EndFunction

Int Function ShownLocationKind()
    If ShownInterior
        Return KindInterior
    EndIf
    Return KindLocation
EndFunction

; ---------------------------------------------------------------------------------------------------
; Exclusion list

Function AddExcluded(Form akItem, String asName, Int aiKind)
    ExcludedItems.Add(akItem)
    ExcludedItemNames.Add(asName)
    ExcludedKinds.Add(aiKind)
EndFunction

Function RemoveExcludedAt(Int aiIndex)
    ExcludedItems.Remove(aiIndex)
    ExcludedItemNames.Remove(aiIndex)
    ExcludedKinds.Remove(aiIndex)
EndFunction

; Done targets leave the exclusion list (the exclusion was for that run of the quest; a discovered location or an
; entered interior has nothing left to skip). A quest with a start point (QuestStarts.json) is often not running before it starts, so
; only completion counts there.
Function Cleanup()
    Bool changed = false
    Int i = ExcludedItems.Length - 1
    While i >= 0
        Bool done
        If ExcludedKinds[i] == KindLocation
            ObjectReference marker = ExcludedItems[i] as ObjectReference
            done = !marker || marker.CanFastTravelToMarker()
        ElseIf ExcludedKinds[i] == KindInterior
            done = NQT_Native.IsInteriorVisited(ExcludedItems[i] as ObjectReference)
        Else
            Quest q = ExcludedItems[i] as Quest
            done = !q || q.IsCompleted() || (!q.IsRunning() && !NQT_Native.HasQuestStart(q))
        EndIf
        If done
            NQT_Native.Log("Cleanup: " + ExcludedItems[i] + " '" + ExcludedItemNames[i] + "' is done")
            RemoveExcludedAt(i)
            changed = true
        EndIf
        i -= 1
    EndWhile
    If changed
        SyncExcluded()
    EndIf
EndFunction

; Copies ExcludedItems to the FormLists the DLL reads and rebuilds the MCM dropdown list.
Function SyncExcluded()
    NQT_ExcludedList.Revert()
    NQT_ExcludedLocationList.Revert()
    Bool russian = IsRussian()
    ExcludedNames = new String[1]
    If ExcludedItems.Length == 0
        ExcludedNames[0] = Choose(russian, "(пусто)", "(empty)")
    Else
        ExcludedNames[0] = Choose(russian, "— выберите —", "— pick one —")
    EndIf
    Int i = 0
    While i < ExcludedItems.Length
        String prefix
        If ExcludedKinds[i] == KindLocation
            NQT_ExcludedLocationList.AddForm(ExcludedItems[i])
            prefix = Choose(russian, "Локация: ", "Location: ")
        ElseIf ExcludedKinds[i] == KindInterior
            NQT_ExcludedLocationList.AddForm(ExcludedItems[i])  ; the DLL skips the door's interior
            prefix = Choose(russian, "Интерьер: ", "Interior: ")
        Else
            NQT_ExcludedList.AddForm(ExcludedItems[i])
            If ExcludedKinds[i] == KindStart
                prefix = Choose(russian, "Новый квест: ", "New quest: ")
            Else
                prefix = Choose(russian, "Квест: ", "Quest: ")
            EndIf
        EndIf
        ExcludedNames.Add(NQT_Native.Shorten(prefix + ExcludedItemNames[i], MenuNameLength))  ; MCM shrinks long lines
        i += 1
    EndWhile
EndFunction

; ---------------------------------------------------------------------------------------------------

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
        HudDump("Notify")   ; DIAG 2026-10-13: also the auto mode's "marked ..." (in dialogue, after a quest ends)
        Debug.Notification(Choose(abRussian, asRussian, asEnglish))
    EndIf
EndFunction
