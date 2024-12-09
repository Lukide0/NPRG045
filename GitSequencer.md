# Git Sequencer Rebase

## Začátek Rebase

1. **Uložení počátečního stavu**
   Funkce: [`init_base_state`](https://github.com/git/git/blob/e66fd72e972df760a53c3d6da023c17adfc426d6/builtin/rebase.c#L292)
2. **Připravení sequencer skriptu**  
   Funkce: [`sequencer_make_script`](https://github.com/git/git/blob/e66fd72e972df760a53c3d6da023c17adfc426d6/builtin/rebase.c#L306)
3. **Spuštění skriptu**  
   Funkce: [`complete_action`](https://github.com/git/git/blob/e66fd72e972df760a53c3d6da023c17adfc426d6/builtin/rebase.c#L318C9-L318C24)
4. **Spuštění editoru**  
   Funkce: [`edit_todo_list`](https://github.com/git/git/blob/e66fd72e972df760a53c3d6da023c17adfc426d6/sequencer.c#L6505C8-L6505C22)
5. **Checkout na `--onto`**  
   Funkce: [`checkout_onto`](https://github.com/git/git/blob/e66fd72e972df760a53c3d6da023c17adfc426d6/sequencer.c#L6550)
6. **Provedení příkazů v TODO souboru**  
   Funkce: [`pick_commits`](https://github.com/git/git/blob/e66fd72e972df760a53c3d6da023c17adfc426d6/sequencer.c#L4952)
7. **Odstranění adresáře sequencer**

## Příkazy v TODO souboru

- **pick**
  - Provádí `cherry-pick`.
- **revert**
  - Vrací změny z commitu ("anti commit").
- **reword**
  - Provádí `pick` a následně `commit --amend`.
- **squash a fixup**
  - Kombinace `cherry-pick --no-commit` a `commit --amend`(používá se, pokud předchozí commit nebyl označen jako `fixup` nebo `squash`)
  - Commit zpráva se přidá do ["message-squash" nebo "message-fixup"](https://github.com/git/git/blob/e66fd72e972df760a53c3d6da023c17adfc426d6/sequencer.c#L115C1-L115C76).
- **exec**
  - Spustí zadaný příkaz během rebase.
- **break**
  - Zastaví rebase na aktuální pozici.
- **drop**
- **label**
  - Uloží novou referenci.
- **reset**
- **merge**
- **update-ref**
