Scriptname NQT_OpenWindowEffect extends ActiveMagicEffect
{Effect of the item "Nearest Quest Tracker" (NQT_WindowItem): opens the mod's window, like the MCM hotkey; with a
gamepad the item in the favorites is the way to open it. The item goes back into the inventory.}

NQT_MainScript Property Main Auto Const Mandatory
Potion Property SelfItem Auto Const Mandatory

Event OnEffectStart(Actor akTarget, Actor akCaster)
    If akTarget == Game.GetPlayer()
        akTarget.AddItem(SelfItem, 1, true)
        Main.OpenWindowWhenFree("item")
    EndIf
EndEvent
