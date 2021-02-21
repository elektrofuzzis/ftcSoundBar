# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "main.cpp" "ftcSoundBar.cpp" "playlist.cpp" "pipeline.cpp" "blink.cpp" "ota.cpp")
set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_EMBED_FILES "img/cocktail.svg" "img/play.svg" "img/next.svg" "img/previous.svg" "img/stop.svg" "img/shuffle.svg" "img/repeat.svg" "img/volumeup.svg" "img/volumedown.svg" "img/setup.svg" "header.html" "img/favicon.ico" "styles.css" "img/ftcsoundbarlogo.svg" )

register_component()
