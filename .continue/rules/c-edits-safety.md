---
description: Prevents mismatch errors due to file drift and ensures atomic,
  consistent edits when introducing new features like bots.
---

Always run functions.read_file on a source file right before editing it with functions.multi_edit, and plan all changes for that file in a single multi_edit call.