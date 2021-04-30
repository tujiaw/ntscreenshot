/****************************************************************************
** Meta object code from reading C++ file 'WindowManager.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.12.9)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../controller/WindowManager.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'WindowManager.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.12.9. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_WindowManager_t {
    QByteArrayData data[10];
    char stringdata0[130];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_WindowManager_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_WindowManager_t qt_meta_stringdata_WindowManager = {
    {
QT_MOC_LITERAL(0, 0, 13), // "WindowManager"
QT_MOC_LITERAL(1, 14, 6), // "sigPin"
QT_MOC_LITERAL(2, 21, 0), // ""
QT_MOC_LITERAL(3, 22, 17), // "sigSettingChanged"
QT_MOC_LITERAL(4, 40, 22), // "sigStickerCountChanged"
QT_MOC_LITERAL(5, 63, 18), // "onScreenshotReopen"
QT_MOC_LITERAL(6, 82, 17), // "onScreenshotClose"
QT_MOC_LITERAL(7, 100, 16), // "onSaveScreenshot"
QT_MOC_LITERAL(8, 117, 6), // "pixmap"
QT_MOC_LITERAL(9, 124, 5) // "onPin"

    },
    "WindowManager\0sigPin\0\0sigSettingChanged\0"
    "sigStickerCountChanged\0onScreenshotReopen\0"
    "onScreenshotClose\0onSaveScreenshot\0"
    "pixmap\0onPin"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_WindowManager[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   49,    2, 0x06 /* Public */,
       3,    0,   50,    2, 0x06 /* Public */,
       4,    0,   51,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       5,    0,   52,    2, 0x08 /* Private */,
       6,    0,   53,    2, 0x08 /* Private */,
       7,    1,   54,    2, 0x08 /* Private */,
       9,    0,   57,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QPixmap,    8,
    QMetaType::Void,

       0        // eod
};

void WindowManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<WindowManager *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->sigPin(); break;
        case 1: _t->sigSettingChanged(); break;
        case 2: _t->sigStickerCountChanged(); break;
        case 3: _t->onScreenshotReopen(); break;
        case 4: _t->onScreenshotClose(); break;
        case 5: _t->onSaveScreenshot((*reinterpret_cast< const QPixmap(*)>(_a[1]))); break;
        case 6: _t->onPin(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (WindowManager::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&WindowManager::sigPin)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (WindowManager::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&WindowManager::sigSettingChanged)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (WindowManager::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&WindowManager::sigStickerCountChanged)) {
                *result = 2;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject WindowManager::staticMetaObject = { {
    &QObject::staticMetaObject,
    qt_meta_stringdata_WindowManager.data,
    qt_meta_data_WindowManager,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *WindowManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *WindowManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_WindowManager.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int WindowManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void WindowManager::sigPin()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void WindowManager::sigSettingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void WindowManager::sigStickerCountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
