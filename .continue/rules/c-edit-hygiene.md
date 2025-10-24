---
description: Prevents atomic multi_edit failures due to stale context when
  editing C++ sources.
alwaysApply: true
---

Before using multi_edit to modify a file, always run read_file to fetch its current contents to avoid mismatched edits.