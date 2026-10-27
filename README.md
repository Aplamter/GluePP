# GluePP: Glue3D in C++ (OpenGL)

Glue3D is a 3D game engine that was made in Scratch (`Glue3D.sb3`). GluePP is that engine converted into a native
C++20 program that draws with **OpenGL 3.3**. It needs no browser, Scratch, .NET or any other runtime.

On top of the original, GluePP adds:

- a start menu and project folders;
- saving with Ctrl+S;
- a Unity-style 3D camera and move/rotate/scale gizmos;
- **File > build..**, which turns your project into a game `.exe` without the editor;
- mouse controls for games;
- about **220 new script blocks** and language features:
  - `else`, `repeat`, `for`, `wait_until`;
  - functions, events between objects, `and`/`or` conditions, and answers inside a line (`var d = object.distance(str player)`);
  - object collision and raycasts, tags, single key presses, gamepads;
  - saved progress, levels, pause and slow motion;
  - camera shake, 3D sound, health bars, keyframe animation and debug drawing;
  - variables every object shares (`global`), math on one line (`var d = (var x - var px) ^ val 2`), shortcuts
    (`++`, `toggle`, `clamp`, `swap`…), local variables in functions, arrays, constants and saved variables;
  - terminal tools: lines that update in place, levels and filters, asserts and breakpoints, timers, a dev
    console you can type commands into while the game runs, and the terminal inside built games;
  - new blocks in every category.

Contents:

