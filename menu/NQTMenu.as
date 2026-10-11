package
{
	import flash.display.MovieClip;
	import flash.display.Shape;
	import flash.display.Sprite;
	import flash.events.Event;
	import flash.events.KeyboardEvent;
	import flash.events.MouseEvent;
	import flash.text.TextField;
	import flash.text.TextFieldAutoSize;
	import flash.text.TextFormat;
	import flash.text.TextFormatAlign;

	// Nearest Quest Tracker: своё окно (этап 3a — «пустое» окно с кнопкой «Закрыть»).
	// Меню регистрирует NQT_MainScript через F4SE UI.RegisterCustomMenu(MENU_NAME, "NearestQuestTracker", "root1", …):
	// этот класс — корень SWF (root1) и объект меню. Движок кладёт в BGSCodeObj функции F4SE CustomMenu
	// (PlaySound, OpenMenu, CloseMenu) и зовёт ProcessUserEvent с событиями управления меню.
	// F4SE отдаёт свой объект в onF4SEObjCreated; через f4se.SendExternalEvent("NQT_MenuLog", …) окно пишет
	// в NearestQuestTracker.log (NQT_MainScript.OnMenuLog) — так видно, какие события доходят до окна.
	// Окно рисуется светлыми тонами: F4SE (extendedFlags «наследовать цвет») красит меню в цвет интерфейса игрока.
	public class NQTMenu extends MovieClip
	{
		public static const MENU_NAME:String = "NQT_Menu";
		private static const LOG_EVENT:String = "NQT_MenuLog";

		private static const WIDTH:Number = 1280;
		private static const HEIGHT:Number = 720;
		private static const PANEL_W:Number = 640;
		private static const PANEL_H:Number = 300;
		private static const BUTTON_W:Number = 200;
		private static const BUTTON_H:Number = 44;

		public var BGSCodeObj:Object;
		private var f4se:Object;
		private var closeButton:Sprite;
		private var closeFrame:Shape;
		private var closing:Boolean = false;

		public function NQTMenu()
		{
			super();
			BGSCodeObj = new Object();
			Build();
			addEventListener(Event.ADDED_TO_STAGE, OnAddedToStage);
		}

		// F4SE: объект f4se готов (после загрузки SWF).
		public function onF4SEObjCreated(obj:Object):void
		{
			f4se = obj;
			Log("onF4SEObjCreated; BGSCodeObj: " + Members(BGSCodeObj));
		}

		// Движок: событие управления меню ("Cancel", "Accept", …), abPressed — нажато / отпущено.
		public function ProcessUserEvent(asEvent:String, abPressed:Boolean):Boolean
		{
			Log("ProcessUserEvent " + asEvent + " " + abPressed);
			if (!abPressed && asEvent == "Cancel")
			{
				Close("Cancel");
				return true;
			}
			return false;
		}

		private function OnAddedToStage(e:Event):void
		{
			stage.addEventListener(KeyboardEvent.KEY_DOWN, OnKeyDown);
			Log("added to stage " + stage.stageWidth + "x" + stage.stageHeight);
		}

		private function OnKeyDown(e:KeyboardEvent):void
		{
			Log("key down " + e.keyCode);
			if (e.keyCode == 27)    // Esc
			{
				Close("Esc");
			}
		}

		private function Close(asWhy:String):void
		{
			if (closing)
			{
				return;
			}
			closing = true;
			Log("close (" + asWhy + ")");
			try
			{
				BGSCodeObj.PlaySound("UIMenuCancel");
				BGSCodeObj.CloseMenu(MENU_NAME);
			}
			catch (err:Error)
			{
				closing = false;
				Log("CloseMenu failed: " + err.message);
			}
		}

		private function Build():void
		{
			// Затемнение с запасом за края: на экранах шире 16:9 меню растягивается с полями.
			var dim:Shape = new Shape();
			dim.graphics.beginFill(0x000000, 0.6);
			dim.graphics.drawRect(-WIDTH, -HEIGHT, WIDTH * 3, HEIGHT * 3);
			dim.graphics.endFill();
			addChild(dim);

			var panel:Sprite = new Sprite();
			panel.x = (WIDTH - PANEL_W) / 2;
			panel.y = (HEIGHT - PANEL_H) / 2;
			panel.graphics.beginFill(0x000000, 0.85);
			panel.graphics.lineStyle(2, 0xFFFFFF, 1);
			panel.graphics.drawRect(0, 0, PANEL_W, PANEL_H);
			panel.graphics.endFill();
			addChild(panel);

			var title:TextField = MakeField(30, TextFormatAlign.CENTER);
			title.text = "$NQT_MENU_TITLE";
			title.width = PANEL_W;
			title.y = 28;
			panel.addChild(title);

			var line:Shape = new Shape();
			line.graphics.lineStyle(1, 0xFFFFFF, 0.6);
			line.graphics.moveTo(40, 80);
			line.graphics.lineTo(PANEL_W - 40, 80);
			panel.addChild(line);

			var text:TextField = MakeField(20, TextFormatAlign.CENTER);
			text.text = "$NQT_MENU_EMPTY";
			text.width = PANEL_W - 80;
			text.x = 40;
			text.y = 110;
			panel.addChild(text);

			closeButton = new Sprite();
			closeButton.x = (PANEL_W - BUTTON_W) / 2;
			closeButton.y = PANEL_H - BUTTON_H - 32;
			closeButton.buttonMode = true;
			closeButton.useHandCursor = true;
			closeButton.mouseChildren = false;
			closeFrame = new Shape();
			closeButton.addChild(closeFrame);
			var label:TextField = MakeField(22, TextFormatAlign.CENTER);
			label.text = "$NQT_MENU_CLOSE";
			label.width = BUTTON_W;
			label.y = (BUTTON_H - label.height) / 2;
			closeButton.addChild(label);
			DrawButton(false);
			closeButton.addEventListener(MouseEvent.ROLL_OVER, OnButtonOver);
			closeButton.addEventListener(MouseEvent.ROLL_OUT, OnButtonOut);
			closeButton.addEventListener(MouseEvent.CLICK, OnButtonClick);
			panel.addChild(closeButton);
		}

		private function DrawButton(abHover:Boolean):void
		{
			closeFrame.graphics.clear();
			closeFrame.graphics.beginFill(0xFFFFFF, abHover ? 0.3 : 0.08);
			closeFrame.graphics.lineStyle(2, 0xFFFFFF, 1);
			closeFrame.graphics.drawRect(0, 0, BUTTON_W, BUTTON_H);
			closeFrame.graphics.endFill();
		}

		private function OnButtonOver(e:MouseEvent):void
		{
			DrawButton(true);
			try
			{
				BGSCodeObj.PlaySound("UIMenuFocus");
			}
			catch (err:Error)
			{
			}
		}

		private function OnButtonOut(e:MouseEvent):void
		{
			DrawButton(false);
		}

		private function OnButtonClick(e:MouseEvent):void
		{
			Log("button click");
			Close("button");
		}

		private function MakeField(size:Number, align:String):TextField
		{
			var tf:TextField = new TextField();
			tf.embedFonts = true;
			tf.selectable = false;
			tf.mouseEnabled = false;
			tf.multiline = true;
			tf.wordWrap = true;
			tf.autoSize = TextFieldAutoSize.LEFT;
			var fmt:TextFormat = new TextFormat("$MAIN_Font", size, 0xFFFFFF);
			fmt.align = align;
			tf.defaultTextFormat = fmt;
			return tf;
		}

		private function Log(s:String):void
		{
			if (f4se == null)
			{
				return;
			}
			try
			{
				f4se.SendExternalEvent(LOG_EVENT, s);
			}
			catch (err:Error)
			{
			}
		}

		private static function Members(o:Object):String
		{
			if (o == null)
			{
				return "null";
			}
			var names:Array = [];
			for (var k:String in o)
			{
				names.push(k);
			}
			return names.length ? names.join(", ") : "(none)";
		}
	}
}
