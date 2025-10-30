---
globs: Client/Ui/TitleScreen/*.cc
description: Prevents build/link errors by keeping SRCS up to date and ensures
  UI is visible by wiring it in Game::init
alwaysApply: false
---

Whenever you add a new source file under Client/Ui/TitleScreen, ensure Client/CMakeLists.txt adds it to SRCS and the title UI window creates the element in Game::init.