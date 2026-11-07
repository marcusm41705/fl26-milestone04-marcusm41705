#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
//Define the ExitCallback function used to confirm an Exit request.
void confirm_exit(QWidget* parent){
    //A function-pointer callback that provides the exit confirmation; separate from
    //QT signal or slot mechanisms. 
    
    QMessageBox::StandardButton choice_msg = 
    QMessageBox::question(parent, "Exit AI Workspace", "Are you sure you want to exit the Workspace?"
        , QMessageBox::Yes | QMessageBox::No, QMessageBox::No); 
        //Yes quits the app, but no preserves the application state
    if(choice_msg  == QMessageBox::Yes){
        parent->close();
    }
}
int main(int argc, char* argv[]) {
    //Creating QT app
    QApplication app(argc, argv);
    //Setting up command line argument parsing
    QCommandLineParser parser;
    parser.setApplicationDescription("AI Workspace M4");
    parser.addHelpOption();
    //This defines the optional corpus build delay
    QCommandLineOption d_option("build-delay-ms", "Extra build delay per document. (milliseconds)", "milliseconds", "0");
    parser.addOption(d_option);
    parser.process(app);
    //Converting argument into int
    bool valid = false;
    int delay = parser.value(d_option).toInt(&valid);
    //rejecting any invalid delay values
    if(!valid || delay < 0){
        qCritical("Invalid build delay.");
        return 1;
    }
    //MainWindow construction
    aiws::gui::MainWindow m_window;
    m_window.set_build_delay_ms(delay); //configuring background build delay
    m_window.set_exit_callback(&confirm_exit); //registering existing Exit callback
    m_window.show(); //Display the app
    return app.exec();
    
}
