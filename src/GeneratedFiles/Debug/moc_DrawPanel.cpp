/****************************************************************************
** Meta object code from reading C++ file 'DrawPanel.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.6.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../view/DrawPanel.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DrawPanel.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.6.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_DrawPanel_t {
    QByteArrayData data[11];
    char stringdata0[119];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_DrawPanel_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_DrawPanel_t qt_meta_stringdata_DrawPanel = {
    {
QT_MOC_LITERAL(0, 0, 9), // "DrawPanel"
QT_MOC_LITERAL(1, 10, 8), // "sigStart"
QT_MOC_LITERAL(2, 19, 0), // ""
QT_MOC_LITERAL(3, 20, 7), // "sigUndo"
QT_MOC_LITERAL(4, 28, 10), // "sigSticker"
QT_MOC_LITERAL(5, 39, 7), // "sigSave"
QT_MOC_LITERAL(6, 47, 11), // "sigFinished"
QT_MOC_LITERAL(7, 59, 18), // "onReferRectChanged"
QT_MOC_LITERAL(8, 78, 4), // "rect"
QT_MOC_LITERAL(9, 83, 17), // "onShapeBtnClicked"
QT_MOC_LITERAL(10, 101, 17) // "onColorBtnClicked"

    },
    "DrawPanel\0sigStart\0\0sigUndo\0sigSticker\0"
    "sigSave\0sigFinished\0onReferRectChanged\0"
    "rect\0onShapeBtnClicked\0onColorBtnClicked"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_DrawPanel[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   54,    2, 0x06 /* Public */,
       3,    0,   55,    2, 0x06 /* Public */,
       4,    0,   56,    2, 0x06 /* Public */,
       5,    0,   57,    2, 0x06 /* Public */,
       6,    0,   58,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       7,    1,   59,    2, 0x0a /* Public */,
       9,    0,   62,    2, 0x0a /* Public */,
      10,    0,   63,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::QRect,    8,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void DrawPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        DrawPanel *_t = static_cast<DrawPanel *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->sigStart(); break;
        case 1: _t->sigUndo(); break;
        case 2: _t->sigSticker(); break;
        case 3: _t->sigSave(); break;
        case 4: _t->sigFinished(); break;
        case 5: _t->onReferRectChanged((*reinterpret_cast< const QRect(*)>(_a[1]))); break;
        case 6: _t->onShapeBtnClicked(); break;
        case 7: _t->onColorBtnClicked(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (DrawPanel::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&DrawPanel::sigStart)) {
                *result = 0;
                return;
            }
        }
        {
            typedef void (DrawPanel::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&DrawPanel::sigUndo)) {
                *result = 1;
                return;
            }
        }
        {
            typedef void (DrawPanel::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&DrawPanel::sigSticker)) {
                *result = 2;
                return;
            }
        }
        {
            typedef void (DrawPanel::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&DrawPanel::sigSave)) {
                *result = 3;
                return;
            }
        }
        {
            typedef void (DrawPanel::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&DrawPanel::sigFinished)) {
                *result = 4;
                return;
            }
        }
    }
}

const QMetaObject DrawPanel::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DrawPanel.data,
      qt_meta_data_DrawPanel,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *DrawPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DrawPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_DrawPanel.stringdata0))
        return static_cast<void*>(const_cast< DrawPanel*>(this));
    return QWidget::qt_metacast(_clname);
}

int DrawPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void DrawPanel::sigStart()
{
    QMetaObject::activate(this, &staticMetaObject, 0, Q_NULLPTR);
}

// SIGNAL 1
void DrawPanel::sigUndo()
{
    QMetaObject::activate(this, &staticMetaObject, 1, Q_NULLPTR);
}

// SIGNAL 2
void DrawPanel::sigSticker()
{
    QMetaObject::activate(this, &staticMetaObject, 2, Q_NULLPTR);
}

// SIGNAL 3
void DrawPanel::sigSave()
{
    QMetaObject::activate(this, &staticMetaObject, 3, Q_NULLPTR);
}

// SIGNAL 4
void DrawPanel::sigFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 4, Q_NULLPTR);
}
QT_END_MOC_NAMESPACE
