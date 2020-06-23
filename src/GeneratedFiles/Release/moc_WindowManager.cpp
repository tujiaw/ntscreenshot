/****************************************************************************
** Meta object code from reading C++ file 'WindowManager.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.6.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../controller/WindowManager.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'WindowManager.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.6.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_WindowManager_t {
    QByteArrayData data[7];
    char stringdata0[94];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_WindowManager_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_WindowManager_t qt_meta_stringdata_WindowManager = {
    {
QT_MOC_LITERAL(0, 0, 13), // "WindowManager"
QT_MOC_LITERAL(1, 14, 18), // "sigSetMainShortcut"
QT_MOC_LITERAL(2, 33, 0), // ""
QT_MOC_LITERAL(3, 34, 7), // "textKey"
QT_MOC_LITERAL(4, 42, 14), // "sigClearResult"
QT_MOC_LITERAL(5, 57, 18), // "onScreenshotReopen"
QT_MOC_LITERAL(6, 76, 17) // "onScreenshotClose"

    },
    "WindowManager\0sigSetMainShortcut\0\0"
    "textKey\0sigClearResult\0onScreenshotReopen\0"
    "onScreenshotClose"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_WindowManager[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   34,    2, 0x06 /* Public */,
       4,    0,   37,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       5,    0,   38,    2, 0x08 /* Private */,
       6,    0,   39,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void WindowManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        WindowManager *_t = static_cast<WindowManager *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->sigSetMainShortcut((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->sigClearResult(); break;
        case 2: _t->onScreenshotReopen(); break;
        case 3: _t->onScreenshotClose(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (WindowManager::*_t)(const QString & );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&WindowManager::sigSetMainShortcut)) {
                *result = 0;
                return;
            }
        }
        {
            typedef void (WindowManager::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&WindowManager::sigClearResult)) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject WindowManager::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_WindowManager.data,
      qt_meta_data_WindowManager,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *WindowManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *WindowManager::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_WindowManager.stringdata0))
        return static_cast<void*>(const_cast< WindowManager*>(this));
    return QObject::qt_metacast(_clname);
}

int WindowManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void WindowManager::sigSetMainShortcut(const QString & _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void WindowManager::sigClearResult()
{
    QMetaObject::activate(this, &staticMetaObject, 1, Q_NULLPTR);
}
QT_END_MOC_NAMESPACE