1. [Build it](#1-build-it)
2. [Use the editor](#2-use-the-editor)
3. [Scripting](#3-scripting): how scripts work
4. [Block reference](#4-block-reference): every block, old and new
5. [Example scripts](#5-example-scripts)
6. [Building your game](#6-building-your-game)
7. [How the code is organized](#7-how-the-code-is-organized)
8. [Changes from the original](#8-changes-from-the-original-scratch-project)
9. [How it was checked](#9-how-it-was-checked)
10. [Known differences from Scratch/TurboWarp](#10-known-differences-from-scratchturbowarp)

---

## 1. Build it

All the libraries are already in `third_party/`: GLFW, glad, stb, miniaudio and nlohmann/json. Nothing is
downloaded, and OpenGL comes with your graphics driver.

### Windows

1. Install [Visual Studio 2022 Community](https://visualstudio.microsoft.com/vs/community/) with the
   **"Desktop development with C++"** workload. That gives you the compiler and CMake.
2. Double-click **`build-windows.bat`**. When it finishes, it opens `build\bin\`.

You can also use **File > Open > Folder** in Visual Studio, choose **Glue3D.exe** as the startup item, and press F5.

### Linux / macOS

```bash
# Linux needs the X11 development packages, e.g. on Ubuntu:
# sudo apt install build-essential cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bin/Glue3D
```

You need a C++20 compiler: Visual Studio 2019 16.10 or later, GCC 11 or later, or Clang 14 or later.

### What you get

```
build/bin/
  Glue3D.exe               the editor
  assets/                  costumes, sounds and font (keep them next to Glue3D.exe)
  player/Glue3DPlayer.exe  the game player that File > build.. turns into your game
  Projects/                your projects (created the first time you make one)
```

The `.exe` files don't need any other files installed. Visual Studio builds use the static runtime, and MinGW builds
link libstdc++ statically.

---

## 2. Use the editor

### Start menu and projects

- After the welcome popup you get the **start menu**:
  - **Create a new project:** type a name and pick the engine parts by switching **tile**, **segment** and
    **mode7** on or off.
  - **Load a project:** browse to a project folder and click it.
- **Projects** are folders inside `Projects/`, next to `Glue3D.exe`. The C# version saves projects in the same
  format, so they open here too.
- **Save** with **File > save project..** or **Ctrl+S**.

### The 3D view

The camera works like Unity's:

| Do this | To do this |
| --- | --- |
| Hold the **right mouse button** and drag | Look around (up to 90° up and down) |
| **W/A/S/D** | Move |
| **Q / E** while holding the right button | Move down / up |
| **Mouse wheel** | Zoom in and out |
| **Middle-drag** | Pan |

The speeds are at the top of `Custom/Glue3DSprite.members.h`.

**Scene toolbar** (top-left of the 3D view):

- The tools are **Select**, **Move**, **Rotate** and **Scale**. Drag an arrow or ring to change the selected
  object, and hold **Ctrl** to snap.
- The **gizmo** button turns the gizmos off. The tool buttons then hide, and clicking an object opens Glue3D's
  own object menu (properties, delete, copy…).

### Scrollbars and other controls

- **Mouse wheel on scrollbars:** the wheel scrolls the scrollbar under the mouse, or the panel it scrolls.
  - **Shift + wheel**, a sideways wheel, or a touchpad swipe scrolls left/right, for example in the script editor.
  - **The block palette's scrollbar fits the category:**
    - A short category gets a short bar, so dragging the handle moves the blocks as far as the handle moves.
    - A long one gets the full bar and scrolls to its last block.
    - One wheel notch always moves at least one block.
- **F11** or **Alt+Enter** toggles fullscreen.

### The script editor

- Pick a category with the icons above the palette, then drag a block into your script.
- Dragging `if`, `while`, `forever`, `repeat`, `for` or `func` adds its end line (`endif`, `endwhile`…) for you.
- Dragging a block that answers something (like `math.add` or `object.get_pos`) adds `var variable = data_return`
  under it, ready for you to rename the variable.
- Blocks inside `if`/`else`/`while`/`forever`/`repeat`/`for`/`func` are drawn indented.
- Functions (`func`, `call`), events (`event.…`), saving (`save.…`) and debugging (`debug.…`) are in the
  **other** category (the red "?" icon).
- The **Aa** button switches to a plain text editor, where you can type scripts.
- Click a value bubble (`val 0.1`, `str human`…) to edit it.
- A block whose name is too long for it is drawn in smaller letters, so the name stays inside the block.
- **Variables panel:** while the game runs, select an object in the scene (Select tool). Its variables, and every
  global variable, show live at the top-right of the game view. The panel is read-only: change values with blocks.

---

## 3. Scripting

### Running a script

A script is a list of lines. You attach a script to an object by dragging it onto the object in the scene. Each
object runs its own script from the top when the game runs (**run** menu, or a built game). A script stops at its
last line unless you use `forever`.

### What a line looks like

```
category.command(type value, type value, ...)
```

For example, `object.move(val 0, val 0.1, val 0)` moves the object up by 0.1.

Every value has a type in front of it:

| Type | Meaning | Example |
| --- | --- | --- |
| `val` | A number | `val 3.5` |
| `str` | A word | `str player` |
| `var` | The value of one of this object's variables | `var score` |

- **A value can't contain spaces or commas**, because spaces and commas separate the values. Use `_` instead:
  blocks that show text (`terminal.warn`, `game.message`, `list.join`…) turn `_` into a space, so
  `str game_over` shows "game over".
- Lines starting with `//` are comments. They're skipped.

### Variables

Variables belong to the object running the script.

```
var score = val 0          // make (or set) a variable
var name = str bob
var copy = var score       // copy another variable
var score += val 10        // also -=  *=  /=  %=  ^=
```

- An object's own properties are variables too, with these names:
  - `x` `y` `z`: position
  - `rx` `ry` `rz`: rotation in degrees (`rx` is the direction it faces)
  - `sx` `sy` `sz`: scale
  - `model`: the model it shows
- So `var x += val 1` moves the object, and you shouldn't use these names for your own variables.
- To read or change another object's variables, use `object.get_data` and `object.edit_data`.
  `object.new_attribute` gives another object a new variable.
- An object's own bookkeeping (`name`, `type`, `script`, `clock`…) can't be set with `var`: it prints an error
  instead of breaking the object.

### Math on one line (new)

After `=` (and `+=`, `-=`, …) you can write a whole calculation:

```
var dist = (var x - var px) ^ val 2 + (var z - var pz) ^ val 2
var hp -= var damage * val 1.5
var d = object.distance(str player) * val 2
```

- **Order:** `^` first, then `*` `/` `%`, then `+` `-`, as in math. Brackets work, and `-` in front of a value
  makes it negative.
- **What can go in:** any value: `val`, `var`, `str`, `global`, `data_return`, an array item, or a block that
  answers.
- **Where else:** the same math works in `if`, `while`, `wait_until`, `terminal.assert` and the values of `call`:

  ```
  if (var hp * val 2 < var max_hp)
  call hit ((var power + val 1) * val 3)
  ```

### Shortcuts (new)

| Line | What it does |
| --- | --- |
| `var i ++` / `var i --` | Adds or subtracts 1. |
| `var on toggle` | Flips `true` ↔ `false`. |
| `var name .= str _bob` | Adds text to the end (`+=` adds numbers). |
| `var hp clamp (val 0, val 100)` | Keeps it between the two values. |
| `var hp min= val 100` | No more than 100 (caps it from above). |
| `var hp max= val 0` | No less than 0 (caps it from below). |
| `var a swap var b` | Swaps two variables (`global` ones too). |
| `var hp ?= val 100` | Sets it only if it doesn't exist yet. Good after `clone_with`: the clone keeps the value it was given, and otherwise gets this default. |

### Global variables (new)

A **global** variable belongs to the whole game, not one object, so every object can read and change it without
`get_data`:

```
global score = val 0
global score += val 10
var s = global score
if (global score > val 100)
interface.text(global score)
```

- `global` works with every operator and shortcut: `=`, `+=`, `++`, `toggle`, `.=`, ….
- `global score` also works as a value in the other blocks, like `interface.text(global score)`.
- A global that was never set reads as 0.
- Globals reset when the game starts again.

### Local variables (new)

Inside a function, `local tmp = val 0` makes a variable that only lasts until the function returns. If a
variable with that name already existed, it gets its old value back. Read it with `var tmp`.

`arg1`, `arg2`… are local too: a function can call another one without losing its own values.

### Arrays (new)

An array is a numbered set of variables, with no files needed:

```
var slot[1] = str sword
var slot[2] = str bow
var w = var slot[var i]
var.push(str slot, str shield)
```

- The index can be a value, a variable or math: `var slot[var i + val 1]`.
- **How they're stored:** `slot[2]` is a normal variable called `slot_2`, so `terminal.dump` and the variables
  panel show the items.
- **Inside other blocks' values**, write the index without a type: `interface.text(var slot[2])` or
  `var slot[i]`.

### Constants and saved variables (new)

- **`const gravity = val 0.9`:** a variable that can't change. Changing it prints an error with the line and keeps
  the value.
- **`saved var highscore = val 0`:**
  - The first time this line runs, it loads `highscore` from `save.json` (or uses 0 if nothing is saved yet).
  - After that, every change is saved by itself.
  - It replaces the `save.get` / `save.set` pattern for the common case.

### Blocks that answer: `data_return`

A block that answers a question puts the answer in the object's **`data_return`** variable. Copy it into your own
variable on the next line:

```
math.add(var score, val 5)
var score = data_return
game.key_pressed(str space)
var jumping = data_return     // "true" or "false"
```

Yes/no answers are the words `true` and `false`, so you check them with `if (var jumping = str true)`.

**Shorter: answers inside a line (new).** Put the block straight after `=`, and its answer goes into the variable:

```
var d = object.distance(str player)
var score += math.add(var bonus, val 5)
var hit = physics.touching(str enemy)
```

This works with every block that answers, and with `=`, `+=`, `-=`, `*=`, `/=`, `%=` and `^=`. The answer is also
left in `data_return`.

### Conditions: `if`, `else`, `while`, `wait_until`

A condition compares two values:

```
if (var score > val 100)
    terminal.print(str you_win, val 0)
else
    terminal.print(str keep_going, val 0)
endif
```

- **Comparisons:**
  - `=` (or `equals`)
  - `!=` (or `not_equals`), new
  - `>` and `<`
  - `>=` and `<=`, new
  - `contains`, new: the first value contains the second, ignoring capitals
- **Combining yes/no values:** `and`, `or`, `xor`, `nand` and `nor`, for example
  `if (var on_floor and var jump_pressed)`.
- **Several comparisons in one condition (new):** join them with `and` / `or`, and put `not` in front of one to turn
  it around:

  ```
  if (var hp < val 1 and var lives > val 0)
  while (var k < val 10 and not var done = str true)
  wait_until (var ready = str true or var timer > val 5)
  ```

  - `and` is worked out before `or`, so `a or b and c` means `a or (b and c)`.
  - Brackets group comparisons, and math works too: `if ((var a > val 1 or var b > val 1) and var c * val 2 < val 9)`.
  - A variable on its own, like `var on_floor`, means "is it `true`?".
- Text comparisons ignore capitals, and `"10"` equals `10`, as in Scratch.
- `else` is optional. An `if` can go inside another `if`, and each one needs its own `endif`.

### Loops

| Loop | Runs |
| --- | --- |
| `forever` … `endforever` | until the game stops |
| `while (condition)` … `endwhile` | while the condition is true (checked at `endwhile`, so the inside runs at least once) |
| `repeat (val 10)` … `endrepeat` | the inside 10 times (new) |
| `for (var i, val 1, val 10)` … `endfor` | with `i` = 1, 2, … 10 (new). A 4th value sets the step: `for (var i, val 10, val 0, val -2)` gives 10, 8, 6, 4, 2, 0 |
| `wait_until (condition)` | pauses here until the condition is true (new) |
| `wait (val 1)` | pauses for that many seconds |
| `stop` | ends the script (new) |

Loops can go inside each other.

### Functions (new)

A function is a named piece of script you can run from anywhere in the same script with `call`:

```
call respawn (val 0, val 5)
var hp = val 100

func respawn
    object.set_pos(var arg1, var arg2, val 0)
    physics.stop()
endfunc
```

- **Values:** the values after the name arrive in the function as the variables `arg1`, `arg2`, ….
- **`return (val 5)`:** leaves the function early. With a value, the value goes into `data_return`, so
  `var x = data_return` works after the `call`.
- **Where to put them:** running down the script skips over a `func` … `endfunc` block, so functions can go
  anywhere, usually at the end.
- **Calls inside calls** work, up to 200 deep. `arg1`, `arg2`… (and `local` variables) belong to each call, so
  an inner call doesn't change the caller's. When the function returns, they get their old values back, or go away
  if they didn't exist before.
- **Don't** `return` from inside a `while` or `forever`.

### Events between objects (new)

Objects can send each other messages instead of checking each other's variables every frame:

```
// the switch
event.broadcast(str door_open)

// the door
event.wait_for(str door_open)
object.move(val 0, val 3, val 0)
```

- **Sending:** `event.broadcast` sends to every object. `event.send(str door, str open)` sends to one object, or
  to every object with that tag.
- **Receiving:**
  - `event.received(str name)` answers `true` once, the first time it's asked after the message arrived.
  - `event.wait_for(str name)` waits until the message arrives.
- Each object keeps its own messages until it takes them. A message sent twice before it's taken counts once.
- Starting the game again clears every message.

### Tags (new)

`object.set_tag(str enemy)` gives an object a tag. Clones keep their parent's tag, so a whole group of enemies
shares one.

- `object.nearest(str enemy)` gives the closest object with that tag.
- `object.count(str enemy)` gives how many there are.
- `object.broadcast_tag(str enemy, str freeze)` sends them all an event.
- `physics.touching(str enemy)` checks whether this object touches any of them.

---

## 4. Block reference

Blocks marked **new** were added by GluePP (in `Custom/ScriptBlocks.cpp`). The others are Glue3D's own.
Blocks marked **→** answer in `data_return`.

The block palette shows the same blocks, with short names such as "set position". The
**palette name** column gives that name.

### object: position, rotation, scale and objects

| Block | Palette name | What it does |
| --- | --- | --- |
| `object.go_to(val x, val y, val z, val speed)` | go to | Moves to a point. `speed 1` gets there at once; bigger numbers glide there more slowly. |
| `object.go_to_object(str object, val speed)` | go to object | Moves to another object, the same way. |
| `object.move(val x, val y, val z)` | move | Moves by that much on each axis. |
| `object.move_dir(val dir, val distance)` | directional move | Moves that far in a direction, in degrees. |
| `object.rotate_to(val rx, val ry, val rz, val speed)` | rotate to | Turns to these angles (speed works like go_to's). |
| `object.rotate_toward(str object, val speed)` | rotate towards | Turns to face another object. |
| `object.turn(val rx, val ry, val rz)` | turn | Turns by these angles. |
| `object.scale_to(val sx, val sy, val sz, val speed)` | scale | Scales to this size. |
| `object.distance(str object)` → | distance to object | The distance to another object. |
| `object.clone(str script)` | clone | Makes a copy of this object that runs the given script. |
| `object.delete()` | delete | Deletes this object. |
| `object.new_attribute(str object, str name, val value)` | new attribute | Gives an object a new variable. This block was in the palette but did nothing before; it works now. |
| `object.get_data(str object, str data)` → | get data | Reads a variable of another object. |
| `object.edit_data(str object, str data, val value)` | edit data | Sets a variable of another object. |
| `object.set_pos(val x, val y, val z)` **new** | set position | Jumps to a point. |
| `object.set_rotation(val rx, val ry, val rz)` **new** | set rotation | Sets all three angles. |
| `object.set_scale(val sx, val sy, val sz)` **new** | set scale | Sets the size on each axis. |
| `object.change_scale(val sx, val sy, val sz)` **new** | change scale | Adds to the size on each axis. |
| `object.get_pos(str x)` → **new** | get position | Its `x`, `y` or `z`. |
| `object.get_rotation(str x)` → **new** | get rotation | Its `rx`, `ry` or `rz` (give `x`, `y` or `z`). |
| `object.get_scale(str x)` → **new** | get scale | Its `sx`, `sy` or `sz` (give `x`, `y` or `z`). |
| `object.look_at(str object)` **new** | look at object | Faces another object at once (rotate_toward turns gradually). |
| `object.move_forward(val distance)` **new** | move forward | Moves the way it's facing (its `rx`). A negative distance moves it back. |
| `object.distance_to_point(val x, val y, val z)` → **new** | distance to point | The distance to a point. |
| `object.hide()` **new** | hide | Hides the object (its model is kept for show). |
| `object.show()` **new** | show | Shows it again. |
| `object.is_visible()` → **new** | visible? | `true` or `false`. |
| `object.exists(str object)` → **new** | object exists? | Whether an object with that name exists, e.g. after a clone was deleted. |
| `object.my_name()` → **new** | my name | This object's name. Useful in clones. |
| `object.set_tag(str enemy)` **new** | set tag | Gives it a tag, a group name shared by many objects. |
| `object.get_tag()` → **new** | get tag | Its tag (empty if none). |
| `object.nearest(str enemy)` → **new** | nearest with tag | The name of the closest other object with that tag (empty if none). |
| `object.count(str enemy)` → **new** | count with tag | How many objects have that tag, e.g. for wave spawners and win conditions. |
| `object.broadcast_tag(str enemy, str freeze)` **new** | event to tag | Sends an event to every object with that tag. |
| `object.clone_with(str script, str var_name, val value)` **new** | clone with value | Clones like `clone` and gives the copy a variable, e.g. its speed or team. |
| `object.parent()` → **new** | parent object | In a clone: the object that made it. |
| `object.last_clone()` → **new** | last clone | The name of the clone this object made last, so you can position it. |

`object.clone` now also gives the copy a `parent` variable and the parent's tag, and remembers it for `last_clone`.

### variable

| Block | What it does |
| --- | --- |
| `var name = val 5` / `str word` / `var other` | Makes or sets a variable. |
| `var name = data_return` | Copies the last answer. |
| `var name += val 1` | Adds. |
| `var name -= val 1` | Subtracts. |
| `var name *= val 2` | Multiplies. |
| `var name /= val 2` **new** | Divides. |
| `var name %= val 3` **new** | Keeps the remainder after dividing (modulo). |
| `var name ^= val 2` **new** | Raises to a power. |

`+=` `-=` `*=` `/=` `%=` `^=` also take `var other`, `global x` or math as the value.

| Line | What it does |
| --- | --- |
| `var i ++`, `var i --`, `var on toggle`, `var s .= str x`, `var hp clamp (val 0, val 100)`, `var hp min= val 100`, `var hp max= val 0`, `var a swap var b`, `var hp ?= val 100` **new** | The shortcuts (see "Shortcuts" in section 3). |
| `global score = val 0` **new** (and every operator) | A variable every object shares. Read it with `global score`. |
| `local tmp = val 0` **new** | A variable that lasts until the function returns. |
| `const gravity = val 0.9` **new** | A variable that can't change. |
| `saved var highscore = val 0` **new** | A variable loaded from, and saved to, `save.json` by itself. |
| `var slot[1] = str sword` **new** | An array item. Read it with `var slot[var i]`. |

#### var.* blocks (new)

| Block | What it does |
| --- | --- |
| `var.length(str slot)` → | How many items the array has. |
| `var.push(str slot, str shield)` | Adds an item to the end of the array. |
| `var.pop(str slot)` → | Removes the last item and answers it. |
| `var.clear_array(str slot)` | Empties the array. |
| `var.get(str name)` → | Reads a variable whose name is in a value, e.g. `var.get(var stat_name)`. |
| `var.set(str name, val 5)` | Sets a variable by name. |
| `var.exists(str hp)` → | Whether this object has it. |
| `var.delete(str hp)` | Removes it. |
| `var.names()` → | This object's own variable names, as `a,b,c,` (read them with `math.split`). |
| `var.copy_from(str player)` | Copies every variable of another object into this one (not its position, model or name). |
| `var.changed(str hp)` → | `true` once after the value changed since it was last asked. The first question only starts watching. |
| `var.previous(str hp)` → | The value before the last change `changed` saw. |
| `var.type(var x)` → | `number`, `text`, `bool` or `none`. |
| `var.is_number(var x)` → | Whether a value is a number, e.g. after `terminal.input` or `command_arg`. |
| `var.round_to(var x, val 2)` → | Rounded to that many decimals, for showing values with `interface.text`. |
| `var.strict(str true)` | From now on, reading a variable that doesn't exist prints a warning with the object and line (once per line), instead of quietly giving `null`. Catches typos like `var scroe`. |

### terminal: the output panel

| Block | What it does |
| --- | --- |
| `terminal.print(str hello_world, val color)` | Prints a word, a number (`val`) or a variable (`var`). |
| `terminal.clear()` | Empties the terminal. |
| `terminal.print_return(val color)` **new** | Prints `data_return`. |
| `terminal.print_value(str label, var name, val color)` **new** | Prints `label: value`, e.g. `score: 40`. |
| `terminal.warn(str message)` **new** | Prints `warning: message` in a warning color. |

#### Terminal tools (new)

A `print` inside `forever` fills the terminal with 60 lines a second. These blocks show less, show more about
where lines come from, and catch bugs for you. They work in built games too (see "overlay").

**Less spam**

| Block | What it does |
| --- | --- |
| `terminal.watch(str hp, var hp)` | One line, `hp: 40`, that updates in place. Call it every frame. Watched lines show in a box at the top-left of the game view. |
| `terminal.unwatch(str hp)` | Removes that line. |
| `terminal.count(str shots)` | A watched line counting how many times this block ran: `shots: 17`. |
| `terminal.print_once(str landed)` | Prints only the first time it's reached ("did this code ever run?"). |
| `terminal.print_every(val 1, var speed)` | Prints at most once every that many seconds. With a `var`, it prints `speed: 3`. |

**Who printed what, and when**

| Block | What it does |
| --- | --- |
| `terminal.show_source(str true)` | Puts the object and line number in front of every line: `[enemy3 :12] hit`. Useful with clones, which all run the same script. |
| `terminal.timestamps(str true)` | Adds the game time: `[4.25s] jumped`. |
| `terminal.format(str hp={}_ammo={}, var hp, var ammo)` | Fills in each `{}` with the next value: `hp=40 ammo=7`. |
| `terminal.trace()` | Prints the object, line and script, and the lines of the `call`s that led here. |

**Levels and filtering**

| Block | What it does |
| --- | --- |
| `terminal.info(str …)` / `success(str …)` / `error(str …)` | Prints at that level, in its color. `warn` (above) is the warning level. |
| `terminal.filter(str warn)` | Shows only that level and above (`info` < `success` < `warn` < `error`). `filter(str info)` shows everything again. |
| `terminal.mute(str enemy)` / `unmute(str enemy)` | Hides or shows the lines of an object, or of every object with that tag. |
| `terminal.only(str player)` | Shows only the lines of that object or tag (call it again to add more). |
| `terminal.show_all()` | Turns off filter, mute and only. |

**Catching bugs**

| Block | What it does |
| --- | --- |
| `terminal.assert(var hp >= val 0, str hp_went_negative)` | Does nothing while the condition holds. When it doesn't, it prints an error with the object and line. The condition is written like `if`'s, `and`/`or` included; the last value is the message. |
| `terminal.assert_pause(…)` | The same, and when it fails, everything pauses like `break`. |
| `terminal.break()` | A breakpoint: every script stops and this object's variables are printed. **F8** (or typing `continue` in the console) goes on. |
| `terminal.dump()` | Prints every variable of this object. |
| `terminal.dump(str player)` | Prints every variable of another object. |
| `terminal.print_list(str inventory)` | Prints a list one item per line, with its number. |

**Timing**

| Block | What it does |
| --- | --- |
| `terminal.time_start(str ai)` … `terminal.time_end(str ai)` | Prints how long the lines in between took: `ai took 0.48 ms`. |
| `terminal.fps_watch()` | A watched line with the frames per second and the number of objects. |

**The dev console:** while the game runs, press **`** (the key under Esc) to open it at the bottom-left of the game.
Type a line and press **Enter**. **`** or **Esc** closes it. While it's open, the game doesn't see your keys.

| Block | What it does |
| --- | --- |
| `terminal.command(str give)` → | `true` once after someone types a line starting with `give`. |
| `terminal.command_arg(val 1)` → | A word of that line: for `give coins 50`, 1 gives `coins`, 2 gives `50`, and 0 gives `give`. |
| `terminal.input(str name?)` → | Asks in the console and waits for the answer, like Scratch's "ask and wait". The answer goes into `data_return`, or straight into a variable with `var who = terminal.input(str name?)`. |

With these, you can make cheat codes and test commands (`teleport 0 5 0`, `spawn enemy`, `level level2`)
without changing scripts. See the example in section 5.

**Built games**

| Block | What it does |
| --- | --- |
| `terminal.overlay(str true)` | Shows the last lines of the terminal over the game view. A built game has no editor terminal; this is how you see it. |
| `terminal.log_to_file(str true)` | Also writes every line to `log.txt`: next to the game's `.exe`, or in the project folder in the editor. Players can send you the file when something breaks. |
| `terminal.max_lines(val 200)` | Keeps only the last that many lines, so a long game doesn't slow down. The default is 1000. |

Every terminal setting (watches, filters, show_source…) goes back to normal when the game starts again.

### other: control flow

| Block | What it does |
| --- | --- |
| `wait (val seconds)` | Pauses. |
| `if (a op b)` … `endif` | Runs the inside when the condition is true. |
| `else` **new** | Starts the part that runs when the `if` was false. |
| `forever` … `endforever` | Repeats forever. |
| `while (a op b)` … `endwhile` | Repeats while the condition is true. |
| `repeat (val n)` … `endrepeat` **new** | Repeats n times. |
| `for (var i, val from, val to, val step)` … `endfor` **new** | Counts `i` from `from` to `to`. The step is optional (1, or −1 when counting down). |
| `wait_until (a op b)` **new** | Waits until the condition is true. |
| `stop` **new** | Ends this object's script. |
| `func name` … `endfunc` **new** | A function (see "Functions" in section 3). |
| `call name (val 1, var x)` **new** | Runs a function. The values arrive as `arg1`, `arg2`, …. |
| `return (val 5)` **new** | Leaves the function. A value goes into `data_return`. |

### event: messages between objects (new)

These are in the **other** palette category.

| Block | What it does |
| --- | --- |
| `event.broadcast(str message)` | Sends a message to every object. |
| `event.send(str object, str message)` | Sends a message to one object, or to every object with that tag. |
| `event.received(str message)` → | `true` once after the message arrives. |
| `event.wait_for(str message)` | Waits until the message arrives. |
| `event.clear()` | Forgets this object's messages that haven't been taken. |

### save: progress that survives a restart (new)

These are in the **other** palette category. Values are saved straight away to `save.json`:
- in a built game, next to the game's `.exe`;
- in the editor, in the project folder.

| Block | What it does |
| --- | --- |
| `save.set(str highscore, var score)` | Saves a number or a word under a name. |
| `save.get(str highscore, val 0)` → | Reads it back. The second value is the answer when nothing was saved yet (0 if you leave it out). |
| `save.has(str highscore)` → | Whether something is saved under that name. |
| `save.delete(str highscore)` | Deletes one saved value. |
| `save.clear_all()` | Deletes everything saved. |

### debug: seeing what's going on (new)

These are in the **other** palette category.

| Block | What it does |
| --- | --- |
| `debug.show_hitboxes(str true)` | Draws every object's collision box (green) over the 3D view. `false` turns it off. |
| `debug.draw_line(val x1, val y1, val z1, val x2, val y2, val z2, val seconds)` | Draws a yellow 3D line for that long. Leave out the time for a line that lasts about a frame, for drawing one every frame. |
| `debug.clear()` | Removes the lines. |
| `debug.log(var hp)` | Prints `object: hp = 40` to the terminal. |

The palette also has ready-made `if not equal`, `if more or equal` and `if less or equal` blocks, which use the
new `!=`, `>=` and `<=` comparisons.

### game: players, cameras, input

| Block | Palette name | What it does |
| --- | --- | --- |
| `game.include_game()` | include game | Sets the object up for game.movement. Use it once, at the start. |
| `game.movement_type(str human)` | movement type | `human` walks, strafes and jumps with space; `vehicle` drives and steers. |
| `game.movement(val speed, val turn_speed, str controls, str view)` | movement | Player controls; use it inside `forever`. The last two values are explained under "Movement controls" below. |
| `game.key_pressed(str key)` → | key pressed? | `true` while the key is held. Keys are letters, digits, `space` and Scratch's key names. |
| `game.get_key()` → | get key | The key being pressed. |
| `game.set_camera(str camera)` | set camera | Makes a camera object the game camera. |
| `game.camera_1st(str object, val elevation)` | first person cam | A first-person camera on an object. |
| `game.camera_3rd(str object, val distance, val speed)` | third person cam | A camera that follows an object. |
| `game.fov(str fov)` | field of view | Sets the field of view. |
| `game.mouse_down()` → **new** | mouse down? | Whether the left mouse button is down. |
| `game.right_mouse_down()` → **new** | right mouse down? | Whether the right mouse button is down. |
| `game.any_key()` → **new** | any key pressed? | Whether any key is down. |
| `game.mouse_motion(str x)` → **new** | mouse motion | How far the mouse moved this frame (`x` or `y`) while it's locked or the right button is held. |
| `game.lock_mouse(str true)` **new** | lock mouse | Locks the mouse to the game (`true`) or frees it (`false`). **Esc** always frees it. |
| `game.mouse_locked()` → **new** | mouse locked? | Whether the mouse is locked. |
| `game.get_camera()` → **new** | get camera | The name of the game camera. |
| `game.message(str hello_world, val seconds)` **new** | show message | Shows the text at the top of the game for a while (2 seconds if you leave the time out). |
| `game.quit()` **new** | quit game | Closes a built game. In the editor it just stops the script. |
| `game.key_down(str e)` → **new** | key just pressed? | `true` once per press, unlike `key_pressed`, which is true the whole time. Use it for jumping, menus and shooting. |
| `game.key_up(str e)` → **new** | key just released? | `true` once per release. |
| `game.mouse_clicked()` → **new** | mouse clicked? | `true` once per click. |
| `game.load_scene(str level2)` **new** | load level | Switches to another level (see "Levels" in section 6). |
| `game.restart()` **new** | restart level | Starts the current level again from the beginning. |
| `game.current_scene()` → **new** | current level | The level loaded with load_scene (empty in the first one). |
| `game.pause(str true)` **new** | pause game | Stops every other object's script until `pause(str false)` (see below). |
| `game.is_paused()` → **new** | paused? | Whether the game is paused. |
| `game.camera_shake(val 3, val 0.5)` **new** | camera shake | Shakes the camera with that strength, fading out over that many seconds. |
| `game.gamepad_connected()` → **new** | gamepad connected? | Whether a gamepad is plugged in. |
| `game.gamepad_button(str a)` → **new** | gamepad button held? | Whether a button is held: `a` `b` `x` `y` `lb` `rb` `back` `start` `guide` `ls` `rs` `up` `down` `left` `right`. |
| `game.gamepad_pressed(str a)` → **new** | gamepad button pressed? | `true` once per press. |
| `game.gamepad_axis(str left_x)` → **new** | gamepad stick / trigger | `left_x` `left_y` `right_x` `right_y`: −1 to 1, where up and right are +. `lt` `rt`: 0 to 1. Small stick wobbles count as 0. |

**Key names** for `key_down` / `key_up`: letters, digits, `space`, `enter`, and `up`, `down`, `left`, `right`
for the arrow keys.

**Pause:**
- The object that calls `game.pause(str true)` keeps running, and so do UI elements, so a pause menu works.
- Every other object's script waits.
- Pausing stops scripts, not time: use `time.scale(val 0)` as well to freeze `delta`.

**Movement controls** (the last two values of `game.movement`):

- **controls** (`arrows`, `mouse` or `both`): what turns the view. The default is `arrows`.
- **view** (`left/right`, `up/down` or `both`): which way the view can turn. The default is `left/right`, as in
  the original. Spellings like `up\down` and `UP-DOWN` work too.
- **Arrow keys** look around in the directions the view allows. The other arrow keys move: Up/Down walk forward
  and back, Left/Right step sideways. W/A/S/D always move.
- **Looking up/down** tilts the game camera up to 60° each way. The tilt resets when the game stops.
- **With mouse controls**, click the game view to lock the mouse, and press **Esc** to free it. Holding the right
  mouse button works too.
- **`vehicle`:** W/S (or Up/Down) drive, A/D (or Left/Right) steer, and with mouse controls the mouse steers too.
- **Settings:** the mouse speed is `GameMouseSensitivity` and the tilt limit is `GameMaxPitch`, both in
  `Custom/Glue3DSprite.members.h`.
- **Template:** import > **+movement** gives a ready-made player script.

### physics

Start with `physics.include_physics()`. It gives the object velocity, gravity, friction and a hitbox. Then call
`physics.physics_tick()` every frame, inside `forever`, so it actually moves and collides.

| Block | Palette name | What it does |
| --- | --- | --- |
| `physics.include_physics()` | include physics | Turns physics on for this object. |
| `physics.physics_tick()` | physics tick | Moves the object by its velocity and handles gravity and collisions. Call it every frame. |
| `physics.set_factors(val gravity, val jump, val friction, val bounce)` | set factors | Sets all four at once. The defaults are 0.9, 1.5, 0.85 and 0.1. |
| `physics.hitbox(val x, val y, val z)` | set hitbox | The size of its collision box. |
| `physics.surface_collision(str true)` | surface collision | Whether it lands on floors. |
| `physics.hitbox_collision(str true)` | hitbox collision | Whether it bumps into walls. |
| `physics.add_force(str axis, val force)` | add force | Pushes along `x`, `y` or `z`. |
| `physics.dir_force(val direction, val distance)` | directional force | Pushes in a direction. |
| `physics.on_floor()` → | on floor ? | Whether it's standing on something. |
| `physics.collided()` → | collided | Whether it hit a wall. |
| `physics.set_velocity(val x, val y, val z)` **new** | set velocity | Sets its speed on each axis. |
| `physics.get_velocity(str x)` → **new** | get velocity | Its speed on `x`, `y` or `z`. |
| `physics.stop()` **new** | stop moving | Sets the velocity to 0. |
| `physics.jump()` **new** | jump | Jumps by its jump height, only when on the floor. `jump(val 3)` jumps by 3 instead. |
| `physics.set_gravity(val 0.9)` **new** | set gravity | Sets the gravity. |
| `physics.set_friction(val 0.85)` **new** | set friction | Sets the friction. |
| `physics.set_bounce(val 0.1)` **new** | set bounce | Sets the bounce. |
| `physics.set_jump(val 1.5)` **new** | set jump height | Sets the jump height used by `jump` and the space bar. |
| `physics.is_moving()` → **new** | moving? | Whether it's moving at all. |
| `physics.speed()` → **new** | speed | Its speed along the ground (x and z). |
| `physics.touching(str enemy)` → **new** | touching? | Whether its collision box overlaps an object with that name or tag. |
| `physics.touching_any()` → **new** | touching what | The name of an object it touches (empty if none). |
| `physics.raycast(val dir, val pitch, val distance)` → **new** | raycast | The first object along a line from this object's center (empty if none), for shooting, line of sight and ground checks. Its distance goes into the `ray_distance` variable. |

**Collision boxes** for `touching` and `raycast`:
- They are centered on the object.
- Their size is the physics hitbox (`physics.hitbox`, or 1 without physics) times the object's scale.
- Cameras and UI elements are left out. You don't need `include_physics` for these blocks.

**Raycast directions:**
- `dir` works like `move_dir` and the object's `rx`, so `physics.raycast(var rx, val 0, val 20)` looks the way the
  object faces.
- `pitch` is up (+) or down (−), so `val -90` looks straight down.
- A ray starting inside another object's box hits that object at distance 0.

### math

All math blocks answer in `data_return`.

**Numbers**

| Block | Answer |
| --- | --- |
| `add(val a, val b)`, `subtract`, `multiply`, `divide` | a + b, a − b, a × b, a ÷ b |
| `modulo(val a, val b)` | the remainder of a ÷ b |
| `power(val a, val b)` | a to the power b |
| `sqrt(val a)` | the square root |
| `abs(val a)` | the size without the sign |
| `floor(val a)` | rounded down |
| `round(val a)` **new** | rounded to the nearest whole number |
| `ceil(val a)` **new** | rounded up |
| `snap(val a, val step)` **new** | rounded to the nearest multiple of step |
| `sign(val a)` **new** | −1, 0 or 1 |
| `min(val a, val b)`, `max(val a, val b)` **new** | the smaller or the larger |
| `clamp(val a, val min, val max)` | a, kept between min and max |
| `interpolate(val a, val b, val c)` | moves from a toward b by 1/c, for smooth movement |
| `lerp(val a, val b, val t)` **new** | the point t of the way from a to b (t = 0.5 gives halfway) |
| `random(val a, val b)` | a random number from a to b |
| `random_int(val a, val b)` **new** | a random whole number from a to b, ends included |

**Angles and logarithms**

| Block | Answer |
| --- | --- |
| `sin`, `cos`, `tan`, `asin`, `acos`, `atan` | trigonometry, in degrees |
| `atan2(val y, val x)` **new** | the angle of a direction, in degrees |
| `pi()` **new** | 3.14159… |
| `log(val a)` | the logarithm |
| `exp(val a)` **new** | e to the power a |
| `ln(val a)` **new** | the natural logarithm |

**Distances**

| Block | Answer |
| --- | --- |
| `distance_2d(val x1, val y1, val x2, val y2)` **new** | the distance between two points |
| `distance_3d(val x1, val y1, val z1, val x2, val y2, val z2)` **new** | the distance between two 3D points |

**Text**

| Block | Answer |
| --- | --- |
| `join(str a, str b)` | a and b joined |
| `letter(val a, str b)` | letter number a of b |
| `length(str text)` **new** | how many letters |
| `contains(str text, str part)` **new** | `true` if the text contains the part, ignoring capitals |
| `upper(str text)`, `lower(str text)` **new** | the text in capitals or small letters |
| `split(str a-b-c, str -, val 2)` **new** | piece 2 when the text is cut at each `-` (`b`). Piece 0 gives how many pieces there are. Write `comma`, `space`, `dot` or `colon` for those separators, e.g. to read a `list.read_list` answer. |
| `substring(str text, val start, val length)` **new** | part of the text, from letter `start` (the first is 1) |
| `replace(str text, str find, str with)` **new** | the text with every `find` changed to `with`. `with` can be `space` or `nothing`. |
| `to_number(str 12.5)` **new** | the text as a number (0 if it isn't one) |

**Chance**

| Block | Answer |
| --- | --- |
| `chance(val 25)` **new** | `true` 25% of the time |

**Comparisons and logic** (answer `true` or `false`)

| Block | Answer |
| --- | --- |
| `equals`, `greater_than`, `less_than` | a = b, a > b, a < b |
| `not_equals`, `greater_equal`, `less_equal` **new** | a ≠ b, a ≥ b, a ≤ b |
| `and`, `or`, `xor`, `not` | on `true` and `false` values |

### model

| Block | Palette name | What it does |
| --- | --- | --- |
| `model.set_model(str model)` | set model | Shows a different model. |
| `model.refresh_models()` | refresh models | Reloads models after they've changed. |
| `model.set_bone(str bone)` | set bone | Gives the object a bone skeleton. |
| `model.rotate_bone(val rx, val ry, val rz, str bone)` | rotate bone | Turns a bone toward these angles, smoothly if easing is set. |
| `model.turn_bone(val rx, val ry, val rz, str bone)` | turn bone | Turns a bone by these angles. |
| `model.ease(val value)` | set easing | How smoothly `rotate_bone` moves. 0 means at once. |
| `model.get_model()` → **new** | get model | The model it shows (while hidden, the model it will show again). |
| `model.copy_model(str object)` **new** | copy model of | Uses the same model as another object. |
| `model.keyframe(str walk, val time, str bone, val rx, val ry, val rz)` **new** | add keyframe | Adds a key to an animation: at `time` seconds, that bone is at those angles. |
| `model.play_animation(str walk, str loop, val speed)` **new** | play animation | Plays it on this object's bones. `loop` repeats; `once` stops on the last key. The speed is optional (1 = normal). |
| `model.stop_animation()` **new** | stop animation | Stops it, keeping the pose. |
| `model.is_animating()` → **new** | animating? | Whether an animation is playing. |
| `model.bone_angle(str bone, str x)` → **new** | animated bone angle | The angle the animation has given a bone (`x`, `y` or `z`). |

**Animations:**
- They are shared by name: build one once (for example in the player's script), and any object with the same
  bones can play it.
- Between keys the angles move smoothly.
- The object needs its bones first (`model.set_bone`).
- Playing turns the bones with `turn_bone`, so you can still turn other bones yourself.

### sound

| Block | Palette name | What it does |
| --- | --- | --- |
| `sound.play(str sound_name)` | play sound | Plays a sound. |
| `sound.play_advanced(str id, str sound_name, val volume, val pitch)` | advanced play | Plays a sound you can change later by its id. |
| `sound.edit(str id, val volume, val pitch)` | edit sound | Changes a sound started with play_advanced. |
| `sound.stereo()`, `sound.mono()` | stereo sound, mono sound | Stereo or mono sound. |
| `sound.stop_all()` **new** | stop all sounds | Stops every sound. |
| `sound.volume(val 100)` **new** | set volume | The volume of `play`, from 0 to 100. |
| `sound.pitch(val 0)` **new** | set pitch | The pitch of `play`. 0 is normal; 10 is about one semitone. |
| `sound.play_at(str boom, str object, val 30)` **new** | play sound at object | Plays a sound that's quieter the further the object is from the camera. It goes silent at the range (30 if you leave it out). |

### interface: 2D UI elements

These work on **UI element** objects, which are 2D things drawn over the game. Their `x` and `y` are stage units
(−240…240, −180…180).

| Block | Palette name | What it does |
| --- | --- | --- |
| `interface.set_id(str costume_id)` | set costume to | Which picture it shows (`empty` = none). |
| `interface.text(str text)` | display text | Text to show on it. |
| `interface.set_pos(val x, val y)` | go to | Its position. |
| `interface.dir(val direction)` | set direction | Its angle. |
| `interface.scale(val scale)` | scale factor | Its size. |
| `interface.set_ghost(val value)` | ghost effect | See-through, from 0 to 100. |
| `interface.clickable(str true)` | set clickable | Lets it be clicked. |
| `interface.clicked()` → | is clicked? | `true` once after it's clicked. |
| `interface.get_mouse_axis(str axis)` → | mouse pos (x / y) | The mouse's `x` or `y`. |
| `interface.get_pos(str x)` → **new** | get position | Its `x` or `y`. |
| `interface.move_by(val x, val y)` **new** | move by | Moves it. |
| `interface.turn(val degrees)` **new** | turn | Turns it. |
| `interface.change_scale(val amount)` **new** | change scale | Grows it, or shrinks it with a negative amount. |
| `interface.hide()`, `interface.show()` **new** | hide, show | Hides or shows it. |
| `interface.get_text()` → **new** | get text | Its text. |
| `interface.hovered(val 20)` → **new** | mouse over? | Whether the mouse is within that many units of it. |
| `interface.color(val 50)` **new** | color | Changes the picture's color, like Scratch's color effect (0–200). |
| `interface.font_size(val 1.5)` **new** | text size | Text size; 1 is normal. |
| `interface.text_brightness(val 100)` **new** | text brightness | −100 is black text (the default) and 100 is white. |
| `interface.bar(var hp, val 100)` **new** | bar (health bar) | Draws a bar under the element, filled to value ÷ max. It goes from red to green. |
| `interface.hide_bar()` **new** | hide bar | Removes the bar. |

### time

| Block | Palette name | What it does |
| --- | --- | --- |
| `time.timer()` → | current timer | Seconds since the timer was reset. |
| `time.reset()` | reset timer | Resets the timer. |
| `time.ticks()` → | ticks from start | Frames since the start. |
| `time.timer_factor(val factor)` → | timer * factor | The timer times a number. |
| `time.tick_factor(val factor)` → | tick * factor | The ticks times a number. |
| `time.delta()` → | delta time | Seconds since the last frame. Multiply speeds by it for smooth movement. |
| `time.halt()` | stop script | Stops this object's script. |
| `time.fps()` → **new** | frames per second | Frames per second. |
| `time.date(str hour)` → **new** | date / time part | Part of the clock: `year`, `month`, `day`, `hour`, `minute`, `second`, or `weekday` (1 = Monday … 7 = Sunday). |
| `time.seconds()` → **new** | seconds since 1970 | The time as seconds since 1970, to measure long times or seed things. |
| `time.scale(val 0.5)` **new** | time scale | Slow motion (below 1) or fast forward (above 1). It multiplies `delta`, so movement that uses `delta`, `wait` and animations change speed. It goes back to 1 when the game starts again. |
| `time.get_scale()` → **new** | get time scale | The current time scale. |
| `time.cooldown(str shoot, val 0.5)` → **new** | cooldown ready? | `true` if at least that many seconds have passed since it last said `true` for that name. One block instead of a timer variable. |

### list

Lists are kept in your project's files. Make a file first, then a list in it.

| Block | Palette name | What it does |
| --- | --- | --- |
| `list.create_list(str name, str file)` | create list | Makes a list in a file. |
| `list.clear(str name)` | clear list | Empties it. |
| `list.add_item(str listname, str value)` | add item | Adds an item at the end. |
| `list.edit_item(str list, val index, str newvalue)` | edit item | Changes item number index (the first is 1). |
| `list.delete_item(str list, val index)` | delete item | Removes an item. |
| `list.read(str list, val index)` → | read item | Item number index. |
| `list.read_list(str list)` → | list reporter | The whole list, as `a,b,c,`. |
| `list.length(str list)` → | list length | How many items. |
| `list.contains(str list, str item)` → **new** | list contains? | Whether the item is in it. |
| `list.index_of(str list, str item)` → **new** | index of item | Where the item is (0 if it isn't there). |
| `list.insert_item(str list, val index, str item)` **new** | insert item at | Puts an item at that position. |
| `list.random_item(str list)` → **new** | random item | A random item. |
| `list.last_item(str list)` → **new** | last item | The last item. |
| `list.remove_last(str list)` → **new** | remove last item | Removes the last item and answers it. |
| `list.join(str list, str _)` → **new** | join items | All the items in one text, with this between them (`_` is a space). |

### file: your project's files

| Block | What it does |
| --- | --- |
| `file.create_file(str name, str true)` | Makes a file. With `true`, it's temporary and goes away when the game stops. |
| `file.delete_file(str name)` | Deletes a file. |
| `file.create_item(str type, str name, str file)` | Makes an item in a file. |
| `file.move_item(str name, str filename)` | Moves an item to another file. |
| `file.copy_item(str name, str filename, str copyname)` | Copies an item into another file, with a new name. |
| `file.delete_item(str name)` | Deletes an item. |

### editor: making things from a script

These blocks build models and objects while the game runs, for level generators and tools.

| Block | What it does |
| --- | --- |
| `editor.include_editor` | Turns these blocks on. |
| `editor.create_model(str name, str type, str file)` | Makes a model. |
| `editor.create_bone(str name, str file)` | Makes a bone. |
| `editor.create_object(str name, str script, str model)` | Makes a 3D object. |
| `editor.static_object(str name, str model)` | Makes an object without a script. |
| `editor.edit_mode7(str name, str texture)` | Sets a mode7 model's texture. |
| `editor.set_points(val number, val x, val y, val z)` | Sets a 3D point of the model being made. |
| `editor.add_quad(str modelname, val texture, val group)` | Adds a four-sided face. |
| `editor.set_2d_points(val number, val x, val z)` | Sets a 2D point, for segment models. |
| `editor.set_fake_height(val number, val ytop, val ybottom)` | Sets the top and bottom of a segment point. |
| `editor.add_seg(str modelname, val texture, val group, val surface)` | Adds a segment. |

---

## 5. Example scripts

**A player that walks, looks with the mouse, jumps and shows its speed**

```
physics.include_physics()
game.include_game()
game.movement_type(str human)
game.set_camera(str camera)
forever
    game.movement(val 0.1, val 1, str both, str both)
    physics.physics_tick()
    game.key_pressed(str f)
    var f = data_return
    if (var f = str true)
        physics.jump(val 3)
    endif
    physics.speed()
    var speed = data_return
endforever
```

**A coin that spins, and disappears when the player comes close**

```
forever
    object.turn(val 3, val 0, val 0)
    object.distance(str player)
    var d = data_return
    if (var d < val 1)
        object.get_data(str player, str coins)
        var c = data_return
        var c += val 1
        object.edit_data(str player, str coins, var c)
        game.message(str coin!, val 1)
        object.delete()
    endif
endforever
```

**Counting and timing**

```
var total = val 0
for (var i, val 1, val 10)
    var total += var i
endfor
terminal.print_value(str total, var total, val 0)     // prints "total: 55"

repeat (val 3)
    sound.play(str pop)
    wait (val 0.5)
endrepeat
```

**A start button (a UI element) that grows when the mouse is over it**

```
forever
    interface.hovered(val 30)
    var over = data_return
    if (var over = str true)
        interface.scale(val 110)
    else
        interface.scale(val 100)
    endif
    interface.clicked()
    var click = data_return
    if (var click = str true)
        game.message(str go!, val 2)
        interface.hide()
        stop
    endif
endforever
```

**A door that opens when a switch is touched (events + collision)**

```
// the switch's script
forever
    var hit = physics.touching(str player)
    if (var hit = str true)
        event.send(str door, str open)
        wait (val 1)
    endif
endforever

// the door's script
event.wait_for(str open)
repeat (val 30)
    object.move(val 0, val 0.1, val 0)
endrepeat
```

**Jump on the frame the key goes down, with a cooldown on shooting**

```
physics.include_physics()
forever
    physics.physics_tick()
    var jump = game.key_down(str space)
    if (var jump = str true)
        physics.jump()
    endif
    var fire = game.key_pressed(str f)
    var ready = time.cooldown(str shot, val 0.3)
    if (var fire = str true and var ready = str true)
        object.clone_with(str bullet, str speed, val 2)
        sound.play_at(str pew, str player)
    endif
endforever
```

**A wave spawner using tags**

```
var wave = val 1
forever
    var left = object.count(str enemy)
    if (var left = val 0)
        repeat (var wave)
            object.clone_with(str enemy_ai, str hp, val 3)
        endrepeat
        var wave += val 1
        game.message(str next_wave!, val 2)
    endif
endforever
```

The clone's script `enemy_ai` starts with `object.set_tag(str enemy)` and chases the player:

```
object.set_tag(str enemy)
forever
    object.rotate_toward(str player, val 10)
    object.move_forward(val 0.05)
    if (var hp < val 1)
        object.delete()
    endif
endforever
```

**A high score that's kept after the game closes**

```
var best = save.get(str highscore, val 0)
if (var score > var best)
    save.set(str highscore, var score)
    game.message(str new_high_score!, val 3)
endif
```

**A function with values, and a pause key**

```
forever
    var p = game.key_down(str p)
    if (var p = str true)
        game.is_paused()
        var paused = data_return
        if (var paused = str true)
            game.pause(str false)
        else
            game.pause(str true)
        endif
    endif
    var r = game.key_down(str r)
    if (var r = str true)
        call teleport (val 0, val 5, val 0)
    endif
endforever

func teleport
    object.set_pos(var arg1, var arg2, var arg3)
    game.camera_shake(val 3, val 0.4)
endfunc
```

**Gamepad movement**

```
forever
    var sx = game.gamepad_axis(str left_x)
    var sy = game.gamepad_axis(str left_y)
    var sx *= val 0.1
    var sy *= val 0.1
    object.move(var sx, val 0, var sy)
endforever
```

**Cheat codes in the dev console**

```
forever
    var tp = terminal.command(str teleport)
    if (var tp = str true)
        var x = terminal.command_arg(val 1)
        var y = terminal.command_arg(val 2)
        var z = terminal.command_arg(val 3)
        object.set_pos(var x, var y, var z)
        terminal.success(str teleported)
    endif
    var lv = terminal.command(str level)
    if (var lv = str true)
        var name = terminal.command_arg(val 1)
        game.load_scene(var name)
    endif
    terminal.watch(str position, var x)
endforever
```

**Debugging a player without flooding the terminal**

```
terminal.show_source(str true)
terminal.fps_watch()
forever
    terminal.watch(str hp, var hp)
    terminal.watch(str state, var state)
    terminal.assert_pause(var hp >= val 0 and var hp <= val 100, str hp_out_of_range)
    var hit = physics.touching(str enemy)
    if (var hit = str true)
        terminal.count(str hits_taken)
        terminal.print_once(str first_hit)
    endif
endforever
```

**A walk animation made from keyframes**

```
model.set_bone(str hero_bones)
model.keyframe(str walk, val 0, str leg_l, val 30, val 0, val 0)
model.keyframe(str walk, val 0.5, str leg_l, val -30, val 0, val 0)
model.keyframe(str walk, val 1, str leg_l, val 30, val 0, val 0)
model.play_animation(str walk, str loop, val 1)
```

---

## 6. Building your game

**File > build..** makes a game you can give to players. It has three settings:

- **Fullscreen:** start in fullscreen.
- **Intro:** play the Glue3D logo animation. When it's off, the game starts straight away.
- **Custom loading screen:** pick a PNG or JPG. The game shows it while it loads, instead of a black screen. The
  picture is copied into your project folder (`loading-screen.png`), and the red button removes it.

The result:

```
Projects/my-game/build/
  my-game.exe          <- the game (no editor inside)
  data/project.glue3d  <- your project
  data/build.json      <- the build settings
  data/loading.png     <- your loading screen picture (if you picked one)
  data/assets/         <- costumes, sounds, font
```

Give players the whole `build` folder. The game needs a camera: add a camera object and use **assign camera** (or
`game.set_camera`), otherwise the game view is black.

### Levels

**Each level is its own project.** Make `level2` as a normal project in the start menu. Then
`game.load_scene(str level2)` in any script switches to it: its objects, scripts, models and map replace the
current ones, and it starts running.

- **In the editor:** the level loads from `Projects/level2`. Stopping the game brings back the project you were
  editing. While another level plays, saving is blocked, so the level can't overwrite your project.
- **In a build:** File > build.. finds every `load_scene(str …)` in your project, and in those levels too, and
  copies those projects into `data/levels/`.
- **What carries over:** saved values (`save.…`) do, so use them to carry score or lives from level to level.
  Events and variables don't.
- **Restarting:** `game.restart()` starts the current level again from the beginning.

### Saved progress

`save.set` writes `save.json`. In a built game it's next to the `.exe`; in the editor it's in the project folder.
Delete the file to start fresh.

### How "build" works without a compiler

It works the way Unity and Godot do. The editor comes with a prebuilt **game player** and copies it next to your
game data, so nothing is compiled when you press build.

The player (`Glue3DPlayer`) is the same code compiled with `GLUE3D_PLAYER` defined:

- `BuildFlavor::IsEditor` is `false`, so every editor-only branch is left out (`Custom/BuildFlavor.h`).
- `tools/glue3d_edits.py` works out which generated methods a game can never reach: 89 of Glue3D's 195 methods,
  such as the script editor, model editor, settings and menus. They're wrapped in `#ifndef GLUE3D_PLAYER`, so they
  aren't compiled into the player at all.
- The start menu, gizmos, saving and build dialog (`Custom/MainMenu.cpp`, `SceneGizmo.cpp`, `BuildGame.cpp`)
  aren't in it either. `Player/PlayerStubs.cpp` stands in for them.
- The script blocks (`Custom/ScriptBlocks.cpp`, `Custom/ScriptSystems.cpp`) **are** in the player, so every block
  works in a built game.

---

## 7. How the code is organized

| Path | What it is |
| --- | --- |
| `Generated/Glue3DSprite*.g.cpp` | **All of Glue3D's scripts**: about 15,000 lines of C++. Each custom block is a method; each "when ..." script is a coroutine. |
| `Generated/Glue3DSprite.g.h` | The sprite class: its methods, plus the hand-written members from `Custom/`. |
| `Generated/Stage.g.h` | The project's 531 global variables and 44 lists. |
| `Generated/Project.g.cpp` | Creates the stage and the sprite in their starting state. |
| `runtime/` | A small Scratch engine in C++ on OpenGL (see below). |
| `Custom/` | Hand-written additions. The converter never overwrites them. |
| `app/main.cpp` | The editor's `main()`, plus the hooks for automated tests. |
| `Player/` | The game player's `main()` and its stand-ins for editor-only features. |
| `assets/` | 278 costumes (SVGs pre-rendered to PNG), 120 sounds, the font, and the starting list contents (including the block palette). |
| `tools/` | The converter (`sb3_to_cpp.py`), the asset renderer and `glue3d_edits.py`. See `tools/README.md`. |
| `third_party/` | The bundled libraries. See `third_party/README.md`. |

### The `Custom/` files

| File | What it adds |
| --- | --- |
| `Glue3DSprite.members.h` | Declarations of the hand-written members, pulled into the generated class like a C# partial class. The camera, mouse and scroll settings are here. |
| `ScriptBlocks.cpp` | **The new script blocks**, the `!=` `>=` `<=` `contains` comparisons, `else`/`repeat`/`for`/`wait_until`, and the palette helpers. |
| `ScriptVariables.cpp` | Variables: every `var`/`global`/`local`/`const`/`saved var` line, the math and condition parser, shortcuts, arrays, the `var.*` blocks and the editor's variables panel. |
| `ScriptTerminal.cpp` | The terminal tools: watches, levels and filters, asserts and breakpoints, timers, the dev console, the overlay and log file. |
| `ScriptSystems.cpp` | The bigger script features: events, functions, collision and raycasts, tags, single key presses, saving, levels, pause and time scale, clone helpers, answers inside a line, `and`/`or` conditions, text helpers, camera shake, gamepads, 3D sound, UI styles and bars, keyframe animation and debug drawing. |
| `MainMenu.cpp` | The start menu, creating and loading projects, saving, Ctrl+S. |
| `SceneGizmo.cpp` | The scene toolbar and the gizmos. |
| `CameraControls.cpp` | The Unity-style camera and game.movement's mouse and arrow controls. |
| `BuildGame.cpp` | File > build.. |
| `ProjectFiles.cpp` | The project folder format. |
| `PlayerSettings.cpp` | The build settings. |
| `LoadingScreen.cpp` | The editor's loading screen and a game's loading picture. |
| `Scrolling.cpp` | Mouse-wheel scrolling for scrollbars. |

### Adding your own block

1. In `Custom/ScriptBlocks.cpp`, find the function for the category, such as `GameBlocks()`, and add a case:

   ```cpp
   if (c == u"double_it")                 // game.double_it(val 4)
   {
       SetReturn(Arg(0).Num() * 2);       // Arg(n) reads value n (it handles var too)
       return true;                       // true = handled
   }
   ```

2. To show it in the palette, add `("game", "double it", "double_it(val 4)")` to that category's list in
   `PALETTE` in `tools/glue3d_edits.py`. The first item is the icon name.
3. If it answers something, add its name to `BlockReturnsValue()`. Dropping the block then also adds
   `var variable = data_return`.
4. Regenerate and build:

   ```bash
   python3 tools/sb3_to_cpp.py <unzipped sb3> Generated assets --namespace Glue3D --title Glue3D \
       --rename "\`=Glue3D" --members "Glue3DSprite=../Custom/Glue3DSprite.members.h"
   python3 tools/glue3d_edits.py Generated
   cmake --build build
   ```

   If you only changed `ScriptBlocks.cpp`, just build.

### How the Scratch blocks map to C++

```cpp
// Scratch: when green flag clicked / forever / broadcast [Tick]
Script Glue3DSprite::WhenGreenFlagClicked()
{
    StartEngine(0);
    while (true)
    {
        Broadcast(u"Tick"_s);
        co_yield NextFrame; // Scratch waits for the next frame here
    }
}
```

- **Scripts** are C++20 coroutines (`Script`). `co_yield NextFrame;` is where Scratch waits a frame.
  - A custom block that can pause is called with `co_await`.
  - A "run without screen refresh" block is a plain method.
- **Values** are `Value` objects: a number, text or true/false, converted with Scratch's rules.
  - `.Num()`, `.Str()` and `.Bool()` read them as each type.
  - Text is UTF-16, like JavaScript's.
- **Text literals** are written `u"text"_s`.
- **Lists** are `ScratchList` objects with Scratch's 1-based indexes, for example `stage.ramEngineData.Item(79)`.
- **Operators** (`Eq`, `Lt`, `Gt`, `Mod`, `Round`, `Sin`…) come from `runtime/Ops.h`. Comparisons follow Scratch:
  they ignore capitals, and `"10" = 10`.

### How Glue3D runs a script line

- `Glue3dExecute` runs a few lines of every object's script per frame. Each object keeps its own **program
  counter**.
- `RunLineMainObjectScript` splits a line into elements, and the category picks a `Proccess…` method
  (`ProccessObject`, `ProccessGame`…).
- GluePP puts `CustomCommand()` in front of those methods. A line it doesn't know goes on to Glue3D's own blocks.
  `CustomCommand()` asks `SystemsCommand()` (`ScriptSystems.cpp`) first.
- **Control flow** works by moving the program counter (`JumpTo`):
  - `else`, `repeat`/`for` and `func`/`call`/`return` find their matching lines by counting nested blocks.
  - Waiting (`wait_until`, `event.wait_for`) holds the counter in place with the object's wait value.

### The runtime (`runtime/`)

| File | What it does |
| --- | --- |
| `Value.*` | Scratch values and strings, with JavaScript-exact number parsing and printing |
| `Ops.*`, `ScratchList.*` | Operator blocks, comparisons and lists |
| `Script.h` | Coroutines for scripts |
| `Target.*` | Stage and sprite blocks: motion, looks, pen, sound, clones and colors |
| `Engine.*` | The window and main loop. It runs scripts frame by frame like Scratch, at 60 fps. |
| `Gfx.*` | A small OpenGL 3.3 2D renderer: batching, render targets, shapes and text |
| `Renderer.*`, `Shaders.h` | The pen layer and stamps, using GLSL ported from Scratch's renderer |
| `Input.*` | Mouse and keyboard through GLFW, including mouse lock |
| `Audio.*` | Sound through miniaudio, with per-sprite volume, pitch and pan |
| `Assets.*`, `Platform.*` | Costumes, sounds and the manifest, the file picker, and UTF-8 file paths |
| `Monitors.*` | On-stage list displays and the UI font |
| `Automation.*` | Scripted input for tests. It's off unless `SCRATCH_AUTOMATION` is set. |

---

## 8. Changes from the original Scratch project

Each change is marked `// EDITED` in `Generated/` and applied by `tools/glue3d_edits.py`.

| Change | Where |
| --- | --- |
| Start menu (create / load project) after the welcome popup | `Glue3DAll`, `Custom/MainMenu.cpp` |
| "save project.." and Ctrl+S really save into the project folder | `MainbarFile`, `Custom/MainMenu.cpp`, `Custom/ProjectFiles.cpp` |
| File menu: "import files" removed, **build..** added | `ElementMainBar`, `MainbarFile`, `Custom/BuildGame.cpp` |
| "export program" removed (Settings dropdown, sidebar button and tab) | `ElementSideBarSelector`, `ElementConfigureTabs`, `ConfigurationTabs` |
| Right-mouse look instead of the arrow keys; ±90° pitch; wheel zoom, middle-drag pan, Q/E | `EngineFreeCamera`, `Custom/CameraControls.cpp` |
| The sky image stops sliding past 45° | `EngineScreenRender` |
| `game.movement` gets **controls** and **view** values (mouse look, looking up/down) | `ProccessGame`, `GameMoveplayer`, `Custom/CameraControls.cpp`, the movement template |
| A dragged block or item keeps its color (it used to flicker) | `Render2D…`, `MainAfterControl` |
| Scene toolbar and gizmos | `Custom/SceneGizmo.cpp`, hooked up in `app/main.cpp` |
| The mouse wheel scrolls every scrollbar | `ScrollerElement`, `Custom/Scrolling.cpp` |
| **About 95 new script blocks and `else`** | `RunLineMainObjectScript` (calls `CustomCommand`), `Operation` (calls `CustomOperation`), `ProccessOther` (`else`), `Custom/ScriptBlocks.cpp` |
| The palette lists the new blocks, scrolls to the end of long categories, and adds `var … = data_return` after more blocks | `BlockCategories`, `BlockBasedEditor`, the `PALETTE` table in `tools/glue3d_edits.py` |
| The block editor indents `repeat`, `for` and `else` | `BlockBasedEditor` |
| `object.new_attribute` works (it was in the palette but did nothing) | `Custom/ScriptBlocks.cpp` |
| Functions, events, tags, collision, saving, levels, gamepads, animation and the other `ScriptSystems.cpp` blocks | `Custom/ScriptSystems.cpp`; dropping `func` adds `endfunc` (`BlockBasedEditor`) |
| `and` / `or` / `not` conditions | `CustomOperation`, which `Operation` calls first |
| Terminal tools: every terminal line goes through `TerminalHook` (source, time, filters, log file, line limit) | `TerminalPrint`, `Custom/ScriptTerminal.cpp` |
| The block palette's scrollbar fits the category, and a wheel notch moves at least one block | `ElementSideBar`, `EditorTexteditor` |
| Long block names in the palette shrink to fit their block | `BlockCategories` |
| Variable lines (`var`, `global`, `local`, `const`, `saved var`) and conditions are read by GluePP's parser, so they can use math, brackets, globals and arrays. Glue3D's own forms work the same as before. | `Custom/ScriptVariables.cpp`, `CustomOperation` |
| Typing into the dev console doesn't press the game's keys | `runtime/Input` (`TextCapture`) |
| `game.pause` | `Glue3dExecute` skips paused objects |
| `time.scale` | `MainTime` multiplies the delta time |
| `game.camera_shake` | `MainControl` adds the shake to the camera position |
| `interface.color`, `font_size`, `text_brightness` | `EngineRenderEngineSprites` reads them when it draws a UI element |
| **Bug fix:** reading or setting a variable an object doesn't have no longer finds or changes the variable of the object after it in the scene | `EditDataInObjectDataOperation` stops at the end of the object |
| Projects keep the newest block palette, even when they were saved by an older version | `Custom/ProjectFiles.cpp` doesn't load the palette, block colors or import templates from the project file |
| Editor-only code is left out of the game player | `BuildFlavor::IsEditor` checks, `#ifndef GLUE3D_PLAYER` around 89 methods |

---

## 9. How it was checked

- **Against the C# version:** both versions were given the same scripted clicks. The welcome screen, start menu,
  "new project" dialog and editor matched pixel for pixel, apart from the folder path on screen and a tooltip
  caught at a different point in its fade. The 3D object positions matched exactly.
- **Projects:** a project saved by the C# version loads here, and saving and reloading a C++ project brings back
  the objects and camera exactly.
- **Script blocks:** test scripts ran through Glue3D's real interpreter, using the `runscript` test hook, and every
  result was checked. Among them:
  - `repeat` inside `repeat` gives 4 × (1 + 3 × 10) = 124;
  - `for` counts up, and down with a step of −3;
  - `if`/`else` works when nested;
  - `!=`, `>=` and `<=`;
  - `wait_until` waits until another object changes the variable;
  - the math, object, physics, list, interface and time blocks;
  - events between objects (`broadcast`, `send`, `received`, `wait_for`);
  - functions with values and `return`, including a call inside a call;
  - `touching`, `touching_any` and `raycast`, `nearest` and `count`;
  - `key_down` counting 2 for 2 presses of a held key;
  - `and`/`or`/`not` conditions in `if`, `else` and `while`;
  - answers inside a line, and the text helpers;
  - saving to `save.json`, `clone_with`/`parent`/`last_clone`, `cooldown`, `pause` (another object's counter stops,
    then goes on);
  - `load_scene` to another project and back when the game stops, and `restart`;
  - camera shake offsets, gamepad answers with no gamepad, and the UI bar and color, hitboxes and debug lines on
    screen;
  - the terminal tools: format, print_once, levels with filter and mute, trace through a `call`, assert, time_start
    and time_end, dump, log.txt and max_lines, watches and the overlay on screen, `input` and `command` answered
    from the console, and `break` stopping another object's counter until `continue`;
  - typing into the console with real key presses (letters, Backspace, Enter, the ` key);
  - variables:
    - `global` with `+=`, and math with brackets and powers (`(4 − 1) ^ 2 + 1 = 10`);
    - `-=` with `*`, an `if` with math and a global, and a `while` whose condition has brackets;
    - a block that answers inside math;
    - every shortcut;
    - arrays with a variable index, push and pop;
    - every `var.*` block;
    - a const refusing a change, with an error;
    - locals and `arg1` getting their values back after nested calls;
    - a `saved var` loading its start value and saving its change;
    - `strict` warning about a typo;
  - the live variables panel on screen, and long palette names shrinking to fit.

  The same tests gave the same results in the Windows build under Wine.
- **Editor:** palette categories, the short scrollbar of a short category and the full one of a long category
  (one wheel notch each), scrolling to the last block, dragging `repeat` (its `endrepeat` is added and
  indented), the gizmos, mouse look, saving and building.
- **The built game:** the intro on and off, the custom loading picture, and the game view. A build of a project
  that loads `level2` contains `data/levels/level2`. In the built game, `load_scene` switched to it and `save.json`
  was written next to the game.
- **Not tested with real hardware or assets:** a real gamepad, and keyframe animation on a model with real bones.
  The animation timing and angles were checked, but not the bones moving on screen.
- **Platforms:**
  - Built with GCC on Linux and with MinGW for Windows. The Windows editor and game were run under Wine, and the
    game's screen matched the Linux one pixel for pixel. It also compiles cleanly with Clang.
  - **Not yet tested:** a Visual Studio build, or real Windows hardware.

---

## 10. Known differences from Scratch/TurboWarp

- SVG costumes are pre-rendered to PNG at 4–8× resolution, using Scratch's fonts.
- Effects and sizes treat NaN as JavaScript does, so a NaN effect is skipped instead of becoming an extreme value.
- `Infinity - Infinity` and `0 × Infinity` give NaN, where Scratch would first turn the NaN into 0. This only
  happens with infinite values.
- Only the blocks this project uses were converted. If you convert another project, any others show up as
  `// (unsupported block: ...)` comments.
