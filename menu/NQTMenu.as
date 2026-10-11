package
{
	import flash.display.MovieClip;
	import flash.display.Shape;
	import flash.display.Sprite;
	import flash.events.Event;
	import flash.events.KeyboardEvent;
	import flash.events.MouseEvent;
	import flash.geom.Point;
	import flash.text.TextField;
	import flash.text.TextFieldAutoSize;
	import flash.text.TextFormat;
	import flash.text.TextFormatAlign;
	import flash.utils.getTimer;

	// Nearest Quest Tracker: своё окно. Вкладка «Квесты» (этап 3c): слева дерево категорий (QuestCategories.json) со
	// счётчиками «выполнено/всего», справа квесты выбранной категории; фильтры — состояние, маркер, мир. Вкладка
	// «Локации» (этап 3d): неоткрытые локации карты и интерьеры, где игрок не был; фильтры — замеченные / все, локации /
	// интерьеры, мир. У каждой вкладки свои значения фильтров (те же три кнопки).
	// Меню регистрирует NQT_MainScript через F4SE UI.RegisterCustomMenu(MENU_NAME, "NearestQuestTracker", "root1", …):
	// этот класс — корень SWF (root1) и объект меню. Движок кладёт в BGSCodeObj функции F4SE CustomMenu
	// (PlaySound, OpenMenu, CloseMenu) и зовёт ProcessUserEvent с событиями управления меню.
	// Данные: F4SE отдаёт свой объект в onF4SEObjCreated, в нём f4se.plugins.NearestQuestTracker — функции DLL
	// (f4se_plugin/src/Window.h: GetLanguage, GetQuests, GetLocations, GetCategories; строки — объекты с полями
	// kind, quest, ref, name, world, worldRank, dist, tracked, excluded, …), уже в порядке хоткея «Отметить ближайшее».
	// Действия: f4se.SendExternalEvent(ACTION_EVENT, "mark" | "exclude" | "restore", kind, quest, ref) ->
	// NQT_MainScript.OnWindowAction, тот делает то же, что хоткеи, и зовёт root1.Refresh (UI.Invoke). Papyrus работает
	// и при открытом окне (игра на паузе; тест 3b).
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
		private static const MARGIN:Number = 30;
		private static const LIST_Y:Number = 150;
		private static const ACTION_W:Number = 104;
		// Дерево категорий слева (вкладка «Квесты»).
		private static const TREE_W:Number = 300;
		private static const TREE_INDENT:Number = 16;
		// Игровые единицы -> метры (1 единица = 1,428 см).
		private static const METERS_PER_UNIT:Number = 0.01428;
		// Сколько кадров ждать root1.Refresh от скрипта после действия, прежде чем обновиться самим.
		private static const REFRESH_WAIT_FRAMES:int = 90;

		// Kind* из NQT_MainScript (поле kind строки).
		private static const KIND_NONE:int = 0;
		private static const KIND_NO_MARKER:int = 2;
		private static const KIND_LOCATION:int = 4;
		private static const KIND_INTERIOR:int = 5;
		// Поле state строки квеста (Window.h).
		private static const STATE_PIPBOY:int = 1;
		private static const STATE_CAN_START:int = 2;
		private static const STATE_NOT_STARTED:int = 3;
		private static const STATE_COMPLETED:int = 4;
		// Фильтр состояния: невыполненные (по умолчанию), все, в Pip-Boy, можно начать, не начатые, выполненные.
		private static const STATE_FILTERS:int = 6;

		public var BGSCodeObj:Object;
		private var f4se:Object;
		private var plugin:Object;
		private var russian:Boolean = true;

		private var quests:Array = [];
		private var locations:Array = [];
		private var locationsSeen:int = 0;  // замеченные (seen): хоткей выбирает их без MCM «Все локации»
		private var shown:Array = [];       // строки текущей вкладки после фильтров
		private var tab:int = 0;            // 0 квесты, 1 локации
		private var scroll:int = 0;

		// Дерево: узлы {key, name, depth, parent, children, total, done, expanded}; корень — key "" («Все квесты»).
		private var nodes:Object = {};
		private var root:Object;
		private var treeLines:Array = [];   // видимые узлы по порядку
		private var treeScroll:int = 0;
		private var selectedKey:String = "";
		// Фильтры вкладки «Квесты».
		private var stateFilter:int = 0;
		private var markerOnly:Boolean = false;
		// Фильтры вкладки «Локации».
		private var seenOnly:Boolean = true;
		private var placeFilter:int = 0;    // 0 локации и интерьеры, 1 только локации, 2 только интерьеры
		// Фильтр мира, по вкладкам: имена миров из строк (в порядке ранга) и выбранный (-1 все миры).
		private var worlds:Array = [[], []];
		private var worldFilter:Array = [-1, -1];

		private var panel:Sprite;
		private var listBox:Sprite;
		private var listLine:Shape;
		private var treeBox:Sprite;
		private var rows:Array = [];        // ROWS объектов {sprite, back, name, world, dist, state, button, buttonBack, buttonText, data}
		private var treeRows:Array = [];    // ROWS объектов {sprite, back, name, count, node}
		private var tabButtons:Array = [];
		private var filterButtons:Array = [];   // состояние, маркер, мир
		private var header:Array = [];      // колонки списка (0..3), заголовок дерева (4)
		private var status:TextField;
		private var scrollInfo:TextField;
		private var closeButton:Sprite;
		private var closeFrame:Shape;
		private var closing:Boolean = false;
		private var waitFrames:int = 0;
		// Колонки списка для текущей вкладки: x названия, мира, расстояния, состояния, кнопки; ширина списка.
		private var listW:Number = 0;
		private var cols:Array = [];

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
					BuildTree(plugin.GetCategories());
				}
				catch (err:Error)
				{
					Log("GetLanguage / GetCategories failed: " + err.message);
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
				ShowAll();
				return;
			}
			var t0:int = getTimer();
			try
			{
				quests = plugin.GetQuests();
				var t1:int = getTimer();
				locations = plugin.GetLocations();
				var t2:int = getTimer();
				locationsSeen = 0;
				for (var i:int = 0; i < locations.length; i++)
				{
					if (locations[i].seen)
					{
						locationsSeen++;
					}
				}
				CountTree();
				CollectWorlds(0, quests);
				CollectWorlds(1, locations);
				Log("Refresh: " + quests.length + " quests (" + (t1 - t0) + " ms), " + locations.length + " locations, "
					+ locationsSeen + " seen (" + (t2 - t1) + " ms); " + treeLines.length + " tree lines, worlds: "
					+ worlds[0].join(", ") + " / " + worlds[1].join(", "));
				SetStatus("");
			}
			catch (err:Error)
			{
				Log("Refresh failed: " + err.message);
				SetStatus(T("Ошибка чтения списков: ", "Could not read the lists: ") + err.message);
			}
			ShowAll();
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

		// Колесо над деревом прокручивает дерево, иначе — список.
		private function OnWheel(e:MouseEvent):void
		{
			var step:int = e.delta > 0 ? -3 : 3;
			if (tab == 0)
			{
				var p:Point = treeBox.globalToLocal(new Point(e.stageX, e.stageY));
				if (p.x >= 0 && p.x < TREE_W && p.y >= 0 && p.y < ROWS * ROW_H)
				{
					var next:int = Math.max(0, Math.min(treeLines.length - ROWS, treeScroll + step));
					if (next != treeScroll)
					{
						treeScroll = next;
						ShowTree();
					}
					return;
				}
			}
			ScrollBy(step);
		}

		private function ScrollBy(aiRows:int):void
		{
			var max:int = Math.max(0, shown.length - ROWS);
			var next:int = Math.max(0, Math.min(max, scroll + aiRows));
			if (next != scroll)
			{
				scroll = next;
				ShowList();
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

		// ---------------------------------------------------------------------------------------------------
		// Дерево категорий

		// Узлы из GetCategories (родитель раньше детей; ключ "a.b.c" — ребёнок "a.b"), "other" — последний.
		private function BuildTree(categories:Array):void
		{
			nodes = {};
			root = { key: "", name: T("Все квесты", "All quests"), depth: -1, parent: null, children: [], total: 0, done: 0,
				expanded: true };
			nodes[""] = root;
			for (var i:int = 0; i < categories.length; i++)
			{
				AddNode(categories[i].key, categories[i].name);
			}
		}

		private function AddNode(key:String, name:String):Object
		{
			if (nodes[key])
			{
				return nodes[key];
			}
			var dot:int = key.lastIndexOf(".");
			var parent:Object = dot < 0 ? root : nodes[key.substr(0, dot)];
			if (!parent)
			{
				parent = AddNode(key.substr(0, dot), key.substr(0, dot));
			}
			var node:Object = { key: key, name: name, depth: parent.depth + 1, parent: parent, children: [], total: 0, done: 0,
				expanded: false };
			parent.children.push(node);
			nodes[key] = node;
			return node;
		}

		// Счётчики «выполнено/всего» по всем квестам (без фильтров). Категории "other.<плагин>" появляются тут:
		// их имя — имя файла плагина.
		private function CountTree():void
		{
			for (var k:String in nodes)
			{
				nodes[k].total = 0;
				nodes[k].done = 0;
			}
			for (var i:int = 0; i < quests.length; i++)
			{
				var q:Object = quests[i];
				var key:String = q.category;
				var node:Object = nodes[key];
				if (!node)
				{
					node = AddNode(key, key.indexOf("other.") == 0 ? key.substr(6) : key);
				}
				for (; node; node = node.parent)
				{
					node.total++;
					if (q.state == STATE_COMPLETED)
					{
						node.done++;
					}
				}
			}
			if (!nodes[selectedKey] || nodes[selectedKey].total == 0)
			{
				selectedKey = "";
			}
			BuildTreeLines();
		}

		// Видимые строки дерева: развёрнутые ветки, без пустых категорий (дополнение не установлено).
		private function BuildTreeLines():void
		{
			treeLines = [];
			AddTreeLines(root);
			treeScroll = Math.max(0, Math.min(treeScroll, treeLines.length - ROWS));
		}

		private function AddTreeLines(node:Object):void
		{
			if (node.total == 0)
			{
				return;
			}
			treeLines.push(node);
			if (node.expanded)
			{
				for (var i:int = 0; i < node.children.length; i++)
				{
					AddTreeLines(node.children[i]);
				}
			}
		}

		private function HasVisibleChildren(node:Object):Boolean
		{
			for (var i:int = 0; i < node.children.length; i++)
			{
				if (node.children[i].total > 0)
				{
					return true;
				}
			}
			return false;
		}

		private function ShowTree():void
		{
			for (var i:int = 0; i < ROWS; i++)
			{
				var row:Object = treeRows[i];
				var node:Object = treeScroll + i < treeLines.length ? treeLines[treeScroll + i] : null;
				row.node = node;
				row.sprite.visible = node != null;
				if (!node)
				{
					continue;
				}
				var indent:Number = Math.max(0, node.depth) * TREE_INDENT;
				var sign:String = node == root ? "" : (HasVisibleChildren(node) ? (node.expanded ? "– " : "+ ") : "");
				row.name.x = 6 + indent;
				row.name.width = TREE_W - 70 - indent;
				SetCell(row.name, sign + node.name);
				SetText(row.count, node.done + "/" + node.total);
				DrawTreeRow(row, false);
			}
		}

		// Щелчок по категории: выбрать её (и развернуть); по уже выбранной — свернуть / развернуть.
		private function OnTreeClick(e:MouseEvent):void
		{
			var row:Object = TreeRowOf(e.currentTarget);
			if (!row || !row.node)
			{
				return;
			}
			var node:Object = row.node;
			if (node != root)
			{
				node.expanded = node.key == selectedKey ? !node.expanded : true;
			}
			selectedKey = node.key;
			scroll = 0;
			PlaySound("UIMenuFocus");
			BuildTreeLines();
			ShowAll();
		}

		private function TreeRowOf(o:Object):Object
		{
			for (var i:int = 0; i < treeRows.length; i++)
			{
				if (treeRows[i].sprite == o)
				{
					return treeRows[i];
				}
			}
			return null;
		}

		private function OnTreeOver(e:MouseEvent):void
		{
			var row:Object = TreeRowOf(e.currentTarget);
			if (row)
			{
				DrawTreeRow(row, true);
				if (row.node)
				{
					SetStatus(row.node.name);
				}
			}
		}

		private function OnTreeOut(e:MouseEvent):void
		{
			var row:Object = TreeRowOf(e.currentTarget);
			if (row)
			{
				DrawTreeRow(row, false);
			}
			SetStatus("");
		}

		private function DrawTreeRow(row:Object, abHover:Boolean):void
		{
			var selected:Boolean = row.node && row.node.key == selectedKey;
			row.back.graphics.clear();
			row.back.graphics.beginFill(0xFFFFFF, abHover ? 0.18 : (selected ? 0.25 : 0.0));
			row.back.graphics.drawRect(0, 0, TREE_W, ROW_H - 2);
			row.back.graphics.endFill();
		}

		// ---------------------------------------------------------------------------------------------------
		// Фильтры

		// Миры строк вкладки aiTab по рангу (мир игрока первым); выбранный мир остаётся выбранным, если он ещё есть.
		private function CollectWorlds(aiTab:int, list:Array):void
		{
			var old:Array = worlds[aiTab];
			var current:String = worldFilter[aiTab] >= 0 && worldFilter[aiTab] < old.length ? old[worldFilter[aiTab]] : null;
			var ranks:Object = {};
			var names:Array = [];
			for (var i:int = 0; i < list.length; i++)
			{
				var w:String = list[i].world;
				if (w != "" && ranks[w] === undefined)
				{
					ranks[w] = list[i].worldRank;
					names.push(w);
				}
			}
			names.sort(function(a:String, b:String):int
			{
				return ranks[a] != ranks[b] ? ranks[a] - ranks[b] : (a < b ? -1 : (a > b ? 1 : 0));
			});
			worlds[aiTab] = names;
			worldFilter[aiTab] = current != null ? names.indexOf(current) : -1;
		}

		private function WorldFilterName():String
		{
			return worldFilter[tab] >= 0 ? worlds[tab][worldFilter[tab]] : T("Все миры", "All worlds");
		}

		private function PassesStateFilter(q:Object):Boolean
		{
			switch (stateFilter)
			{
				case 0:
					return q.state != STATE_COMPLETED;
				case 2:
					return q.state == STATE_PIPBOY;
				case 3:
					return q.state == STATE_CAN_START;
				case 4:
					return q.state == STATE_NOT_STARTED;
				case 5:
					return q.state == STATE_COMPLETED;
			}
			return true;
		}

		private function StateFilterName():String
		{
			switch (stateFilter)
			{
				case 0:
					return T("Невыполненные", "Not completed");
				case 1:
					return T("Все состояния", "Any state");
				case 2:
					return T("В Pip-Boy", "In the Pip-Boy");
				case 3:
					return T("Можно начать", "Can be started");
				case 4:
					return T("Не начатые", "Not started");
				case 5:
					return T("Выполненные", "Completed");
			}
			return "";
		}

		private function InCategory(q:Object):Boolean
		{
			return selectedKey == "" || q.category == selectedKey || q.category.indexOf(selectedKey + ".") == 0;
		}

		private function Filter():void
		{
			shown = [];
			var world:String = worldFilter[tab] >= 0 ? worlds[tab][worldFilter[tab]] : null;
			var i:int;
			if (tab == 1)
			{
				for (i = 0; i < locations.length; i++)
				{
					var l:Object = locations[i];
					var interior:Boolean = l.kind == KIND_INTERIOR;
					if ((!seenOnly || l.seen) && (placeFilter == 0 || interior == (placeFilter == 2))
						&& (world == null || l.world == world))
					{
						shown.push(l);
					}
				}
				return;
			}
			for (i = 0; i < quests.length; i++)
			{
				var q:Object = quests[i];
				if (InCategory(q) && PassesStateFilter(q) && (!markerOnly || q.dist != -1) && (world == null || q.world == world))
				{
					shown.push(q);
				}
			}
		}

		// Кнопки фильтров по кругу: щелчок — следующее значение.
		private function OnFilterClick(e:MouseEvent):void
		{
			var i:int = filterButtons.indexOf(ButtonOf(filterButtons, e.currentTarget));
			if (i == 0)
			{
				if (tab == 0)
				{
					stateFilter = (stateFilter + 1) % STATE_FILTERS;
				}
				else
				{
					seenOnly = !seenOnly;
				}
			}
			else if (i == 1)
			{
				if (tab == 0)
				{
					markerOnly = !markerOnly;
				}
				else
				{
					placeFilter = (placeFilter + 1) % 3;
				}
			}
			else if (i == 2)
			{
				worldFilter[tab] = worldFilter[tab] + 1 >= worlds[tab].length ? -1 : worldFilter[tab] + 1;
			}
			scroll = 0;
			PlaySound("UIMenuFocus");
			ShowAll();
			DrawFrame(filterButtons[i].back, filterButtons[i].w, filterButtons[i].h, true);
		}

		// ---------------------------------------------------------------------------------------------------
		// Список

		private function ShowAll():void
		{
			Filter();
			treeBox.visible = tab == 0;
			LayoutList();
			if (tab == 0)
			{
				ShowTree();
				SetText(filterButtons[0].text, StateFilterName());
				SetText(filterButtons[1].text, markerOnly ? T("Только с маркером", "With a marker") : T("С маркером и без", "Marker or not"));
			}
			else
			{
				SetText(filterButtons[0].text, seenOnly ? T("Замеченные", "Seen") : T("Все (спойлеры)", "All (spoilers)"));
				SetText(filterButtons[1].text, placeFilter == 0 ? T("Локации и интерьеры", "Places and interiors")
					: (placeFilter == 1 ? T("Только локации", "Locations only") : T("Только интерьеры", "Interiors only")));
			}
			SetCell(filterButtons[2].text, WorldFilterName());
			for (var f:int = 0; f < filterButtons.length; f++)
			{
				DrawFrame(filterButtons[f].back, filterButtons[f].w, filterButtons[f].h, false);
			}
			SetText(tabButtons[0].text, T("Квесты", "Quests") + " (" + NotCompletedCount() + ")");
			SetText(tabButtons[1].text, T("Локации", "Locations") + " (" + locationsSeen + "/" + locations.length + ")");
			for (var t:int = 0; t < tabButtons.length; t++)
			{
				DrawTab(tabButtons[t], false);
			}
			ShowList();
		}

		private function NotCompletedCount():int
		{
			var count:int = 0;
			for (var i:int = 0; i < quests.length; i++)
			{
				if (quests[i].state != STATE_COMPLETED)
				{
					count++;
				}
			}
			return count;
		}

		// Квесты: список правее дерева, колонки уже; локации: на всю ширину.
		private function LayoutList():void
		{
			if (tab == 0)
			{
				listBox.x = MARGIN + TREE_W + 20;
				listW = PANEL_W - MARGIN - listBox.x;
				// Мир 135 (тест 3c: «Commonwealth» в 110 не влезал), расстояние 70, состояние 125.
				cols = [6, listW - ACTION_W - 365, listW - ACTION_W - 220, listW - ACTION_W - 135, listW - ACTION_W];
			}
			else
			{
				listBox.x = MARGIN;
				listW = PANEL_W - 2 * MARGIN;
				cols = [10, 520, 680, 790, listW - ACTION_W - 6];
			}
			for (var c:int = 0; c < 4; c++)
			{
				var w:Number = cols[c + 1] - cols[c] - 10;
				header[c].x = cols[c];
				header[c].width = w;
				for (var i:int = 0; i < ROWS; i++)
				{
					var cell:TextField = [rows[i].name, rows[i].world, rows[i].dist, rows[i].state][c];
					cell.x = cols[c];
					cell.width = w;
				}
			}
			for (i = 0; i < ROWS; i++)
			{
				rows[i].button.x = cols[4];
			}
			listLine.graphics.clear();
			listLine.graphics.lineStyle(1, 0xFFFFFF, 0.6);
			listLine.graphics.moveTo(0, -2);
			listLine.graphics.lineTo(listW, -2);
			SetText(header[0], tab == 0 ? T("Квест (клик — отметить)", "Quest (click to mark)")
				: T("Локация (клик — отметить)", "Location (click to mark)"));
			SetText(header[1], T("Мир", "World"));
			SetText(header[2], T("Расст.", "Dist."));
			SetText(header[3], tab == 0 ? T("Состояние", "State") : T("Тип", "Type"));
			SetText(header[4], T("Категории (выполнено/всего)", "Categories (done/total)"));
		}

		private function ShowList():void
		{
			scroll = Math.max(0, Math.min(scroll, shown.length - ROWS));
			for (var i:int = 0; i < ROWS; i++)
			{
				var row:Object = rows[i];
				var data:Object = scroll + i < shown.length ? shown[scroll + i] : null;
				row.data = data;
				row.sprite.visible = data != null;
				if (!data)
				{
					continue;
				}
				SetCell(row.name, (data.tracked ? "» " : "") + RowName(data));
				SetCell(row.world, data.world);
				SetText(row.dist, DistanceText(data.dist));
				SetCell(row.state, RowState(data));
				row.sprite.alpha = data.excluded ? 0.45 : (data.kind == KIND_NONE ? 0.7 : 1.0);
				row.sprite.buttonMode = data.kind != KIND_NONE;
				row.button.visible = data.kind != KIND_NONE;
				SetText(row.buttonText, data.excluded ? T("Вернуть", "Restore") : T("Исключить", "Exclude"));
				DrawRow(row, false);
				DrawFrame(row.buttonBack, ACTION_W, ROW_H - 6, false);
			}
			if (shown.length == 0)
			{
				SetText(scrollInfo, T("Пусто", "Empty"));
			}
			else
			{
				SetText(scrollInfo, (scroll + 1) + "–" + Math.min(shown.length, scroll + ROWS) + T(" из ", " of ") + shown.length);
			}
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
				return data.onMap ? T("на карте", "on the map") : (data.seen ? T("на компасе", "on the compass") : T("не замечена", "not seen"));
			}
			switch (data.state)
			{
				case STATE_PIPBOY:
					return data.kind == KIND_NO_MARKER ? T("Pip-Boy, без маркера", "Pip-Boy, no marker") : T("в Pip-Boy", "in the Pip-Boy");
				case STATE_CAN_START:
					return T("можно начать", "can be started");
				case STATE_NOT_STARTED:
					return T("не начат", "not started");
				case STATE_COMPLETED:
					return T("выполнен", "completed");
			}
			return "";
		}

		private function DistanceText(dist:Number):String
		{
			if (dist == -2)
			{
				return T("др. мир", "other world");
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
			PlaySound("UIMenuOK");
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

		// Полное название строки под мышью — в строке состояния (в списке оно может быть обрезано).
		private function OnRowOver(e:MouseEvent):void
		{
			var row:Object = RowOf(e.currentTarget);
			if (row && row.data)
			{
				DrawRow(row, row.data.kind != KIND_NONE);
				SetStatus(RowName(row.data));
			}
		}

		private function OnRowOut(e:MouseEvent):void
		{
			var row:Object = RowOf(e.currentTarget);
			if (row)
			{
				DrawRow(row, false);
			}
			SetStatus("");
		}

		private function OnRowButtonOver(e:MouseEvent):void
		{
			var row:Object = RowOf((e.currentTarget as Sprite).parent);
			if (row)
			{
				DrawFrame(row.buttonBack, ACTION_W, ROW_H - 6, true);
			}
		}

		private function OnRowButtonOut(e:MouseEvent):void
		{
			var row:Object = RowOf((e.currentTarget as Sprite).parent);
			if (row)
			{
				DrawFrame(row.buttonBack, ACTION_W, ROW_H - 6, false);
			}
		}

		private function OnTabClick(e:MouseEvent):void
		{
			var i:int = tabButtons.indexOf(ButtonOf(tabButtons, e.currentTarget));
			if (i >= 0 && i != tab)
			{
				tab = i;
				scroll = 0;
				PlaySound("UIMenuFocus");
				ShowAll();
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

			panel = new Sprite();
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
				var tabButton:Object = MakeButton(180, BUTTON_H, OnTabClick, OnTabOver, OnTabOut);
				tabButton.sprite.x = MARGIN + t * 190;
				tabButton.sprite.y = 62;
				tabButtons.push(tabButton);
				panel.addChild(tabButton.sprite);
			}
			// Фильтры вкладки «Квесты» — справа от вкладок: состояние, маркер, мир.
			var filterW:Array = [190, 230, 170];
			var fx:Number = PANEL_W - MARGIN;
			for (var f:int = filterW.length - 1; f >= 0; f--)
			{
				fx -= filterW[f];
				var filter:Object = MakeButton(filterW[f], BUTTON_H, OnFilterClick, OnFilterOver, OnFilterOut);
				filter.sprite.x = fx;
				filter.sprite.y = 62;
				filterButtons.unshift(filter);
				panel.addChild(filter.sprite);
				fx -= 10;
			}

			listBox = new Sprite();
			listBox.y = LIST_Y;
			panel.addChild(listBox);
			for (var c:int = 0; c < 4; c++)
			{
				var h:TextField = MakeField(16, c == 2 ? TextFormatAlign.RIGHT : TextFormatAlign.LEFT, 100, false);
				h.y = -28;
				h.alpha = 0.7;
				header.push(h);
				listBox.addChild(h);
			}
			listLine = new Shape();
			listBox.addChild(listLine);
			for (var i:int = 0; i < ROWS; i++)
			{
				listBox.addChild(MakeRow(i));
			}

			treeBox = new Sprite();
			treeBox.x = MARGIN;
			treeBox.y = LIST_Y;
			panel.addChild(treeBox);
			var treeTitle:TextField = MakeField(16, TextFormatAlign.LEFT, TREE_W, false);
			treeTitle.y = -28;
			treeTitle.alpha = 0.7;
			header.push(treeTitle);
			treeBox.addChild(treeTitle);
			for (var r:int = 0; r < ROWS; r++)
			{
				treeBox.addChild(MakeTreeRow(r));
			}

			// Строка под списком: слева состояние (и полное название строки под мышью), по центру «Закрыть», справа счётчик.
			scrollInfo = MakeField(16, TextFormatAlign.RIGHT, 260, false);
			scrollInfo.x = PANEL_W - MARGIN - 260;
			scrollInfo.y = PANEL_H - BUTTON_H - 16 + 8;
			scrollInfo.alpha = 0.7;
			panel.addChild(scrollInfo);

			status = MakeField(16, TextFormatAlign.LEFT, (PANEL_W - 200) / 2 - MARGIN - 20, false);
			status.x = MARGIN;
			status.y = scrollInfo.y;
			status.alpha = 0.7;
			panel.addChild(status);

			var close:Object = MakeButton(200, BUTTON_H, OnCloseClick, OnCloseOver, OnCloseOut);
			closeButton = close.sprite;
			closeFrame = close.back;
			SetText(close.text, "$NQT_MENU_CLOSE");
			closeButton.x = (PANEL_W - 200) / 2;
			closeButton.y = PANEL_H - BUTTON_H - 16;
			panel.addChild(closeButton);

			BuildTree([]);
			ShowAll();
		}

		private function MakeRow(i:int):Sprite
		{
			var row:Object = {};
			var sprite:Sprite = new Sprite();
			sprite.y = i * ROW_H;
			sprite.buttonMode = true;
			sprite.useHandCursor = true;
			var back:Shape = new Shape();
			sprite.addChild(back);
			row.sprite = sprite;
			row.back = back;
			row.name = AddCell(sprite, TextFormatAlign.LEFT);
			row.world = AddCell(sprite, TextFormatAlign.LEFT);
			row.dist = AddCell(sprite, TextFormatAlign.RIGHT);
			row.state = AddCell(sprite, TextFormatAlign.LEFT);

			var button:Object = MakeButton(ACTION_W, ROW_H - 6, OnRowButtonClick, OnRowButtonOver, OnRowButtonOut);
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

		private function MakeTreeRow(i:int):Sprite
		{
			var row:Object = {};
			var sprite:Sprite = new Sprite();
			sprite.y = i * ROW_H;
			sprite.buttonMode = true;
			sprite.useHandCursor = true;
			var back:Shape = new Shape();
			sprite.addChild(back);
			row.sprite = sprite;
			row.back = back;
			row.name = AddCell(sprite, TextFormatAlign.LEFT);
			row.count = AddCell(sprite, TextFormatAlign.RIGHT);
			row.count.x = TREE_W - 64;
			row.count.width = 58;
			row.count.alpha = 0.7;
			sprite.addEventListener(MouseEvent.CLICK, OnTreeClick);
			sprite.addEventListener(MouseEvent.ROLL_OVER, OnTreeOver);
			sprite.addEventListener(MouseEvent.ROLL_OUT, OnTreeOut);
			sprite.visible = false;
			treeRows.push(row);
			return sprite;
		}

		private function AddCell(parent:Sprite, align:String):TextField
		{
			var tf:TextField = MakeField(18, align, 100, false);
			tf.y = 3;
			parent.addChild(tf);
			return tf;
		}

		// Кнопка: {sprite, back, text, w, h}; надпись по центру.
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
			sprite.addEventListener(MouseEvent.ROLL_OVER, onOver);
			sprite.addEventListener(MouseEvent.ROLL_OUT, onOut);
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
			row.back.graphics.drawRect(0, 0, listW, ROW_H - 2);
			row.back.graphics.endFill();
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

		private function ButtonOf(list:Array, o:Object):Object
		{
			for (var i:int = 0; i < list.length; i++)
			{
				if (list[i].sprite == o)
				{
					return list[i];
				}
			}
			return null;
		}

		private function OnTabOver(e:MouseEvent):void
		{
			DrawTab(ButtonOf(tabButtons, e.currentTarget), true);
		}

		private function OnTabOut(e:MouseEvent):void
		{
			DrawTab(ButtonOf(tabButtons, e.currentTarget), false);
		}

		private function OnFilterOver(e:MouseEvent):void
		{
			var b:Object = ButtonOf(filterButtons, e.currentTarget);
			DrawFrame(b.back, b.w, b.h, true);
		}

		private function OnFilterOut(e:MouseEvent):void
		{
			var b:Object = ButtonOf(filterButtons, e.currentTarget);
			DrawFrame(b.back, b.w, b.h, false);
		}

		private function OnCloseOver(e:MouseEvent):void
		{
			DrawFrame(closeFrame, 200, BUTTON_H, true);
			PlaySound("UIMenuFocus");
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
			SetCell(status, s);
		}

		private function PlaySound(asSound:String):void
		{
			try
			{
				BGSCodeObj.PlaySound(asSound);
			}
			catch (err:Error)
			{
			}
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

		// Текст ячейки; не влезает в ширину — обрезается с «…».
		private static function SetCell(tf:TextField, s:String):void
		{
			SetText(tf, s);
			var limit:Number = tf.width - 6;
			if (s == null || tf.textWidth <= limit)
			{
				return;
			}
			var lo:int = 0;
			var hi:int = s.length;
			while (lo < hi)
			{
				var mid:int = (lo + hi + 1) >> 1;
				SetText(tf, s.substr(0, mid) + "…");
				if (tf.textWidth <= limit)
				{
					lo = mid;
				}
				else
				{
					hi = mid - 1;
				}
			}
			SetText(tf, s.substr(0, lo) + "…");
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
