QT = core gui

CONFIG += c++11 console
CONFIG -= app_bundle

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    main.cpp \
    uohues.cpp \
    EasyBMP/EasyBMP.cpp \
    uoppackage/uopblock.cpp \
    uoppackage/uoperror.cpp \
    uoppackage/uopfile.cpp \
    uoppackage/uophash.cpp \
    uoppackage/uoppackage.cpp \
    uoppackage/uopcompression.cpp \
    img2dds/libsquish/alpha.cpp \
    img2dds/libsquish/clusterfit.cpp \
    img2dds/libsquish/colourblock.cpp \
    img2dds/libsquish/colourfit.cpp \
    img2dds/libsquish/colourset.cpp \
    img2dds/libsquish/maths.cpp \
    img2dds/libsquish/rangefit.cpp \
    img2dds/libsquish/singlecolourfit.cpp \
    img2dds/libsquish/squish.cpp \
    img2dds/ImageBuilder.cc

HEADERS += \
    uohues.h \
    EasyBMP/EasyBMP.h \
    EasyBMP/EasyBMP_BMP.h \
    EasyBMP/EasyBMP_DataStructures.h \
    EasyBMP/EasyBMP_VariousBMPutilities.h \
    EasyBMP/EasyBMP.h \
    uoppackage/uopblock.h \
    uoppackage/uopcompression.h \
    uoppackage/uoperror.h \
    uoppackage/uopfile.h \
    uoppackage/uophash.h \
    uoppackage/uoppackage.h \
    img2dds/libsquish/alpha.h \
    img2dds/libsquish/clusterfit.h \
    img2dds/libsquish/colourblock.h \
    img2dds/libsquish/colourfit.h \
    img2dds/libsquish/colourset.h \
    img2dds/libsquish/config.h \
    img2dds/libsquish/maths.h \
    img2dds/libsquish/rangefit.h \
    img2dds/libsquish/simd.h \
    img2dds/libsquish/simd_float.h \
    img2dds/libsquish/simd_sse.h \
    #img2dds/libsquish/simd_ve.h \
    img2dds/libsquish/singlecolourfit.h \
    img2dds/libsquish/squish.h \
    img2dds/ImageBuilder.hh


###### Compiler/Linker settings

# windows
win32:!unix {
    contains(QMAKE_CC, gcc) {
        # MinGW

        #   Dynamically link zlib and FreeImage.dll
        contains(QT_ARCH, x86_64) {
            LIBS += -L\"$$PWD\\..\winlibs\\64\" -lz -lFreeImage
        } else {
            LIBS += -L\"$$PWD\\..\winlibs\\32\" -lz -lFreeImage
        }
    }

    contains(QMAKE_CC, cl) {
        # Visual Studio

        #   Dynamically link zlib and FreeImage.dll
        contains(QT_ARCH, x86_64) {
            LIBS += -L\"$$PWD\\..\winlibs\\64\" -lzlib -lFreeImage
        } else {
            LIBS += -L\"$$PWD\\..\winlibs\\32\" -lzlib -lFreeImage
        }
        DEFINES += "ZLIB_WINAPI=1"
        DEFINES += "ZLIB_DLL=1"
    }

    contains(QMAKE_COPY, copy) {    # When it's compiled by AppVeyor, QMAKE_COPY is cp -f, not copy
                                    # this way, we'll copy dlls to build directory only when building locally on Windows
        contains(QT_ARCH, x86_64) {
            QMAKE_PRE_LINK += $$QMAKE_COPY \"$$PWD\\..\winlibs\\64\\*.dll\" \"$$DESTDIR\"
        } else {
            QMAKE_PRE_LINK += $$QMAKE_COPY \"$$PWD\\..\winlibs\\32\\*.dll\" \"$$DESTDIR\"
        }
    }
}

# linux
unix {
    LIBS += -lfreeimage -lz         # dynamically link zlib
}
