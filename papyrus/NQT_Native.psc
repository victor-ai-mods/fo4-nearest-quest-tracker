Scriptname NQT_Native Native Hidden
{Native functions of NearestQuestTracker.dll (F4SE plugin).}

; Quests in the Pip-Boy list: at least one objective displayed and not completed.
Quest[] Function GetTrackedQuestList() native global

; The nearest target: aiSources is a sum of 1 = targets of Pip-Boy quests, 2 = start points of quests not
; started yet (QuestStarts.json), 4 = Pip-Boy quests without a marker (only together with 1). Quests in
; akExcluded are skipped. Order: by world - the player's world, then the Commonwealth, Far Harbor, Nuka-World,
; other worlds; within a world targets by distance, then quests without a marker. Distance follows the engine's
; quest-marker path through doors (start points: straight line on the world map); a target in another
; worldspace loses to any reachable one. Result: [quest FormID, marker ref FormID, flags]; the marker ref is 0
; for a Pip-Boy quest and is where the start marker goes for a start point; flags: 1 = the quest has no marker;
; [0, 0, 0] if nothing is found.
; FormIDs and not forms: a Quest returned by the DLL arrives as None (see Papyrus.cpp), so get the forms with
; Game.GetForm(id).
Int[] Function FindNearestTarget(FormList akExcluded, Int aiSources) native global

; Where the start marker of akQuest goes now: the quest giver once it is in memory, else a door or map marker
; on the way to it. 0 if akQuest has no start point or it is no longer available (quest started or completed,
; giver dead or disabled, a required quest not completed).
Int Function GetStartMarkerID(Quest akQuest) native global

; True if QuestStarts.json (or QuestStarts-user.json) has a start point for akQuest.
Bool Function HasQuestStart(Quest akQuest) native global

; Name of a quest not started yet (its full name without <...> tags); "" if it has none.
String Function GetStartName(Quest akQuest) native global

; True if akQuest starts when the signal of a radio transmitter is picked up (the start point is the transmitter).
Bool Function IsRadioStart(Quest akQuest) native global

; Station name of akQuest's radio start as the Pip-Boy radio list shows it; "" if none or not known.
String Function GetStartRadioName(Quest akQuest) native global

; Replaces the text of objective aiObjective of akQuest (the start marker quest). Not saved with the game.
Bool Function SetObjectiveText(Quest akQuest, Int aiObjective, String asText) native global

; Writes "script: <asText>" to NearestQuestTracker.log (Papyrus logging not needed).
Function Log(String asText) native global

; Distance in game units to the nearest target of akQuest.
; -1.0: the quest has no target with a position; -2.0: the target is in another worldspace.
Float Function GetQuestDistance(Quest akQuest) native global

; The name the Pip-Boy shows for akQuest, aliases resolved. For a Miscellaneous quest: the text of
; the displayed objective that owns the nearest target. "" if the quest has no displayed objective.
String Function GetQuestDisplayName(Quest akQuest) native global

; asText cut to aiMaxChars characters (not bytes), with "…" at the end if it was longer.
String Function Shorten(String asText, Int aiMaxChars) native global

; sLanguage from the game INI in lower case ("en", "ru", ...); "en" if not set.
String Function GetLanguage() native global

; Hash of the displayed (active, not completed) objectives of all quests. It changes when an objective
; is completed or failed or a new one appears (a quest started or advanced); used by the auto mode.
Int Function GetObjectiveSignature() native global

; Debug: writes all quest targets, objective states and distances, and all quest start points with their
; state, to NearestQuestTracker.log.
; Console: cgf "NQT_Native.DumpQuestTargets"
Function DumpQuestTargets() native global
