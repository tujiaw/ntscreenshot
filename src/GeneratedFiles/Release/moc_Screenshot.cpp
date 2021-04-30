/****************************************************************************
** Meta object code from reading C++ file 'Screenshot.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.12.9)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../view/Screenshot.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Screenshot.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.12.9. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_ScreenshotWidget_t {
    QByteArrayData data[14];
    char stringdata0[233];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ScreenshotWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ScreenshotWidget_t qt_meta_stringdata_ScreenshotWidget = {
    {
QT_MOC_LITERAL(0, 0, 16), // "ScreenshotWidget"
QT_MOC_LITERAL(1, 17, 9), // "sigReopen"
QT_MOC_LITERAL(2, 27, 0), // ""
QT_MOC_LITERAL(3, 28, 8), // "sigClose"
QT_MOC_LITERAL(4, 37, 19), // "sigCursorPosChanged"
QT_MOC_LITERAL(5, 57, 14), // "sigDoubleClick"
QT_MOC_LITERAL(6, 72, 25), // "sigChildWindowRectChanged"
QT_MOC_LITERAL(7, 98, 17), // "sigSaveScreenshot"
QT_MOC_LITERAL(8, 116, 6), // "pixmap"
QT_MOC_LITERAL(9, 123, 9), // "onTopMost"
QT_MOC_LITERAL(10, 133, 21), // "onScreenBorderPressed"
QT_MOC_LITERAL(11, 155, 22), // "onScreenBorderReleased"
QT_MOC_LITERAL(12, 178, 27), // "onSelectedScreenSizeChanged"
QT_MOC_LITERAL(13, 206, 26) // "onSelectedScreenPosChanged"

    },
    "ScreenshotWidget\0sigReopen\0\0sigClose\0"
    "sigCursorPosChanged\0sigDoubleClick\0"
    "sigChildWindowRectChanged\0sigSaveScreenshot\0"
    "pixmap\0onTopMost\0onScreenBorderPressed\0"
    "onScreenBorderReleased\0"
    "onSelectedScreenSizeChanged\0"
    "onSelectedScreenPosChanged"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ScreenshotWidget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       6,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   69,    2, 0x06 /* Public */,
       3,    0,   70,    2, 0x06 /* Public */,
       4,    2,   71,    2, 0x06 /* Public */,
       5,    0,   76,    2, 0x06 /* Public */,
       6,    0,   77,    2, 0x06 /* Public */,
       7,    1,   78,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       9,    0,   81,    2, 0x08 /* Private */,
      10,    2,   82,    2, 0x08 /* Private */,
      11,    2,   87,    2, 0x08 /* Private */,
      12,    2,   92,    2, 0x08 /* Private */,
      13,    2,   97,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QPixmap,    8,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,

       0        // eod
};

void ScreenshotWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ScreenshotWidget *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->sigReopen(); break;
        case 1: _t->sigClose(); break;
        case 2: _t->sigCursorPosChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 3: _t->sigDoubleClick(); break;
        case 4: _t->sigChildWindowRectChanged(); break;
        case 5: _t->sigSaveScreenshot((*reinterpret_cast< const QPixmap(*)>(_a[1]))); break;
        case 6: _t->onTopMost(); break;
        case 7: _t->onScreenBorderPressed((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 8: _t->onScreenBorderReleased((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 9: _t->onSelectedScreenSizeChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 10: _t->onSelectedScreenPosChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ScreenshotWidget::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ScreenshotWidget::sigReopen)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (ScreenshotWidget::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ScreenshotWidget::sigClose)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (ScreenshotWidget::*)(int , int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ScreenshotWidget::sigCursorPosChanged)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (ScreenshotWidget::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ScreenshotWidget::sigDoubleClick)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (ScreenshotWidget::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ScreenshotWidget::sigChildWindowRectChanged)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (ScreenshotWidget::*)(const QPixmap & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ScreenshotWidget::sigSaveScreenshot)) {
                *result = 5;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject ScreenshotWidget::staticMetaObject = { {
    &QWidget::staticMetaObject,
    qt_meta_stringdata_ScreenshotWidget.data,
    qt_meta_data_ScreenshotWidget,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ScreenshotWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ScreenshotWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ScreenshotWidget.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ScreenshotWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void ScreenshotWidget::sigReopen()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ScreenshotWidget::sigClose()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ScreenshotWidget::sigCursorPosChanged(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void ScreenshotWidget::sigDoubleClick()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ScreenshotWidget::sigChildWindowRectChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void ScreenshotWidget::sigSaveScreenshot(const QPixmap & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}
struct qt_meta_stringdata_SelectedScreenSizeWidget_t {
    QByteArrayData data[8];
    char stringdata0[66];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SelectedScreenSizeWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SelectedScreenSizeWidget_t qt_meta_stringdata_SelectedScreenSizeWidget = {
    {
QT_MOC_LITERAL(0, 0, 24), // "SelectedScreenSizeWidget"
QT_MOC_LITERAL(1, 25, 17), // "onPositionChanged"
QT_MOC_LITERAL(2, 43, 0), // ""
QT_MOC_LITERAL(3, 44, 1), // "x"
QT_MOC_LITERAL(4, 46, 1), // "y"
QT_MOC_LITERAL(5, 48, 13), // "onSizeChanged"
QT_MOC_LITERAL(6, 62, 1), // "w"
QT_MOC_LITERAL(7, 64, 1) // "h"

    },
    "SelectedScreenSizeWidget\0onPositionChanged\0"
    "\0x\0y\0onSizeChanged\0w\0h"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SelectedScreenSizeWidget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    2,   24,    2, 0x0a /* Public */,
       5,    2,   29,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    3,    4,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    6,    7,

       0        // eod
};

void SelectedScreenSizeWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SelectedScreenSizeWidget *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->onPositionChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 1: _t->onSizeChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject SelectedScreenSizeWidget::staticMetaObject = { {
    &QWidget::staticMetaObject,
    qt_meta_stringdata_SelectedScreenSizeWidget.data,
    qt_meta_data_SelectedScreenSizeWidget,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *SelectedScreenSizeWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SelectedScreenSizeWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_SelectedScreenSizeWidget.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int SelectedScreenSizeWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 2;
    }
    return _id;
}
struct qt_meta_stringdata_SelectedScreenWidget_t {
    QByteArrayData data[26];
    char stringdata0[297];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SelectedScreenWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SelectedScreenWidget_t qt_meta_stringdata_SelectedScreenWidget = {
    {
QT_MOC_LITERAL(0, 0, 20), // "SelectedScreenWidget"
QT_MOC_LITERAL(1, 21, 14), // "sigSizeChanged"
QT_MOC_LITERAL(2, 36, 0), // ""
QT_MOC_LITERAL(3, 37, 18), // "sigPositionChanged"
QT_MOC_LITERAL(4, 56, 14), // "sigDoubleClick"
QT_MOC_LITERAL(5, 71, 16), // "sigBorderPressed"
QT_MOC_LITERAL(6, 88, 17), // "sigBorderReleased"
QT_MOC_LITERAL(7, 106, 8), // "sigClose"
QT_MOC_LITERAL(8, 115, 17), // "sigSaveScreenshot"
QT_MOC_LITERAL(9, 133, 6), // "pixmap"
QT_MOC_LITERAL(10, 140, 19), // "onSelectRectChanged"
QT_MOC_LITERAL(11, 160, 4), // "left"
QT_MOC_LITERAL(12, 165, 3), // "top"
QT_MOC_LITERAL(13, 169, 5), // "right"
QT_MOC_LITERAL(14, 175, 6), // "bottom"
QT_MOC_LITERAL(15, 182, 18), // "onCursorPosChanged"
QT_MOC_LITERAL(16, 201, 1), // "x"
QT_MOC_LITERAL(17, 203, 1), // "y"
QT_MOC_LITERAL(18, 205, 9), // "onSticker"
QT_MOC_LITERAL(19, 215, 12), // "onSaveScreen"
QT_MOC_LITERAL(20, 228, 17), // "onSaveScreenOther"
QT_MOC_LITERAL(21, 246, 11), // "onUploadImg"
QT_MOC_LITERAL(22, 258, 10), // "onKeyEvent"
QT_MOC_LITERAL(23, 269, 10), // "QKeyEvent*"
QT_MOC_LITERAL(24, 280, 1), // "e"
QT_MOC_LITERAL(25, 282, 14) // "quitScreenshot"

    },
    "SelectedScreenWidget\0sigSizeChanged\0"
    "\0sigPositionChanged\0sigDoubleClick\0"
    "sigBorderPressed\0sigBorderReleased\0"
    "sigClose\0sigSaveScreenshot\0pixmap\0"
    "onSelectRectChanged\0left\0top\0right\0"
    "bottom\0onCursorPosChanged\0x\0y\0onSticker\0"
    "onSaveScreen\0onSaveScreenOther\0"
    "onUploadImg\0onKeyEvent\0QKeyEvent*\0e\0"
    "quitScreenshot"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SelectedScreenWidget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      15,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    2,   89,    2, 0x06 /* Public */,
       3,    2,   94,    2, 0x06 /* Public */,
       4,    0,   99,    2, 0x06 /* Public */,
       5,    2,  100,    2, 0x06 /* Public */,
       6,    2,  105,    2, 0x06 /* Public */,
       7,    0,  110,    2, 0x06 /* Public */,
       8,    1,  111,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      10,    4,  114,    2, 0x0a /* Public */,
      15,    2,  123,    2, 0x0a /* Public */,
      18,    0,  128,    2, 0x0a /* Public */,
      19,    0,  129,    2, 0x0a /* Public */,
      20,    0,  130,    2, 0x0a /* Public */,
      21,    0,  131,    2, 0x0a /* Public */,
      22,    1,  132,    2, 0x0a /* Public */,
      25,    0,  135,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QPixmap,    9,

 // slots: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::Int,   11,   12,   13,   14,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   16,   17,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 23,   24,
    QMetaType::Void,

       0        // eod
};

void SelectedScreenWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SelectedScreenWidget *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->sigSizeChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 1: _t->sigPositionChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 2: _t->sigDoubleClick(); break;
        case 3: _t->sigBorderPressed((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 4: _t->sigBorderReleased((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 5: _t->sigClose(); break;
        case 6: _t->sigSaveScreenshot((*reinterpret_cast< const QPixmap(*)>(_a[1]))); break;
        case 7: _t->onSelectRectChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3])),(*reinterpret_cast< int(*)>(_a[4]))); break;
        case 8: _t->onCursorPosChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 9: _t->onSticker(); break;
        case 10: _t->onSaveScreen(); break;
        case 11: _t->onSaveScreenOther(); break;
        case 12: _t->onUploadImg(); break;
        case 13: _t->onKeyEvent((*reinterpret_cast< QKeyEvent*(*)>(_a[1]))); break;
        case 14: _t->quitScreenshot(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (SelectedScreenWidget::*)(int , int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SelectedScreenWidget::sigSizeChanged)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (SelectedScreenWidget::*)(int , int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SelectedScreenWidget::sigPositionChanged)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (SelectedScreenWidget::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SelectedScreenWidget::sigDoubleClick)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (SelectedScreenWidget::*)(int , int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SelectedScreenWidget::sigBorderPressed)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (SelectedScreenWidget::*)(int , int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SelectedScreenWidget::sigBorderReleased)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (SelectedScreenWidget::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SelectedScreenWidget::sigClose)) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (SelectedScreenWidget::*)(const QPixmap & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SelectedScreenWidget::sigSaveScreenshot)) {
                *result = 6;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject SelectedScreenWidget::staticMetaObject = { {
    &QWidget::staticMetaObject,
    qt_meta_stringdata_SelectedScreenWidget.data,
    qt_meta_data_SelectedScreenWidget,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *SelectedScreenWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SelectedScreenWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_SelectedScreenWidget.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int SelectedScreenWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 15;
    }
    return _id;
}

// SIGNAL 0
void SelectedScreenWidget::sigSizeChanged(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void SelectedScreenWidget::sigPositionChanged(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void SelectedScreenWidget::sigDoubleClick()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void SelectedScreenWidget::sigBorderPressed(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void SelectedScreenWidget::sigBorderReleased(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void SelectedScreenWidget::sigClose()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void SelectedScreenWidget::sigSaveScreenshot(const QPixmap & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
