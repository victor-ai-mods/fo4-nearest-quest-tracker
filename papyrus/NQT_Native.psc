Scriptname NQT_Native Native Hidden
{Native functions of NearestQuestTracker.dll (F4SE plugin).}

; Quests in the Pip-Boy list: at least one objective displayed and not completed.
Quest[] Function GetTrackedQuestList() native global

; The nearest target: aiSources is a sum of 1 = targets of Pip-Boy quests, 2 = start points of quests not
; started yet (QuestStarts.json), 4 = Pip-Boy quests without a marker (only together with 1), 8 = map locations
; not discovered yet (abAllLocations false: only those on the map or the compass), 16 = interiors never entered
; (Pip-Boy local map fog of war; the nearest door from outside). Quests in akExcludedQuests and map markers in
; akExcludedLocations are skipped; a door in akExcludedLocations skips its interior. Order: by world - the player's world, then the Commonwealth, Far
; Harbor, Nuka-World, other worlds; within a world all kinds mixed by distance, then quests without a marker.
; Distance follows the engine's quest-marker path through doors (start points and locations: straight line on the
; world map, to the edge of a location's discovery radius); a target in another worldspace loses to any reachable
; one. Result: [kind, quest FormID, marker ref FormID]; kind: 0 none, 1 Pip-Boy quest, 2 Pip-Boy quest without a
; marker, 3 start point, 4 location, 5 interior; quest is 0 for a location or interior; the marker ref is where the
; start, location or interior marker goes (the door for an interior; 0 for a Pip-Boy quest).
; FormIDs and not forms: a Quest returned by the DLL arrives as None (see Papyrus.cpp), so get the forms with
; Game.GetForm(id).
Int[] Function FindNearestTarget(FormList akExcludedQuests, FormList akExcludedLocations, Int aiSources, Bool abAllLocations) native global

; Where the start marker of akQuest goes now: the quest giver once it is in memory, else a door or map marker
; on the way to it. 0 if akQuest has no start point or it is no longer available (quest started or completed,
; giver dead or disabled, a required quest not completed).
Int Function GetStartMarkerID(Quest akQuest) native global

; True if akQuest has no available start point because its giver is near but does not offer it now: the quest
; must already be running for the giver to offer it, and the Story Manager has not started it (often another
; quest holds the location). Stays true while the player stays in that place.
Bool Function IsStartNotOffered(Quest akQuest) native global

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

; Texts of akQuest's displayed (not completed) objectives as the Pip-Boy shows them, aliases resolved; repeated
; texts once. Empty if none.
String[] Function GetObjectiveTexts(Quest akQuest) native global

; asText cut to aiMaxChars characters (not bytes), with "…" at the end if it was longer.
String Function Shorten(String asText, Int aiMaxChars) native global

; sLanguage from the game INI in lower case ("en", "ru", ...); "en" if not set.
String Function GetLanguage() native global

; Hash of the displayed (active, not completed) objectives of all quests. It changes when an objective
; is completed or failed or a new one appears (a quest started or advanced); used by the auto mode.
Int Function GetObjectiveSignature() native global

; Debug: writes all quest targets, objective states and distances, and all quest start points with their
; state, and the category of each Pip-Boy quest, to NearestQuestTracker.log.
; Console: cgf "NQT_Native.DumpQuestTargets"
Function DumpQuestTargets() native global

; Debug: writes every map marker of every worldspace with its state (unknown / seen / visited), location and
; distance to NearestQuestTracker.log.
; Console: cgf "NQT_Native.DumpMapMarkers"
Function DumpMapMarkers() native global

; Debug: writes the Pip-Boy local map "fog of war" of every interior (seen sections and bits) and the interiors
; that worldspace doors lead into, nearest first, to NearestQuestTracker.log.
; Console: cgf "NQT_Native.DumpInteriors"
Function DumpInteriors() native global

; The name of a map marker as the Pip-Boy map shows it; "" if akMarker is not a map marker.
String Function GetMapMarkerName(ObjectReference akMarker) native global

; The marker's icon type in the game language (e.g. "Vault" / "Убежище"); "" if not known (a mod's type).
String Function GetMapMarkerTypeName(ObjectReference akMarker) native global

; Name of the interior akDoor leads into, as the door prompt shows it; "" if none.
String Function GetInteriorName(ObjectReference akDoor) native global

; True if the player has been inside the interior akDoor leads into (the Pip-Boy local map has uncovered any of
; it), or akDoor leads into no interior.
Bool Function IsInteriorVisited(ObjectReference akDoor) native global

; Category of the quest in the quest tree (QuestCategories.json from the wiki tables, QuestCategories-user.json):
; a key like "faction.bos.side"; quests the files do not list get "other.<plugin file name>". "" for None.
String Function GetQuestCategory(Quest akQuest) native global

; Name of a category key in the game language ("Побочные квесты" / "Side quests"); for "other.<plugin>" the plugin
; file name, for "other" "Другие квесты" / "Other quests"; "" if the key is unknown.
String Function GetQuestCategoryName(String asKey) native global
