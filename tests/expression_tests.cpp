#include "expressions.h"
#include <QtTest>
#include <numbers>
using namespace parameters;
class ExpressionTests:public QObject {
    Q_OBJECT
private slots:
    void arithmeticAndLocale() {
        QCOMPARE(evaluate("2+3*4").value,14.);
        QCOMPARE(evaluate("-(2+3)*4/2").value,-10.);
        QCOMPARE(evaluate("1,25 + .75").value,2.);
        QCOMPARE(evaluate("1e-3 * 1000").value,1.);
        QCOMPARE(evaluate("2 / 4 / 2").value,.25);
    }
    void dimensions() {
        auto q=evaluate("1 in + 2,54 cm");QCOMPARE(q.value,50.8);QCOMPARE(q.length,1);
        q=evaluate("2 cm * 3 mm");QCOMPARE(q.value,60.);QCOMPARE(q.length,2);
        q=evaluate("2 cm / 5 mm");QCOMPARE(q.value,4.);QCOMPARE(q.length,0);
        q=evaluate("180 deg");QVERIFY(std::abs(q.value-std::numbers::pi)<1e-12);QCOMPARE(q.angle,1);
        QVERIFY_THROWS_EXCEPTION(std::exception,evaluate("2 mm + 3 deg"));
        QVERIFY_THROWS_EXCEPTION(std::exception,evaluate("2 mm + 3"));
    }
    void referencesAndCycles() {
        auto q=resolve({{"largura","2 cm"},{"parede","largura / 10"},{"interno","largura - 2*parede"}});
        QCOMPARE(q["interno"].value,16.);QCOMPARE(q["interno"].length,1);
        QVERIFY_THROWS_EXCEPTION(std::exception,resolve({{"a","b+1"},{"b","a+1"}}));
        QVERIFY_THROWS_EXCEPTION(std::exception,resolve({{"a","a"}}));
        QVERIFY_THROWS_EXCEPTION(std::exception,resolve({{"a","ausente"}}));
        QVERIFY_THROWS_EXCEPTION(std::exception,resolve({{"mm","2"}}));
        QVERIFY_THROWS_EXCEPTION(std::exception,resolve({{"2x","2"}}));
    }
    void rejectsUnsafeAndMalformed() {
        for(const auto &s:QStringList{"", "1/0", "1e309", "1e", "1..2", "2 3", "(2", "2)", "sin(2)",
                                      "system('open')", "a=1", "2;3", "1,2.3", "2 ft", "NaN"})
            QVERIFY_THROWS_EXCEPTION(std::exception,evaluate(s));
        QVERIFY_THROWS_EXCEPTION(std::exception,evaluate(QString(100,'(')+"1"+QString(100,')')));
        QVERIFY_THROWS_EXCEPTION(std::exception,evaluate(QString(4097,'1')));
        QVERIFY_THROWS_EXCEPTION(std::exception,evaluate("1e308*1e308"));
    }
    void boundedDependenciesAndNoMutation() {
        QMap<QString,QString> definitions{{"p0","1 mm"}};
        for(int i=1;i<256;++i) definitions[QString("p%1").arg(i)]=QString("p%1 + 1 mm").arg(i-1);
        auto before=definitions; QCOMPARE(resolve(definitions)["p255"].value,256.);QCOMPARE(definitions,before);
        definitions["extra"]="1";QVERIFY_THROWS_EXCEPTION(std::exception,resolve(definitions));
    }
};
QTEST_APPLESS_MAIN(ExpressionTests)
#include "expression_tests.moc"
