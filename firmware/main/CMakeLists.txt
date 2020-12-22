# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "ftcSoundBar.cpp" "playlist.cpp")
set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_EMBED_FILES "img/cocktail.svg" "img/play.svg" "img/next.svg" "img/previous.svg" "img/stop.svg" "img/random.svg" "img/redo.svg" "img/volumeup.svg" "img/volumedown.svg" "img/cog.svg" "header.html" "footer.html" "img/favicon.ico" "styles.css" "img/ftcsoundbarlogo.png" )

register_component()
