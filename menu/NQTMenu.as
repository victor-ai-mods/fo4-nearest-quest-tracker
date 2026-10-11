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
	import flash.utils.getTimer;

	// Nearest Quest Tracker: своё окно. Этап 3b — списки квестов и локаций из DLL, действия над строкой.
	// Меню регистрирует NQT_MainScript через F4SE UI.RegisterCustomMenu(MENU_NAME, "NearestQuestTracker", "root1", …):
	// этот класс — корень SWF (root1) и объект меню. Движок кладёт в BGSCodeObj функции F4SE CustomMenu
	// (PlaySound, OpenMenu, CloseMenu) и зовёт ProcessUserEvent с событиями управления меню.
	// Данные: F4SE отдаёт свой объект в onF4SEObjCreated, в нём f4se.plugins.NearestQuestTracker — функции DLL
	// (f4se_plugin/src/Window.h: GetLanguage, GetQuests, GetLocations, GetCategories; строки — объекты с полями
	// kind, quest, ref, name, world, worldRank, dist, tracked, excluded, …), уже в порядке хоткея «Отметить ближайшее».
	// Действия: f4se.SendExternalEvent(ACTION_EVENT, "mark" | "exclude" | "restore", kind, quest, ref) ->
	// NQT_MainScript.OnWindowAction, тот делает то же, что хоткеи, и зовёт root1.Refresh (UI.Invoke).
	// Лог: f4se.SendExternalEvent(LOG_EVENT, текст) -> строки "script: window: …" в NearestQuestTracker.log.
	// Окно рисуется светлыми тонами: F4SE (extendedFlags «наследовать цвет») красит меню в цвет интерфейса игрока.
	public class NQTMenu extends MovieClip
	{
		public static const MENU_NAME:String = "NQT_Menu";
		private static const LOG_EVENT:String = "NQT_MenuLog";
		private static const ACTION_EVENT:String = "NQT_MenuAction";

		private static const WIDTH:Number = 1280;
		private static const HEIGHT:Number = 720;
		private static const PANEL_W:Number = 1120;
		private static const PANEL_H:Number = 640;
		private static const BUTTON_H:Number = 40;
		private static const ROWS:int = 13;
		private static const ROW_H:Number = 32;
		private static const LIST_X:Number = 30;
		private static const LIST_Y:Number = 150;
		private static const LIST_W:Number = PANEL_W - 60;
		// Колонки строки: название, мир, расстояние, состояние, кнопка действия.
		private static const COL_NAME:Number = 10;
		private static const COL_WORLD:Number = 520;
		private static const COL_DIST:Number = 680;
		private static const COL_STATE:Number = 790;
		private static const COL_ACTION:Number = LIST_W - 110;
		// Игровые единицы -> метры (1 единица = 1,428 см).
		private static const METERS_PER_UNIT:Number = 0.01428;
		// Сколько кадров ждать root1.Refresh от скрипта после действия, прежде чем обновиться самим.
		private static const REFRESH_WAIT_FRAMES:int = 90;

		// Kind* из NQT_MainScript (поле kind строки).
		private static const KIND_NONE:int = 0;
		private static const KIND_LOCATION:int = 4;
		private static const KIND_INTERIOR:int = 5;

		public var BGSCodeObj:Object;
		private var f4se:Object;
		private var plugin:Object;
		private var russian:Boolean = true;
		private var quests:Array = [];
		private var locations:Array = [];   // только замеченные (seen): «все» — фильтр этапа 3d
		private var locationsAll:int = 0;
		private var tab:int = 0;            // 0 квесты, 1 локации
		private var scroll:int = 0;
		private var rows:Array = [];        // ROWS объектов {sprite, back, name, world, dist, state, button, buttonBack, buttonText, data}
		private var tabButtons:Array = [];
		private var status:TextField;
		private var scrollInfo:TextField;
		private var header:Array = [];
		private var closeButton:Sprite;
		private var closeFrame:Shape;
		private var closing:Boolean = false;
		private var waitFrames:int = 0;

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
			plugin = f4se && f4se.plugins ? f4se.plugins.NearestQuestTracker : null;
			Log("onF4SEObjCreated; plugin " + (plugin ? Members(plugin) : "not found"));
			if (plugin)
			{
				try
				{
					russian = plugin.GetLanguage() == "ru";
				}
				catch (err:Error)
				{
					Log("GetLanguage failed: " + err.message);
				}
			}
			Refresh();
		}

		// Перечитать списки из DLL (расстояния считаются заново). Зовёт и NQT_MainScript после действия (UI.Invoke).
		public function Refresh():void
		{
			waitFrames = 0;
			if (!plugin)
			{
				SetStatus(T("Нет NearestQuestTracker.dll (f4se.plugins)", "NearestQuestTracker.dll not found (f4se.plugins)"));
				return;
			}
			var t0:int = getTimer();
			try
			{
				quests = plugin.GetQuests();
				var t1:int = getTimer();
				var all:Array = plugin.GetLocations();
				var t2:int = getTimer();
				locationsAll = all.length;
				locations = [];
				for (var i:int = 0; i < all.length; i++)
				{
					if (all[i].seen)
					{
						locations.push(all[i]);
					}
				}
				Log("Refresh: " + quests.length + " quests (" + (t1 - t0) + " ms), " + locationsAll + " locations, "
					+ locations.length + " seen (" + (t2 - t1) + " ms)" + (quests.length ? "; first quest " + Describe(quests[0]) : "")
					+ (locations.length ? "; first location " + Describe(locations[0]) : ""));
				SetStatus("");
			}
			catch (err:Error)
			{
				Log("Refresh failed: " + err.message);
				SetStatus(T("Ошибка чтения списков: ", "Could not read the lists: ") + err.message);
			}
			ShowList();
		}

		// Движок: событие управления меню ("Cancel", "Accept", …), abPressed — нажато / отпущено.
		public function ProcessUserEvent(asEvent:String, abPressed:Boolean):Boolean
		{
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
			stage.addEventListener(MouseEvent.MOUSE_WHEEL, OnWheel);
			addEventListener(Event.ENTER_FRAME, OnFrame);
		}

		// Скрипт не ответил на действие (root1.Refresh не пришёл) — обновиться самим; в логе видно, что так вышло.
		private function OnFrame(e:Event):void
		{
			if (waitFrames > 0 && --waitFrames == 0)
			{
				Log("no Refresh from the script after an action: refreshing by timer");
				Refresh();
			}
		}

		private function OnKeyDown(e:KeyboardEvent):void
		{
			if (e.keyCode == 27)    // Esc
			{
				Close("Esc");
			}
			else if (e.keyCode == 38)   // вверх
			{
				ScrollBy(-1);
			}
			else if (e.keyCode == 40)   // вниз
			{
				ScrollBy(1);
			}
			else if (e.keyCode == 33)   // PageUp
			{
				ScrollBy(-ROWS);
			}
			else if (e.keyCode == 34)   // PageDown
			{
				ScrollBy(ROWS);
			}
		}

		private function OnWheel(e:MouseEvent):void
		{
			ScrollBy(e.delta > 0 ? -3 : 3);
		}

		private function ScrollBy(aiRows:int):void
		{
			var list:Array = CurrentList();
			var max:int = Math.max(0, list.length - ROWS);
			var next:int = Math.max(0, Math.min(max, scroll + aiRows));
			if (next != scroll)
			{
				scroll = next;
				ShowList();
			}
		}

		private function CurrentList():Array
		{
			return tab == 0 ? quests : locations;
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

		// ---------------------------------------------------------------------------------------------------
		// Список

		private function ShowList():void
		{
			var list:Array = CurrentList();
			scroll = Math.max(0, Math.min(scroll, list.length - ROWS));
			for (var i:int = 0; i < ROWS; i++)
			{
				var row:Object = rows[i];
				var data:Object = scroll + i < list.length ? list[scroll + i] : null;
				row.data = data;
				row.sprite.visible = data != null;
				if (!data)
				{
					continue;
				}
				SetText(row.name, (data.tracked ? "» " : "") + RowName(data));
				SetText(row.world, data.world);
				SetText(row.dist, DistanceText(data.dist));
				SetText(row.state, RowState(data));
				row.sprite.alpha = data.excluded ? 0.45 : 1.0;
				row.button.visible = data.kind != KIND_NONE;
				SetText(row.buttonText, data.excluded ? T("Вернуть", "Restore") : T("Исключить", "Exclude"));
				DrawRow(row, false);
				DrawRowButton(row, false);
			}
			if (list.length == 0)
			{
				SetText(scrollInfo, T("Пусто", "Empty"));
			}
			else
			{
				SetText(scrollInfo, (scroll + 1) + "–" + Math.min(list.length, scroll + ROWS) + T(" из ", " of ") + list.length);
			}
			SetText(tabButtons[0].text, T("Квесты", "Quests") + " (" + quests.length + ")");
			SetText(tabButtons[1].text, T("Локации", "Locations") + " (" + locations.length + "/" + locationsAll + ")");
			for (var t:int = 0; t < tabButtons.length; t++)
			{
				DrawTab(tabButtons[t], false);
			}
			SetText(header[1], T("Мир", "World"));
			SetText(header[2], T("Расстояние", "Distance"));
			SetText(header[3], tab == 0 ? T("Состояние", "State") : T("Тип", "Type"));
			SetText(header[0], tab == 0 ? T("Квест (клик — отметить)", "Quest (click to mark)")
				: T("Локация (клик — отметить)", "Location (click to mark)"));
		}

		private function RowName(data:Object):String
		{
			if (data.kind == KIND_INTERIOR)
			{
				return data.name != "" ? data.name : T("интерьер", "interior");
			}
			if (data.kind == KIND_LOCATION && (!data.onMap || data.name == ""))
			{
				var s:String = T("Неоткрытая локация", "Undiscovered location");
				return data.type != "" ? s + " (" + data.type + ")" : s;
			}
			return data.name;
		}

		private function RowState(data:Object):String
		{
			if (data.kind == KIND_INTERIOR)
			{
				return T("интерьер", "interior");
			}
			if (data.kind == KIND_LOCATION)
			{
				return data.onMap ? T("на карте", "on the map") : T("на компасе", "on the compass");
			}
			switch (data.state)
			{
				case 1:
					return data.kind == 2 ? T("Pip-Boy, без маркера", "Pip-Boy, no marker") : T("в Pip-Boy", "in the Pip-Boy");
				case 2:
					return T("можно начать", "can be started");
				case 3:
					return T("не начат", "not started");
				case 4:
					return T("выполнен", "completed");
			}
			return "";
		}

		private function DistanceText(dist:Number):String
		{
			if (dist == -2)
			{
				return T("другой мир", "other world");
			}
			if (dist < 0)
			{
				return "—";
			}
			var meters:Number = dist * METERS_PER_UNIT;
			if (meters < 1000)
			{
				return Math.round(meters) + T(" м", " m");
			}
			return (Math.round(meters / 100) / 10) + T(" км", " km");
		}

		private function OnRowClick(e:MouseEvent):void
		{
			var row:Object = RowOf(e.currentTarget);
			if (row && row.data && row.data.kind != KIND_NONE)
			{
				SendAction("mark", row.data);
			}
		}

		// Щелчок по кнопке не доходит до строки (иначе строка ещё и отметилась бы).
		private function OnRowButtonClick(e:MouseEvent):void
		{
			e.stopPropagation();
			var row:Object = RowOf((e.currentTarget as Sprite).parent);
			if (row && row.data && row.data.kind != KIND_NONE)
			{
				SendAction(row.data.excluded ? "restore" : "exclude", row.data);
			}
		}

		private function SendAction(asAction:String, data:Object):void
		{
			Log("action " + asAction + ": " + Describe(data));
			try
			{
				BGSCodeObj.PlaySound("UIMenuOK");
			}
			catch (err:Error)
			{
			}
			if (!f4se)
			{
				return;
			}
			SetStatus("…");
			f4se.SendExternalEvent(ACTION_EVENT, asAction, int(data.kind), int(data.quest), int(data.ref));
			waitFrames = REFRESH_WAIT_FRAMES;
		}

		private function RowOf(o:Object):Object
		{
			for (var i:int = 0; i < rows.length; i++)
			{
				if (rows[i].sprite == o)
				{
					return rows[i];
				}
			}
			return null;
		}

		private function OnRowOver(e:MouseEvent):void
		{
			var row:Object = RowOf(e.currentTarget);
			if (row && row.data && row.data.kind != KIND_NONE)
			{
				DrawRow(row, true);
			}
		}

		private function OnRowOut(e:MouseEvent):void
		{
			var row:Object = RowOf(e.currentTarget);
			if (row)
			{
				DrawRow(row, false);
			}
		}

		private function OnRowButtonOver(e:MouseEvent):void
		{
			var row:Object = RowOf((e.currentTarget as Sprite).parent);
			if (row)
			{
				DrawRowButton(row, true);
			}
		}

		private function OnRowButtonOut(e:MouseEvent):void
		{
			var row:Object = RowOf((e.currentTarget as Sprite).parent);
			if (row)
			{
				DrawRowButton(row, false);
			}
		}

		private function OnTabClick(e:MouseEvent):void
		{
			for (var i:int = 0; i < tabButtons.length; i++)
			{
				if (tabButtons[i].sprite == e.currentTarget && tab != i)
				{
					tab = i;
					scroll = 0;
					try
					{
						BGSCodeObj.PlaySound("UIMenuFocus");
					}
					catch (err:Error)
					{
					}
					ShowList();
				}
			}
		}

		// ---------------------------------------------------------------------------------------------------
		// Построение

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

			var title:TextField = MakeField(26, TextFormatAlign.CENTER, PANEL_W, true);
			title.y = 16;
			SetText(title, "$NQT_MENU_TITLE");
			panel.addChild(title);

			for (var t:int = 0; t < 2; t++)
			{
				var tabButton:Object = MakeButton(200, BUTTON_H, OnTabClick, null, null);
				tabButton.sprite.x = LIST_X + t * 210;
				tabButton.sprite.y = 62;
				tabButton.sprite.addEventListener(MouseEvent.ROLL_OVER, OnTabOver);
				tabButton.sprite.addEventListener(MouseEvent.ROLL_OUT, OnTabOut);
				tabButtons.push(tabButton);
				panel.addChild(tabButton.sprite);
			}

			var columns:Array = [COL_NAME, COL_WORLD, COL_DIST, COL_STATE];
			var widths:Array = [COL_WORLD - COL_NAME - 10, COL_DIST - COL_WORLD - 10, COL_STATE - COL_DIST - 10, COL_ACTION - COL_STATE - 10];
			for (var c:int = 0; c < columns.length; c++)
			{
				var h:TextField = MakeField(16, c == 2 ? TextFormatAlign.RIGHT : TextFormatAlign.LEFT, widths[c], false);
				h.x = LIST_X + columns[c];
				h.y = LIST_Y - 28;
				h.alpha = 0.7;
				header.push(h);
				panel.addChild(h);
			}
			var line:Shape = new Shape();
			line.graphics.lineStyle(1, 0xFFFFFF, 0.6);
			line.graphics.moveTo(LIST_X, LIST_Y - 2);
			line.graphics.lineTo(LIST_X + LIST_W, LIST_Y - 2);
			panel.addChild(line);

			for (var i:int = 0; i < ROWS; i++)
			{
				panel.addChild(MakeRow(i));
			}

			// Строка под списком: слева состояние, по центру «Закрыть», справа счётчик.
			scrollInfo = MakeField(16, TextFormatAlign.RIGHT, 300, false);
			scrollInfo.x = LIST_X + LIST_W - 300;
			scrollInfo.y = PANEL_H - BUTTON_H - 16 + 8;
			scrollInfo.alpha = 0.7;
			panel.addChild(scrollInfo);

			status = MakeField(16, TextFormatAlign.LEFT, (PANEL_W - 200) / 2 - LIST_X - 20, false);
			status.x = LIST_X;
			status.y = scrollInfo.y;
			status.alpha = 0.7;
			panel.addChild(status);

			var close:Object = MakeButton(200, BUTTON_H, OnCloseClick, null, null);
			closeButton = close.sprite;
			closeFrame = close.back;
			SetText(close.text, "$NQT_MENU_CLOSE");
			closeButton.x = (PANEL_W - 200) / 2;
			closeButton.y = PANEL_H - BUTTON_H - 16;
			closeButton.addEventListener(MouseEvent.ROLL_OVER, OnCloseOver);
			closeButton.addEventListener(MouseEvent.ROLL_OUT, OnCloseOut);
			DrawFrame(closeFrame, 200, BUTTON_H, false);
			panel.addChild(closeButton);
		}

		private function MakeRow(i:int):Sprite
		{
			var row:Object = {};
			var sprite:Sprite = new Sprite();
			sprite.x = LIST_X;
			sprite.y = LIST_Y + i * ROW_H;
			sprite.buttonMode = true;
			sprite.useHandCursor = true;
			var back:Shape = new Shape();
			sprite.addChild(back);
			row.sprite = sprite;
			row.back = back;
			row.name = AddCell(sprite, COL_NAME, COL_WORLD - COL_NAME - 10, TextFormatAlign.LEFT);
			row.world = AddCell(sprite, COL_WORLD, COL_DIST - COL_WORLD - 10, TextFormatAlign.LEFT);
			row.dist = AddCell(sprite, COL_DIST, COL_STATE - COL_DIST - 10, TextFormatAlign.RIGHT);
			row.state = AddCell(sprite, COL_STATE, COL_ACTION - COL_STATE - 10, TextFormatAlign.LEFT);

			var button:Object = MakeButton(104, ROW_H - 6, OnRowButtonClick, OnRowButtonOver, OnRowButtonOut);
			button.sprite.x = COL_ACTION;
			button.sprite.y = 3;
			row.button = button.sprite;
			row.buttonBack = button.back;
			row.buttonText = button.text;
			sprite.addChild(button.sprite);

			sprite.addEventListener(MouseEvent.CLICK, OnRowClick);
			sprite.addEventListener(MouseEvent.ROLL_OVER, OnRowOver);
			sprite.addEventListener(MouseEvent.ROLL_OUT, OnRowOut);
			sprite.visible = false;
			rows.push(row);
			return sprite;
		}

		private function AddCell(parent:Sprite, x:Number, w:Number, align:String):TextField
		{
			var tf:TextField = MakeField(18, align, w, false);
			tf.x = x;
			tf.y = 3;
			parent.addChild(tf);
			return tf;
		}

		// Кнопка: {sprite, back, text}; надпись по центру.
		private function MakeButton(w:Number, h:Number, onClick:Function, onOver:Function, onOut:Function):Object
		{
			var sprite:Sprite = new Sprite();
			sprite.buttonMode = true;
			sprite.useHandCursor = true;
			sprite.mouseChildren = false;
			var back:Shape = new Shape();
			sprite.addChild(back);
			var text:TextField = MakeField(h > 34 ? 20 : 16, TextFormatAlign.CENTER, w, false);
			text.y = (h - (h > 34 ? 28 : 22)) / 2;
			sprite.addChild(text);
			sprite.addEventListener(MouseEvent.CLICK, onClick);
			if (onOver != null)
			{
				sprite.addEventListener(MouseEvent.ROLL_OVER, onOver);
			}
			if (onOut != null)
			{
				sprite.addEventListener(MouseEvent.ROLL_OUT, onOut);
			}
			var button:Object = { sprite: sprite, back: back, text: text, w: w, h: h };
			DrawFrame(back, w, h, false);
			return button;
		}

		private function DrawFrame(back:Shape, w:Number, h:Number, abHover:Boolean):void
		{
			back.graphics.clear();
			back.graphics.beginFill(0xFFFFFF, abHover ? 0.3 : 0.08);
			back.graphics.lineStyle(1, 0xFFFFFF, 1);
			back.graphics.drawRect(0, 0, w, h);
			back.graphics.endFill();
		}

		private function DrawRow(row:Object, abHover:Boolean):void
		{
			row.back.graphics.clear();
			row.back.graphics.beginFill(0xFFFFFF, abHover ? 0.18 : (row.data && row.data.tracked ? 0.1 : 0.0));
			row.back.graphics.drawRect(0, 0, LIST_W, ROW_H - 2);
			row.back.graphics.endFill();
		}

		private function DrawRowButton(row:Object, abHover:Boolean):void
		{
			DrawFrame(row.buttonBack, 104, ROW_H - 6, abHover);
		}

		private function DrawTab(tabButton:Object, abHover:Boolean):void
		{
			var active:Boolean = tabButtons.indexOf(tabButton) == tab;
			tabButton.back.graphics.clear();
			tabButton.back.graphics.beginFill(0xFFFFFF, active ? 0.35 : (abHover ? 0.2 : 0.06));
			tabButton.back.graphics.lineStyle(active ? 2 : 1, 0xFFFFFF, 1);
			tabButton.back.graphics.drawRect(0, 0, tabButton.w, tabButton.h);
			tabButton.back.graphics.endFill();
		}

		private function OnTabOver(e:MouseEvent):void
		{
			for (var i:int = 0; i < tabButtons.length; i++)
			{
				if (tabButtons[i].sprite == e.currentTarget)
				{
					DrawTab(tabButtons[i], true);
				}
			}
		}

		private function OnTabOut(e:MouseEvent):void
		{
			for (var i:int = 0; i < tabButtons.length; i++)
			{
				if (tabButtons[i].sprite == e.currentTarget)
				{
					DrawTab(tabButtons[i], false);
				}
			}
		}

		private function OnCloseOver(e:MouseEvent):void
		{
			DrawFrame(closeFrame, 200, BUTTON_H, true);
			try
			{
				BGSCodeObj.PlaySound("UIMenuFocus");
			}
			catch (err:Error)
			{
			}
		}

		private function OnCloseOut(e:MouseEvent):void
		{
			DrawFrame(closeFrame, 200, BUTTON_H, false);
		}

		private function OnCloseClick(e:MouseEvent):void
		{
			Close("button");
		}

		private function SetStatus(s:String):void
		{
			SetText(status, s);
		}

		// Тест 3a: формат поля надо ставить setTextFormat после присвоения текста (ключ "$…" Scaleform переводит
		// сразу при присвоении, и одного defaultTextFormat не хватает: текст был без шрифта).
		private function MakeField(size:Number, align:String, w:Number, abMultiline:Boolean):TextField
		{
			var tf:TextField = new TextField();
			tf.width = w;
			tf.embedFonts = true;
			tf.selectable = false;
			tf.mouseEnabled = false;
			tf.multiline = abMultiline;
			tf.wordWrap = abMultiline;
			if (abMultiline)
			{
				tf.autoSize = TextFieldAutoSize.LEFT;
			}
			else
			{
				tf.height = size + 10;
			}
			var fmt:TextFormat = new TextFormat("$MAIN_Font", size, 0xFFFFFF);
			fmt.align = align;
			tf.defaultTextFormat = fmt;
			return tf;
		}

		private static function SetText(tf:TextField, s:String):void
		{
			var fmt:TextFormat = tf.defaultTextFormat;
			tf.text = s != null ? s : "";
			tf.setTextFormat(fmt);
		}

		private function T(asRussian:String, asEnglish:String):String
		{
			return russian ? asRussian : asEnglish;
		}

		private static function Describe(data:Object):String
		{
			return "kind " + data.kind + " quest " + data.quest + " ref " + data.ref + " '" + data.name + "' world '"
				+ data.world + "' rank " + data.worldRank + " dist " + Math.round(data.dist) + (data.tracked ? " tracked" : "")
				+ (data.excluded ? " excluded" : "");
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
