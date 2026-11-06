#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QWidget>
//Define the ExitCallback function used to confirm an Exit request.
void confirm_exit(QWidget* parent){
    //A function-pointer callback that provides the exit confirmation; separate from
    //QT signal or slot mechanisms. 
    
    QMessageBox::StandardButton choice_msg = 
    QMessageBox::question(parent, "Exit AI Workspace", "Are you sure you want to exit works?"
        , QMessageBox::Yes | QMessageBox::No, QMessageBox::No); 
        //Yes quits the app, but no preserves the application state
    if(choice_msg  == QMessageBox::Yes){
        QCoreApplication::quit();
    }
}
int main(int argc, char* argv[]) {
    //Creating QT app
    QApplication app(argc, argv);
    aiws::gui::MainWindow m_window;
    //register exit callback
    m_window.set_exit_callback(&confirm_exit); //Main Window invokes this function when Exit button is clicked. 
    //Displaying app window
    m_window.show();
    return app.exec();
    
}
