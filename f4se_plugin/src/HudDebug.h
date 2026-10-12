#pragma once

namespace NQT
{
	// Diagnostics (2026-10-13): the user's top-left HUD messages stop until a save is loaded, maybe after the
	// mod's hotkeys. Writes the state of the vanilla message queue of HUDMenu
	// (root.HUDNotificationsGroup_mc.Messages_mc, class Messages in HUDMenu.swf) to the log on the UI thread:
	// queued and shown message counts, CanAddMessage, and every child with its position, height, frame and text.
	// a_tag names the call in the log.
	void DumpHudMessages(std::string a_tag);
}
