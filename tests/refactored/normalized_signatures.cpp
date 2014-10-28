#include <QObject>
#include <QUrl>

class Foo : public QObject {
    Q_OBJECT
public:
    Foo() = default;
Q_SIGNALS:
    void ref(QUrl&);
    void ref(); // force disambiguation
    void constRef(const QUrl&);
    void constRef(); // force disambiguation
    void ptr(QUrl*);
    void ptr(); // force disambiguation
    void constPtr(const QUrl*);
    void constPtr(); // force disambiguation
public Q_SLOTS:
    void overloadedSlot(QUrl&) {
        printf("Called overloadedSlot(QUrl&)\n");
        callCount[0]++;
    }
    void overloadedSlot(const QUrl&) {
        printf("Called overloadedSlot(const QUrl&)\n");
        callCount[1]++;
    }
    void overloadedSlot(QUrl*) {
        printf("Called overloadedSlot(QUrl*)\n");
        callCount[2]++;
    }
    void overloadedSlot(const QUrl*) {
        printf("Called overloadedSlot(const QUrl*)\n");
        callCount[3]++;
    }
private:
    int callCount[4] = { 0, 0, 0, 0 };

public:
    void checkCallCount(int c1, int c2, int c3, int c4) {
        if (c1 != callCount[0] || c2 != callCount[1] || c3 != callCount[2] || c4 != callCount[3]) {
            fprintf(stderr, "Call count mismatch: (%d, %d, %d, %d) vs (%d, %d, %d, %d)\n",
                callCount[0], callCount[1], callCount[2], callCount[3], c1, c2, c3, c4);
            exit(1);
        }
    }
};

int main() {
    QUrl url;
    QUrl& r = url;
    const QUrl& cr = url;
    QUrl* p = &url;
    const QUrl* cp = &url;
    Foo* foo = new Foo;

    // exact matches
    QObject::connect(foo, SIGNAL(constPtr(const QUrl*)), foo, SLOT(overloadedSlot(const QUrl*)));
    QObject::connect(foo, SIGNAL(constRef(const QUrl&)), foo, SLOT(overloadedSlot(const QUrl&)));
    QObject::connect(foo, SIGNAL(ptr(QUrl*)), foo, SLOT(overloadedSlot(QUrl*)));
    QObject::connect(foo, SIGNAL(ref(QUrl&)), foo, SLOT(overloadedSlot(QUrl&)));
    // added const in slot doesn't work!
    //QObject::connect(foo, SIGNAL(ptr(QUrl*)), foo, SLOT(overloadedSlot(const QUrl*)));
    //QObject::connect(foo, SIGNAL(ref(QUrl&)), foo, SLOT(overloadedSlot(const QUrl&)));

    foo->ref(r);
    foo->checkCallCount(1, 0, 0, 0);
    foo->constRef(cr);
    foo->checkCallCount(1, 1, 0, 0);
    foo->ptr(p);
    foo->checkCallCount(1, 1, 1, 0);
    foo->constPtr(cp);
    foo->checkCallCount(1, 1, 1, 1);

    foo->disconnect();

    // without normalization QUrl would be mapped to QUrl& or QUrl* since that is only one char different!!

    /* QObject::connect: Incompatible sender/receiver arguments
     *         Foo::ref(QUrl&) --> Foo::overloadedSlot(QUrl)
     */
    // QObject::connect(foo, SIGNAL(ref(QUrl&)), foo, SLOT(overloadedSlot(QUrl)));
    QObject::connect(foo, SIGNAL(constRef(const QUrl&)), foo, SLOT(overloadedSlot(QUrl))); // normalization: QUrl is equiv to const QUrl&
    QObject::connect(foo, SIGNAL(constRef(QUrl)), foo, SLOT(overloadedSlot(const QUrl&))); // normalization: QUrl is equiv to const QUrl&
    QObject::connect(foo, SIGNAL(constRef(QUrl)), foo, SLOT(overloadedSlot(QUrl))); // normalization: QUrl is equiv to const QUrl&
    foo->constRef(cr);
    foo->checkCallCount(1, 4, 1, 1);
    return 0;
}

#include "normalized_signatures.moc"
