#include "aiws/gui/main_window.hpp"
#include "aiws/ipc_server.hpp"
#include <QMetaObject>
#include <iostream>
#include <utility>
#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <cstdlib>
#include <string>
#include <unistd.h>
//final submission comment
//Determines the default unix socket location
std::string def_skt_path(){ //Used to determine the default unix socket location
    const char* user = std::getenv("USER");
    if(user != nullptr && user[0] != '\0'){
        return "/tmp/aiws-" + std::string(user) + ".sock";
    }
    return "/tmp/aiws-" + std::to_string(getuid()) + ".sock";
}
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
    QCommandLineOption d_opt("build-delay-ms", "Extra build delay per document. (milliseconds)", "milliseconds", "0");
    parser.addOption(d_opt);
    QCommandLineOption ipc_opt("ipc-socket", "Path for the IPC server socket.", "path");
    parser.addOption(ipc_opt);
    parser.process(app);
    //Converting argument into int
    bool valid = false;
    int delay = parser.value(d_opt).toInt(&valid);
    //rejecting any invalid delay values
    if(!valid || delay < 0){
        qCritical("Invalid build delay.");
        return 1;
    }
    //MainWindow construction
    aiws::gui::MainWindow m_window;
    //Determining IPC socket path
    std::string ipc_path = parser.isSet(ipc_opt) ? parser.value(ipc_opt).toStdString() : def_skt_path();
    //creation of ipc server and connection to main window
    aiws::IPCServer server(ipc_path, [&m_window](std::string path){
        //IpC listener runs on a separate thread, Queue document additions to MainWindow so changes to Workspace
        // and widget updates will happen on the GUI thread
        QMetaObject::invokeMethod(&m_window, [&m_window, path = std::move(path)](){
            m_window.add_document_file(QString::fromUtf8(path.data(), static_cast<int>(path.size()) ));
        }, Qt::QueuedConnection); });
        std::string server_error;
        if(!server.start(server_error)){
            std::cerr << "IPC server error: " << server_error << '\n';
            return 1;
        }
    
    m_window.set_build_delay_ms(delay); //configuring background build delay
    m_window.set_exit_callback(&confirm_exit); //registering existing Exit callback
    m_window.show(); //Display the app
    int res = app.exec();
    server.stop(); // stopping the socket listener + remove socket file
    return res;
    
}
