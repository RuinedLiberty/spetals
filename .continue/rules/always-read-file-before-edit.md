---
globs: "**/*.*"
description: Prevents mangled edits and ensures context-aware changes.
---

Before modifying any file, always use the read_file tool to fetch the latest content and plan all edits to that file in a single multi_edit call.