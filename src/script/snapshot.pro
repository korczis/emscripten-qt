TEMPLATE = lib
CONFIG += staticlib

CONFIG += building-libs

include($$PWD/v8.pri)

isEmpty(V8SNAPSHOT) {
   cross_compile {
       warning(Snapshot generation disabled when cross-compiling)
       V8SNAPSHOT = no
   } else {
       V8SNAPSHOT = yes
   }
}

contains(V8SNAPSHOT,yes) {
    v8_mksnapshot.commands = mksnapshot/mksnapshot ${QMAKE_FILE_OUT}
    CONFIG(debug): v8_mksnapshot.commands += --logfile $$PWD/generated/snapshot.log --log-snapshot-positions
    DUMMY_FILE = qscriptengine.cpp
    v8_mksnapshot.input = DUMMY_FILE
    v8_mksnapshot.output = $$V8_GENERATED_SOURCES_DIR/snapshot.cpp
    v8_mksnapshot.variable_out = SOURCES
    v8_mksnapshot.dependency_type = TYPE_C
    v8_mksnapshot.name = generating[v8] ${QMAKE_FILE_IN}
    silent:v8_mksnapshot.commands = @echo generating[v8] ${QMAKE_FILE_IN} && $$v8_mksnapshot.commands
    QMAKE_EXTRA_COMPILERS += v8_mksnapshot
} else {
    SOURCES += $$V8DIR/src/snapshot-empty.cc
}