#include <QtCore/qatomic.h>

QT_BEGIN_NAMESPACE

Q_CORE_EXPORT
bool QBasicAtomicInt_testAndSetOrdered(volatile int *_q_value, int expectedValue, int newValue)
{
    bool returnValue = false;
    if (*_q_value == expectedValue) {
        *_q_value = newValue;
        returnValue = true;
    }
    return returnValue;
}

Q_CORE_EXPORT
int QBasicAtomicInt_fetchAndStoreOrdered(volatile int *_q_value, int newValue)
{
    int returnValue;
    returnValue = *_q_value;
    *_q_value = newValue;
    return returnValue;
}

Q_CORE_EXPORT
int QBasicAtomicInt_fetchAndAddOrdered(volatile int *_q_value, int valueToAdd)
{
    int returnValue;
    returnValue = *_q_value;
    *_q_value += valueToAdd;
    return returnValue;
}

Q_CORE_EXPORT
bool QBasicAtomicPointer_testAndSetOrdered(void * volatile *_q_value,
                                           void *expectedValue,
                                           void *newValue)
{
    bool returnValue = false;
    if (*_q_value == expectedValue) {
        *_q_value = newValue;
        returnValue = true;
    }
    return returnValue;
}

Q_CORE_EXPORT
void *QBasicAtomicPointer_fetchAndStoreOrdered(void * volatile *_q_value, void *newValue)
{
    void *returnValue;
    returnValue = *_q_value;
    *_q_value = newValue;
    return returnValue;
}

Q_CORE_EXPORT
void *QBasicAtomicPointer_fetchAndAddOrdered(void * volatile *_q_value, qptrdiff valueToAdd)
{
    void *returnValue;
    returnValue = *_q_value;
    *_q_value = reinterpret_cast<char *>(returnValue) + valueToAdd;
    return returnValue;
}
QT_END_NAMESPACE
