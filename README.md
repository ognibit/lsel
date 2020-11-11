# lsel - Line(s) Selector
`lsel` is inspired by `fzf` and `dmenu` (actualy `slmenu`) and it would be 
a simple and small text stream selector.
It takes the `stdin` and display it line by line on the terminal, in a paged
mode and gives you two only one feature: select a row to put on the `stdout`.
To select a line you have just to press arrow up or down and hit enter to
confirm. To get non output press ESC two times.

The lines are diplayed in the same order they appears on the stdin. If you need
some sort, please add a sort command (es: `sort`) in UNIX style

```
cat file | sort | lsel
```

## Autoselection
With the option `-a` the cursor also select the line, without using TAB.
Can be useful to build a menu, like using *dmenu* or *fzf*.

The autoselection works only in default mode, not in multiselction mode.

## Prompt
With the option `-p` you can add a prompt message to the search bar.


## Matching
To help the line selection you can write some character in order to match them
on each line: it'll display the line with match(es). The output line is the
selected one, so if the matching lines are more than one, only the selected
will put on output.

To select, use TAB.

## Paging
Use pageup and pagedown to scroll faster.

## Case (In)sensitive
The default match is case sensitive. Add the option `-i` to be case
insensitive.

## Multiselection
Select more lines and output them. Active the `-m` option.
It cannot be the default because in a lot of case, for scripting, we need zero
or one line at least as result. If we need a multiline selection, for
extracting portion of a document/stream, it must be specified.

To select, use TAB.

```
dmesg | lsel -m > log_seletion.log
```

