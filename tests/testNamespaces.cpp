#include "testCommon.h"

static const char* input = R"delim(
#include "fake_qobject.h"

namespace NS1 {
class Foo1  : public QObject {
    Q_OBJECT
public:
    void someSlot() {}
};
}

namespace NS2 {
class Foo2 : public QObject {
    Q_OBJECT
public:
    void someSlot() {}
};
class Bar2 : public QObject {
    Q_OBJECT
public:
    void someSlot() {}
};
}

namespace NS3 {
class Foo3 : public QObject {
    Q_OBJECT
public:
    void someSlot() {}
};
}

class Test : public QObject {
    Q_OBJECT
public:
    void globalUsingDirectives();
    void localUsingDirectives();
private:
    NS1::Foo1* foo1;
    NS2::Foo2* foo2;
    NS2::Bar2* bar2;
    NS3::Foo3* foo3;
};

void Test::localUsingDirectives() {
    connect(this, SIGNAL(objectNameChanged(QString)), foo1, SLOT(someSlot()));
    connect(this, SIGNAL(objectNameChanged(QString)), foo2, SLOT(someSlot()));
    connect(this, SIGNAL(objectNameChanged(QString)), foo3, SLOT(someSlot()));
    
    using namespace NS3; //now we no longer need to prepend the namespace NS3
    connect(this, SIGNAL(objectNameChanged(QString)), foo3, SLOT(someSlot()));
    using NS2::Foo2; //Foo2 no longer needs the namespace
    connect(this, SIGNAL(objectNameChanged(QString)), foo2, SLOT(someSlot()));
    //however NS2::Bar2 does
    connect(this, SIGNAL(objectNameChanged(QString)), bar2, SLOT(someSlot()));
}

using namespace NS3;
using NS2::Foo2;

void Test::globalUsingDirectives() {
    //just the same as the local test, however with using directives in the global scope
    connect(this, SIGNAL(objectNameChanged(QString)), foo1, SLOT(someSlot()));
    connect(this, SIGNAL(objectNameChanged(QString)), foo2, SLOT(someSlot()));
    connect(this, SIGNAL(objectNameChanged(QString)), bar2, SLOT(someSlot()));
    connect(this, SIGNAL(objectNameChanged(QString)), foo3, SLOT(someSlot()));
}
)delim";

static const char* output = R"delim(
#include "fake_qobject.h"

namespace NS1 {
class Foo1  : public QObject {
    Q_OBJECT
public:
    void someSlot() {}
};
}

namespace NS2 {
class Foo2 : public QObject {
    Q_OBJECT
public:
    void someSlot() {}
};
class Bar2 : public QObject {
    Q_OBJECT
public:
    void someSlot() {}
};
}

namespace NS3 {
class Foo3 : public QObject {
    Q_OBJECT
public:
    void someSlot() {}
};
}

class Test : public QObject {
    Q_OBJECT
public:
    void globalUsingDirectives();
    void localUsingDirectives();
private:
    NS1::Foo1* foo1;
    NS2::Foo2* foo2;
    NS2::Bar2* bar2;
    NS3::Foo3* foo3;
};

void Test::localUsingDirectives() {
    connect(this, &Test::objectNameChanged, foo1, &NS1::Foo1::someSlot);
    connect(this, &Test::objectNameChanged, foo2, &NS2::Foo2::someSlot);
    connect(this, &Test::objectNameChanged, foo3, &NS3::Foo3::someSlot);
    
    using namespace NS3; //now we no longer need to prepend the namespace NS3
    connect(this, &Test::objectNameChanged, foo3, &Foo3::someSlot);
    using NS2::Foo2; //Foo2 no longer needs the namespace
    connect(this, &Test::objectNameChanged, foo2, &Foo2::someSlot);
    //however NS2::Bar2 does
    connect(this, &Test::objectNameChanged, bar2, &NS2::Bar2::someSlot);
}

using namespace NS3;
using NS2::Foo2;

void Test::globalUsingDirectives() {
    //just the same as the local test, however with using directives in the global scope
    connect(this, &Test::objectNameChanged, foo1, &NS1::Foo1::someSlot);
    connect(this, &Test::objectNameChanged, foo2, &Foo2::someSlot);
    connect(this, &Test::objectNameChanged, bar2, &NS2::Bar2::someSlot);
    connect(this, &Test::objectNameChanged, foo3, &Foo3::someSlot);
}
)delim";

int main() {
    return testMain(input, output, 10, 10);
}

