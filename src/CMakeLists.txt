
AUX_SOURCE_DIRECTORY(. PIKIWIDB_SRC)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/pstd)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/net)

ADD_EXECUTABLE(pikiwidb ${PIKIWIDB_SRC})
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

TARGET_INCLUDE_DIRECTORIES(pikiwidb PRIVATE ${rocksdb_SOURCE_DIR}/include)
TARGET_LINK_LIBRARIES(pikiwidb net; dl; leveldb; fmt; pikiwidb-folly; rocksdb)
SET_TARGET_PROPERTIES(pikiwidb PROPERTIES LINKER_LANGUAGE CXX)
