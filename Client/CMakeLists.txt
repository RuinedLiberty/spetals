set(CMAKE_SYSTEM_NAME Generic)

cmake_minimum_required(VERSION 3.16)

project(gardn-client)
include_directories(..)

set(SRCS
    Assets/Flower.cc
    Assets/Mob.cc
    Assets/Petal.cc
    Assets/Web.cc
    Assets/Crown.cc
    # Per-petal drawings
    Assets/Petals/AntEgg.cc
    Assets/Petals/Antennae.cc
    Assets/Petals/Azalea.cc
    Assets/Petals/Basic.cc
    Assets/Petals/BeetleEgg.cc
    Assets/Petals/Bone.cc
    Assets/Petals/Bubble.cc
    Assets/Petals/Cactus.cc
    Assets/Petals/Corn.cc
    Assets/Petals/Cutter.cc
    Assets/Petals/Dahlia.cc
    Assets/Petals/Dandelion.cc
    Assets/Petals/Faster.cc
    Assets/Petals/Heaviest.cc
    Assets/Petals/Heavy.cc
    Assets/Petals/Iris.cc
    Assets/Petals/Leaf.cc
    Assets/Petals/Light.cc
    Assets/Petals/Lotus.cc
    Assets/Petals/Missile.cc
    Assets/Petals/None.cc
    Assets/Petals/Peas.cc
    Assets/Petals/Pincer.cc
    Assets/Petals/PoisonCactus.cc
    Assets/Petals/PoisonPeas.cc
    Assets/Petals/Pollen.cc
    Assets/Petals/Rice.cc
    Assets/Petals/Rock.cc
    Assets/Petals/Rose.cc
    Assets/Petals/Salt.cc
    Assets/Petals/Sand.cc
    Assets/Petals/Square.cc
    Assets/Petals/Stick.cc
    Assets/Petals/Stinger.cc
    Assets/Petals/ThirdEye.cc
    Assets/Petals/Tricac.cc
    Assets/Petals/Tringer.cc
    Assets/Petals/Triplet.cc
    Assets/Petals/Triweb.cc
    Assets/Petals/Twin.cc
    Assets/Petals/UniqueBasic.cc
    Assets/Petals/Web.cc
    Assets/Petals/Wing.cc
    Assets/Petals/Yggdrasil.cc
    Assets/Petals/YinYang.cc
    Assets/Petals/Yucca.cc
    Assets/Petals/GoldenLeaf.cc
    Assets/Petals/LPeas.cc
    Assets/Petals/Soil.cc
    Assets/Petals/TriFaster.cc
    Assets/Petals/PoisonTricac.cc
    Assets/Petals/Powder.cc

    Render/RenderDrop.cc
    Render/RenderFlower.cc
    Render/RenderHealth.cc
    Render/RenderMob.cc
    Render/RenderName.cc
    Render/RenderPetal.cc
    Render/RenderWeb.cc
    Render/Renderer.cc
    Debug.cc
    DOM.cc
    Game.cc
    Input.cc
    Main.cc
    Network.cc
    Particle.cc
    Rendering.cc
    Setup.cc
    Simulation.cc
    Socket.cc
    StaticData.cc
    Storage.cc
    Ui/InGame/InputFreeze.cc
    Ui/InGame/Leaderboard.cc
    Ui/InGame/LevelBar.cc
    Ui/InGame/LoadoutPetal.cc
    Ui/InGame/LevelBar.cc
    Ui/InGame/AccountLevelBar.cc
    Ui/InGame/LoadoutPetal.cc
    Ui/InGame/LoadoutSlot.cc
    Ui/InGame/Map.cc
    Ui/InGame/Mobile.cc
    Ui/InGame/OverlevelTimer.cc
    Ui/InGame/Tooltip.cc
    Ui/TitleScreen/Changelog.cc
    Ui/TitleScreen/DeathScreen.cc
    Ui/TitleScreen/MainScreen.cc
    Ui/TitleScreen/MobGallery.cc
    Ui/TitleScreen/PetalGallery.cc
    Ui/TitleScreen/Settings.cc
    Ui/TitleScreen/StatScreen.cc
    Ui/TitleScreen/Leaderboard.cc
    Ui/Button.cc
    Ui/Choose.cc
    Ui/Container.cc
    Ui/DynamicText.cc
    Ui/Element.cc
    Ui/Extern.cc
    Ui/ScrollContainer.cc
    Ui/StaticIcon.cc
    Ui/StaticText.cc
    Ui/TextInput.cc
    Ui/Window.cc
    ../Helpers/Math.cc
    ../Helpers/UTF8.cc
    ../Helpers/Vector.cc
    ../Shared/Arena.cc
    ../Shared/Binary.cc
    ../Shared/Config.cc
    ../Shared/Entity.cc
    ../Shared/EntityDef.cc
    ../Shared/Map.cc
    ../Shared/Simulation.cc
    ../Shared/StaticData.cc
)

set(CMAKE_CXX_COMPILER "em++")
set(CMAKE_CXX_FLAGS "-DCLIENTSIDE=1 -std=c++20")

if (USE_CODEPOINT_LEN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CODEPOINT_LEN=1")
endif()
if(DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG=1 -gdwarf-4 -sNO_DISABLE_EXCEPTION_CATCHING")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math --closure=1")
endif()

# Export runtime helpers used by EM_JS/EM_ASM code
add_link_options(-sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToUTF8,lengthBytesUTF8,stringToNewUTF8)
# Export functions used from JS glue
add_link_options(-sEXPORTED_FUNCTIONS=_main,_key_event,_mouse_event,_touch_event,_wheel_event,_clipboard_event,_loop,_on_message,_malloc,_free)


add_executable(gardn-client ${SRCS})
set(CMAKE_EXECUTABLE_SUFFIX ".js")
