include(AddBoostTests)
add_definitions(-UQT_NO_STL)

# Render configuration file (used by unit tests)
configure_file(config.h.in config.h @ONLY)

#
# Unit tests executable
#
add_boost_tests(
    TARGET unit_tests
    MODULE BasicTests
    DYN_LINK
    SOURCES
        sanitize_snippet_tester.cpp
        utils_tester.cpp
        header_files_cache_tester.cpp
        translation_unit_tester.cpp
        database_manager_tester.cpp
        serialize_tester.cpp
        index_utils_tester.cpp
        unsaved_files_list_tester.cpp
  )

target_link_libraries(
    unit_tests
    # NOTE Due a single pass GNU linker `sharedcode4testsmoc` target (library)
    # must be specified twice: before and after `sharedcode4tests`
    sharedcode4tests
    sharedcode4testsmoc
    sharedcode4tests
    sharedcode4testsmoc
    Boost::filesystem
    Boost::serialization
    Boost::system
    Boost::unit_test_framework
    libclang
    ${KDE4_KTEXTEDITOR_LIBS}
    ${KDE4_KFILE_LIBS}
    ${XAPIAN_LIBRARIES}
  )

#
# QTest based indexer unit-tests
#
qt4_wrap_cpp(INDEXER_TESTER_HEADERS_MOC indexer_tester.h)
set(
    INDEXER_UNIT_TEST_SOURCES
    indexer_tester.cpp
    ${INDEXER_TESTER_HEADERS_MOC}
  )

add_executable(
    indexer_unit_tests
    ${INDEXER_UNIT_TEST_SOURCES}
  )

target_link_libraries(
    indexer_unit_tests
    sharedcode4tests
    sharedcode4testsmoc
    Boost::filesystem
    Boost::serialization
    Boost::system
    ${KDE4_KTEXTEDITOR_LIBS}
    ${KDE4_KDEUI_LIBRARY}
    libclang
    ${QT_QTTEST_LIBRARY_RELEASE}
    ${XAPIAN_LIBRARIES}
  )

#
# Sample indexer to play w/ it
#
add_executable(
    indexing_test
    indexing_test.cpp
  )

target_link_libraries(
    indexing_test
    sharedcode4tests
    ${KDE4_KDECORE_LIBRARY}
    libclang
  )

configure_file(data/test_manifest.in data/fake.db/manifest)
