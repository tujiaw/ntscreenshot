/****************************************************************************
** Meta object code from reading C++ file 'Stickers.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.12.9)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../view/Stickers.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Stickers.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.12.9. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_StickerWidget_t {
    QByteArrayData data[11];
    char stringdata0[91];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_StickerWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_StickerWidget_t qt_meta_stringdata_StickerWidget = {
    {
QT_MOC_LITERAL(0, 0, 13), // "StickerWidget"
QT_MOC_LITERAL(1, 14, 6), // "onDraw"
QT_MOC_LITERAL(2, 21, 0), // ""
QT_MOC_LITERAL(3, 22, 6), // "onUndo"
QT_MOC_LITERAL(4, 29, 6), // "onCopy"
QT_MOC_LITERAL(5, 36, 6), // "onSave"
QT_MOC_LITERAL(6, 43, 11), // "onUploadImg"
QT_MOC_LITERAL(7, 55, 7), // "onClose"
QT_MOC_LITERAL(8, 63, 10), // "onCloseAll"
QT_MOC_LITERAL(9, 74, 6), // "onHide"
QT_MOC_LITERAL(10, 81, 9) // "onHideAll"

    },
    "StickerWidget\0onDraw\0\0onUndo\0onCopy\0"
    "onSave\0onUploadImg\0onClose\0onCloseAll\0"
    "onHide\0onHideAll"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_StickerWidget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   59,    2, 0x08 /* Private */,
       3,    0,   60,    2, 0x08 /* Private */,
       4,    0,   61,    2, 0x08 /* Private */,
       5,    0,   62,    2, 0x08 /* Private */,
       6,    0,   63,    2, 0x08 /* Private */,
       7,    0,   64,    2, 0x08 /* Private */,
       8,    0,   65,    2, 0x08 /* Private */,
       9,    0,   66,    2, 0x08 /* Private */,
      10,    0,   67,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void StickerWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<StickerWidget *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->onDraw(); break;
        case 1: _t->onUndo(); break;
        case 2: _t->onCopy(); break;
        case 3: _t->onSave(); break;
        case 4: _t->onUploadImg(); break;
        case 5: _t->onClose(); break;
        case 6: _t->onCloseAll(); break;
        case 7: _t->onHide(); break;
        case 8: _t->onHideAll(); break;
        default: ;
        }
    }
    Q_UNUSED(_a);
}

QT_INIT_METAOBJECT const QMetaObject StickerWidget::staticMetaObject = { {
    &QWidget::staticMetaObject,
    qt_meta_stringdata_StickerWidget.data,
    qt_meta_data_StickerWidget,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *StickerWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *StickerWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_StickerWidget.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int StickerWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 9;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
