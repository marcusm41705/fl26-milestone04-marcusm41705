// Your M4 tests (specification, Section 6).
#include "aiws/gui/main_window.hpp"
#include <QtTest/QtTest>
#include <QFile>
#include <QLabel>
#include <QTemporaryDir>
#include <QMetaObject>

using aiws::gui::MainWindow;

class StudentTests: public QObject{
    Q_OBJECT
    private:
    static bool create_test_document(const QString& path){
        QFile file(path);
        if(!file.open(QIODevice::WriteOnly)){
            return false;
        }
        return file.write("This is a test document that contains" "multiple searchable words and paragraphs. \n") > 0;
    }
    static bool has_label_text(const MainWindow& window, const QString& expected){
        for(QLabel* label:window.findChildren<QLabel*>()){
            if(label->text().contains(expected)){
                return true;
            }
        }
        return false;
    }
    private slots:
    void state_corpus_after_doc_add(){ //Testing R2: rejecting build after change in documents
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString first = directory.filePath("first.txt");
        QString second = directory.filePath("second.txt");
        QVERIFY(create_test_document(first));
        QVERIFY(create_test_document(second));
        MainWindow window;
        window.set_build_delay_ms(1000); //Delay allows changes
        QVERIFY(window.add_document_file(first));
        QVERIFY(QMetaObject::invokeMethod(&window, "build_corpus", Qt::DirectConnection));
        QVERIFY(has_label_text(window, "Building corpus in background"));
        QVERIFY(window.add_document_file(second)); //added to change the Workspace during the build
        QTRY_VERIFY_WITH_TIMEOUT(has_label_text(window, "Outdated corpus discarded"), 5000);
        QVERIFY(!has_label_text(window, "Corpus built successfully."));

    }
    void stale_corpus_after_reset(){
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString first = directory.filePath("first.txt");
        QVERIFY(create_test_document(first));
        MainWindow window;
        window.set_build_delay_ms(500);
        QVERIFY(window.add_document_file(first));
      
        QVERIFY(QMetaObject::invokeMethod(&window, "build_corpus", Qt::DirectConnection));
        QVERIFY(QMetaObject::invokeMethod(&window, "reset_application", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(has_label_text(window, "Outdated corpus discarded"), 5000);
        QVERIFY (!has_label_text(window, "Corpus built successfully."));
    
    
    
    }
    void build_failure_is_reported(){
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString first = directory.filePath("first.txt");
        QVERIFY(create_test_document(first));
        MainWindow m_window;
        m_window.set_build_delay_ms(100);
        QVERIFY(m_window.add_document_file(first));
        m_window.set_fail_nxt_build_for_testing(true);
        QVERIFY(QMetaObject::invokeMethod(&m_window, "build_corpus", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(has_label_text(m_window, "Corpus: Failed"), 3000);
        QVERIFY(QMetaObject::invokeMethod(&m_window, "build_corpus", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(has_label_text(m_window, "Corpus built successfully."), 3000);
    }
};
QTEST_MAIN(StudentTests)
#include "student_tests.moc"
