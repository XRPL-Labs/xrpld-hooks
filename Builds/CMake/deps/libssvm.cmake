find_package (libssvm_src QUIET)
if (NOT TARGET libssvm_src)
    FetchContent_Declare(
        libssvm_src
        GIT_REPOSITORY git@github.com:RichardAH/libssvm.git
        GIT_TAG        382bd11497439d334b37c585f38cc003d7aef080
        )
    FetchContent_MakeAvailable(libssvm_src)
    #    FetchContent_GetProperties(libssvm_src)
    #if(NOT libssvm_src_POPULATED)
    #    FetchContent_Populate(libssvm_src)
    #    add_subdirectory(${libssvm_src_SOURCE_DIR} ${libssvm_src_BINARY_DIR})
    #endif()
    #Message("libssvm: srcdir:")
    #Message(${libssvm_src_SOURCE_DIR})
    target_include_directories (libssvm SYSTEM INTERFACE ${libssvm_src_SOURCE_DIR}/include)
endif()
