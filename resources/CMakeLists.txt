qt_add_resources(${PROJECT_NAME} style_resources
    PREFIX "/"
    FILES
        styles/light.qss
)

qt_add_resources(${PROJECT_NAME} font_resources
    PREFIX "/"
    FILES

    fonts/CodeNewRoman/CodeNewRomanNerdFontPropo-Bold.otf
    fonts/CodeNewRoman/CodeNewRomanNerdFontPropo-Italic.otf
    fonts/CodeNewRoman/CodeNewRomanNerdFontPropo-Regular.otf

    fonts/CodeNewRoman/license.txt
)

qt_add_resources(${PROJECT_NAME} icon_resources
    PREFIX "/"
    FILES
    icons/arrow-down-16x16.png
)
