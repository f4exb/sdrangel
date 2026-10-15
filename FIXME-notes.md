## Enable Git pre-commit hooks
SDRangel provides a Git pre-commit hook to check staged changes for trailing
whitespace before a commit is created. The hook is maintained in
`.githooks/pre-commit` and uses `.gitattributes` to determine which files to
check.

To enable the hook, run the following command from your repository root:
```bash
git config core.hooksPath .githooks
```

This configuration is local to your clone and only needs to be set once.
After enabling it, Git automatically runs the hook before each commit. If
trailing whitespace is detected, the commit is rejected. Correct the affected
lines, stage the changes, and try again.

The hook is shared through the repository, but enabling it is a per-clone setup
step. GitHub Actions also checks pull requests for trailing whitespace, providing
a server-side check independent of the local hook.

## meshtasticdemodgui.cpp - checkbox signal pattern
All checkboxes currently use stateChanged(int) pattern.
Consider refactoring all to clicked(bool) for simplicity in a future cleanup commit.
Affects: sendViaUDP, sendJsonViaUDP, and likely other checkboxes in the same file.

## meshtasticdemod.cpp:796 - APRS guard
m_lastMsgBytes[0] is read without an explicit non-empty size check before the APRS detection block.
Pre-existing issue, not introduced by JSON UDP feature. Fix separately in its own commit.
Safe fix: add `&& !m_lastMsgBytes.isEmpty()` to the APRS condition.

## meshtasticdemod.cpp:658 - hash_matching_index fabricated
hash_matching_index is hardcoded to 0 for decrypted packets and "none"
for all others. The decoder sorts key candidates by hash match but does
not expose the actual index used in DecodeResult. The field is therefore
inaccurate for multi-key configurations. Fix requires extending
DecodeResult to expose the matched key index, then wiring it through
buildMeshtasticJsonPacket. For now the field remains but should not be
relied upon.
