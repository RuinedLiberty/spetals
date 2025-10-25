---
globs: gardn.bat
description: Ensures the server can be configured both on localhost and deployed
  server without manual shell commands. Applies to launcher scripts and Node
  bootstrap code.
---

When adding environment variables for local dev on Windows, load from .env (root) and Server/.env in batch scripts; also support Server/config.json for production where env vars are not injected.