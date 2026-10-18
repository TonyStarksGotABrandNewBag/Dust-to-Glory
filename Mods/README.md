# Mods

A mod is a folder in here holding any of the game's data files. Whatever it contains replaces the shipped
version of that file; everything it leaves out is untouched. Several mods can be installed at once, and
later folders (alphabetically) win.

```
Mods/
  bigger-engines/
    parts.json        <- replaces the shipped parts list
  my-home-town/
    state-ohio.json   <- a whole new state, picked from the front end
```

The files are the same ones in `Content/Data`, documented in
[docs/CONTENT_GUIDE.md](../docs/CONTENT_GUIDE.md). Copy one out of there, change it, and restart the game.
Any file beginning with `state` turns up as a place to play in the front end's list.

The game checks its data on load and says in the log which mod each file came from. Nothing here is signed
or sandboxed: a mod is a text file, and a broken one shows up as a content error rather than a crash.
