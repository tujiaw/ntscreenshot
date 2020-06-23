/****************************************************************************
** Meta object code from reading C++ file 'Screenshot.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.6.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../view/Screenshot.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Screenshot.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.6.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_ScreenshotWidget_t {
    QByteArrayData data[14];
    char stringdata0[219];
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
QT_MOC_LITERAL(4, 37, 15), // "cursorPosChange"
QT_MOC_LITERAL(5, 53, 11), // "doubleClick"
QT_MOC_LITERAL(6, 65, 25), // "sigChildWindowRectChanged"
QT_MOC_LITERAL(7, 91, 9), // "onTopMost"
QT_MOC_LITERAL(8, 101, 21), // "onScreenBorderPressed"
QT_MOC_LITERAL(9, 123, 22), // "onScreenBorderReleased"
QT_MOC_LITERAL(10, 146, 27), // "onSelectedScreenSizeChanged"
QT_MOC_LITERAL(11, 174, 26), // "onSelectedScreenPosChanged"
QT_MOC_LITERAL(12, 201, 6), // "onDraw"
QT_MOC_LITERAL(13, 208, 10) // "onDrawUndo"

    },
    "ScreenshotWidget\0sigReopen\0\0sigClose\0"
    "cursorPosChange\0doubleClick\0"
    "sigChildWindowRectChanged\0onTopMost\0"
    "onScreenBorderPressed\0onScreenBorderReleased\0"
    "onSelectedScreenSizeChanged\0"
    "onSelectedScreenPosChanged\0onDraw\0"
    "onDrawUndo"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ScreenshotWidget[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   74,    2, 0x06 /* Public */,
       3,    0,   75,    2, 0x06 /* Public */,
       4,    2,   76,    2, 0x06 /* Public */,
       5,    0,   81,    2, 0x06 /* Public */,
       6,    0,   82,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       7,    0,   83,    2, 0x08 /* Private */,
       8,    0,   84,    2, 0x08 /* Private */,
       9,    0,   85,    2, 0x08 /* Private */,
      10,    2,   86,    2, 0x08 /* Private */,
      11,    2,   91,    2, 0x08 /* Private */,
      12,    0,   96,    2, 0x08 /* Private */,
      13,    0,   97,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void ScreenshotWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        ScreenshotWidget *_t = static_cast<ScreenshotWidget *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->sigReopen(); break;
        case 1: _t->sigClose(); break;
        case 2: _t->cursorPosChange((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 3: _t->doubleClick(); break;
        case 4: _t->sigChildWindowRectChanged(); break;
        case 5: _t->onTopMost(); break;
        case 6: _t->onScreenBorderPressed(); break;
        case 7: _t->onScreenBorderReleased(); break;
        case 8: _t->onSelectedScreenSizeChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 9: _t->onSelectedScreenPosChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 10: _t->onDraw(); break;
        case 11: _t->onDrawUndo(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (ScreenshotWidget::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&ScreenshotWidget::sigReopen)) {
                *result = 0;
                return;
            }
        }
        {
            typedef void (ScreenshotWidget::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&ScreenshotWidget::sigClose)) {
                *result = 1;
                return;
            }
        }
        {
            typedef void (ScreenshotWidget::*_t)(int , int );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&ScreenshotWidget::cursorPosChange)) {
                *result = 2;
                return;
            }
        }
        {
            typedef void (ScreenshotWidget::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&ScreenshotWidget::doubleClick)) {
                *result = 3;
                return;
            }
        }
        {
            typedef void (ScreenshotWidget::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&ScreenshotWidget::sigChildWindowRectChanged)) {
                *result = 4;
                return;
            }
        }
    }
}

const QMetaObject ScreenshotWidget::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_ScreenshotWidget.data,
      qt_meta_data_ScreenshotWidget,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *ScreenshotWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ScreenshotWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_ScreenshotWidget.stringdata0))
        return static_cast<void*>(const_cast< ScreenshotWidget*>(this));
    return QWidget::qt_metacast(_clname);
}

int ScreenshotWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void ScreenshotWidget::sigReopen()
{
    QMetaObject::activate(this, &staticMetaObject, 0, Q_NULLPTR);
}

// SIGNAL 1
void ScreenshotWidget::sigClose()
{
    QMetaObject::activate(this, &staticMetaObject, 1, Q_NULLPTR);
}

// SIGNAL 2
void ScreenshotWidget::cursorPosChange(int _t1, int _t2)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void ScreenshotWidget::doubleClick()
{
    QMetaObject::activate(this, &staticMetaObject, 3, Q_NULLPTR);
}

// SIGNAL 4
void ScreenshotWidget::sigChildWindowRectChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, Q_NULLPTR);
}
struct qt_meta_stringdata_SelectedScreenSizeWidget_t {
    QByteArrayData data[8];
    char stringdata0[63];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SelectedScreenSizeWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SelectedScreenSizeWidget_t qt_meta_stringdata_SelectedScreenSizeWidget = {
    {
QT_MOC_LITERAL(0, 0, 24), // "SelectedScreenSizeWidget"
QT_MOC_LITERAL(1, 25, 15), // "onPostionChange"
QT_MOC_LITERAL(2, 41, 0), // ""
QT_MOC_LITERAL(3, 42, 1), // "x"
QT_MOC_LITERAL(4, 44, 1), // "y"
QT_MOC_LITERAL(5, 46, 12), // "onSizeChange"
QT_MOC_LITERAL(6, 59, 1), // "w"
QT_MOC_LITERAL(7, 61, 1) // "h"

    },
    "SelectedScreenSizeWidget\0onPostionChange\0"
    "\0x\0y\0onSizeChange\0w\0h"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SelectedScreenSizeWidget[] = {

 // content:
       7,       // revision
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
        SelectedScreenSizeWidget *_t = static_cast<SelectedScreenSizeWidget *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->onPostionChange((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 1: _t->onSizeChange((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        default: ;
        }
    }
}

const QMetaObject SelectedScreenSizeWidget::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_SelectedScreenSizeWidget.data,
      qt_meta_data_SelectedScreenSizeWidget,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *SelectedScreenSizeWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SelectedScreenSizeWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_SelectedScreenSizeWidget.stringdata0))
        return static_cast<void*>(const_cast< SelectedScreenSizeWidget*>(this));
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
    QByteArrayData data[16];
    char stringdata0[186];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SelectedScreenWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SelectedScreenWidget_t qt_meta_stringdata_SelectedScreenWidget = {
    {
QT_MOC_LITERAL(0, 0, 20), // "SelectedScreenWidget"
QT_MOC_LITERAL(1, 21, 10), // "sizeChange"
QT_MOC_LITERAL(2, 32, 0), // ""
QT_MOC_LITERAL(3, 33, 13), // "postionChange"
QT_MOC_LITERAL(4, 47, 11), // "doubleClick"
QT_MOC_LITERAL(5, 59, 16), // "sigBorderPressed"
QT_MOC_LITERAL(6, 76, 17), // "sigBorderReleased"
QT_MOC_LITERAL(7, 94, 8), // "sigClose"
QT_MOC_LITERAL(8, 103, 13), // "onMouseChange"
QT_MOC_LITERAL(9, 117, 1), // "x"
QT_MOC_LITERAL(10, 119, 1), // "y"
QT_MOC_LITERAL(11, 121, 9), // "onSticker"
QT_MOC_LITERAL(12, 131, 8), // "onUpload"
QT_MOC_LITERAL(13, 140, 12), // "onSaveScreen"
QT_MOC_LITERAL(14, 153, 17), // "onSaveScreenOther"
QT_MOC_LITERAL(15, 171, 14) // "quitScreenshot"

    },
    "SelectedScreenWidget\0sizeChange\0\0"
    "postionChange\0doubleClick\0sigBorderPressed\0"
    "sigBorderReleased\0sigClose\0onMouseChange\0"
    "x\0y\0onSticker\0onUpload\0onSaveScreen\0"
    "onSaveScreenOther\0quitScreenshot"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SelectedScreenWidget[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       6,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    2,   74,    2, 0x06 /* Public */,
       3,    2,   79,    2, 0x06 /* Public */,
       4,    0,   84,    2, 0x06 /* Public */,
       5,    0,   85,    2, 0x06 /* Public */,
       6,    0,   86,    2, 0x06 /* Public */,
       7,    0,   87,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       8,    2,   88,    2, 0x0a /* Public */,
      11,    0,   93,    2, 0x0a /* Public */,
      12,    0,   94,    2, 0x0a /* Public */,
      13,    0,   95,    2, 0x0a /* Public */,
      14,    0,   96,    2, 0x0a /* Public */,
      15,    0,   97,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    9,   10,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void SelectedScreenWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        SelectedScreenWidget *_t = static_cast<SelectedScreenWidget *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->sizeChange((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 1: _t->postionChange((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 2: _t->doubleClick(); break;
        case 3: _t->sigBorderPressed(); break;
        case 4: _t->sigBorderReleased(); break;
        case 5: _t->sigClose(); break;
        case 6: _t->onMouseChange((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 7: _t->onSticker(); break;
        case 8: _t->onUpload(); break;
        case 9: _t->onSaveScreen(); break;
        case 10: _t->onSaveScreenOther(); break;
        case 11: _t->quitScreenshot(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (SelectedScreenWidget::*_t)(int , int );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&SelectedScreenWidget::sizeChange)) {
                *result = 0;
                return;
            }
        }
        {
            typedef void (SelectedScreenWidget::*_t)(int , int );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&SelectedScreenWidget::postionChange)) {
                *result = 1;
                return;
            }
        }
        {
            typedef void (SelectedScreenWidget::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&SelectedScreenWidget::doubleClick)) {
                *result = 2;
                return;
            }
        }
        {
            typedef void (SelectedScreenWidget::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&SelectedScreenWidget::sigBorderPressed)) {
                *result = 3;
                return;
            }
        }
        {
            typedef void (SelectedScreenWidget::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&SelectedScreenWidget::sigBorderReleased)) {
                *result = 4;
                return;
            }
        }
        {
            typedef void (SelectedScreenWidget::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&SelectedScreenWidget::sigClose)) {
                *result = 5;
                return;
            }
        }
    }
}

const QMetaObject SelectedScreenWidget::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_SelectedScreenWidget.data,
      qt_meta_data_SelectedScreenWidget,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *SelectedScreenWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SelectedScreenWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_SelectedScreenWidget.stringdata0))
        return static_cast<void*>(const_cast< SelectedScreenWidget*>(this));
    return QWidget::qt_metacast(_clname);
}

int SelectedScreenWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void SelectedScreenWidget::sizeChange(int _t1, int _t2)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void SelectedScreenWidget::postionChange(int _t1, int _t2)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void SelectedScreenWidget::doubleClick()
{
    QMetaObject::activate(this, &staticMetaObject, 2, Q_NULLPTR);
}

// SIGNAL 3
void SelectedScreenWidget::sigBorderPressed()
{
    QMetaObject::activate(this, &staticMetaObject, 3, Q_NULLPTR);
}

// SIGNAL 4
void SelectedScreenWidget::sigBorderReleased()
{
    QMetaObject::activate(this, &staticMetaObject, 4, Q_NULLPTR);
}

// SIGNAL 5
void SelectedScreenWidget::sigClose()
{
    QMetaObject::activate(this, &staticMetaObject, 5, Q_NULLPTR);
}
QT_END_MOC_NAMESPACE
