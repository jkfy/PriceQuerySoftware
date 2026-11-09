cmake_minimum_required(VERSION 3.21)

project(PriceQuerySoftware VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/src/forms)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# 给 VS Code / clangd 用的编译数据库
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Sql)

qt_add_executable(PriceQuerySoftware WIN32
    src/main.cpp
    src/theme.h
    src/mainwindow.h          src/mainwindow.cpp
    src/model/quotanode.h
    src/model/bomtreemodel.h  src/model/bomtreemodel.cpp
    src/model/badgedelegate.h src/model/badgedelegate.cpp
    src/data/dataservice.h    src/data/dataservice.cpp
    src/pages/quotationpage.h src/pages/quotationpage.cpp
    src/pages/materialpage.h  src/pages/materialpage.cpp
    src/pages/otherpages.h    src/pages/otherpages.cpp
    src/importwizard.h        src/importwizard.cpp
    src/forms/mainwindow.ui
    src/forms/quotationpage.ui
    src/forms/materialpage.ui
    src/forms/pricepage.ui
    src/forms/importpage.ui
    src/forms/settingspage.ui
    src/forms/wizardfilepage.ui
    src/forms/wizardmappingpage.ui
    src/forms/wizardpreviewpage.ui
    src/forms/wizarddonepage.ui
    resource/resources.qrc
)

target_include_directories(PriceQuerySoftware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(PriceQuerySoftware PRIVATE Qt6::Widgets Qt6::Sql)

if (WIN32)
    # 调试时希望有控制台输出，就关掉 WIN32 子系统
    set_target_properties(PriceQuerySoftware PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
