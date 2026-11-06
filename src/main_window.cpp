#include "aiws/gui/main_window.hpp"
#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/workspace_widget.hpp"
#include "aiws/gui/term_inspector_widget.hpp"
#include "aiws/gui/context_widget.hpp"
#include "aiws/document.hpp"
#include "aiws/gui/resettable.hpp"
#include <QLabel>
#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include<QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <exception>
#include <utility>
#include <QCoreApplication>
#include <QMessageBox>
#include <cstddef>
#include <future>
#include <QTimer>
#include <chrono>
#include <future>
#include <thread>
#include <utility>

namespace aiws::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    //Serves as the central coordiation for the entire application, 
    //it owns Workspace and ProcessingCore backend objects while connecting
    //Gui components using the QT signals and slots. 
    setWindowTitle("AI Workspace");
    resize(900,900); //Setting up application window
    //Setting up widget and main Layout
    QWidget* central = new QWidget(this);
    QVBoxLayout* main_lay = new QVBoxLayout(central);
    //Setting up application title and header
    QLabel* title_label = new QLabel("AI Workspace", central);
    title_label->setAlignment(Qt::AlignCenter);
   title_label->setStyleSheet("background-color: #80002B;" "color: white;" "font-size: 20px;" "font-weight: bold;"
    "padding: 12px;");
   main_lay->addWidget(title_label);
   //Workspace component
   workspace_widget_ = new WorkspaceWidget(central);
   main_lay->addWidget(workspace_widget_, 1);
    //Search components
   search_widget_ = new SearchWidget(central);
   main_lay->addWidget(search_widget_,2);

   QHBoxLayout* lower_lay = new QHBoxLayout(); //lower portion of gui
   term_inspector_widget_ = new TermInspectorWidget(central); //constructing the term inspector
   context_widget_ = new ContextWidget(central);
   lower_lay->addWidget(term_inspector_widget_, 1);
   lower_lay->addWidget(context_widget_, 1);
   main_lay->addLayout(lower_lay, 1);
   
   //Setting up bottom status and the action buttons
   QHBoxLayout* bottom_lay = new QHBoxLayout();
   status_label_ = new QLabel("Ready.", central);
   reset_button_ = new QPushButton("Reset", central);
   exit_button_ = new QPushButton("Exit", central);
   reset_button_->setStyleSheet("background-color: #d6ac50;" "color: white;" "padding: 8px;");
   exit_button_->setStyleSheet("background-color: #800020;" "color: white;" "padding: 8px;");
   bottom_lay->addWidget(status_label_);
   bottom_lay->addStretch();
   bottom_lay->addWidget(reset_button_);
   bottom_lay->addWidget(exit_button_);
   main_lay->addLayout(bottom_lay);
  
   //Installing the central widget
   setCentralWidget(central);
   //Connecting the WorkspaceWidget elements
   connect(workspace_widget_, &WorkspaceWidget::add_document_requested, this, &MainWindow::add_document);
   connect(workspace_widget_, &WorkspaceWidget::build_corpus_requested,this, &MainWindow::build_corpus);
   connect(workspace_widget_, &WorkspaceWidget::document_selected, this, &MainWindow::document_selected);
   //Connecting the reset and exit elements
   connect(reset_button_, &QPushButton::clicked, this, &MainWindow::reset_application);
   connect(exit_button_, &QPushButton::clicked, this, &MainWindow::exit_application);
   //Conencting the search button request
   connect(search_widget_, &SearchWidget::search_requested, this, &MainWindow::search_requested);
   //Connecting query text changed
   connect(search_widget_, &SearchWidget::query_changed, this, &MainWindow::query_changed);
   //Connecting view source requested
   connect(search_widget_, &SearchWidget::source_document_requested, this, &MainWindow::source_document_requested);
   connect(term_inspector_widget_, &TermInspectorWidget::term_inspection_requested, this, &MainWindow::term_inspection_requested);
   connect(search_widget_, &SearchWidget::selected_chunk_changed, term_inspector_widget_, &TermInspectorWidget::set_selected_chunk);
   connect(context_widget_, &ContextWidget::build_context_requested, this, &MainWindow::build_context_requested);
   //Initializing the workspace states
   workspace_widget_->set_corpus_status("Corpus: Not built");
   update_action_states();
 //Timer that checks background build completion
 build_timer_ = new QTimer(this);
 build_timer_->setInterval(50);
 connect(build_timer_, &QTimer::timeout, this, &MainWindow::check_build_finished);
}

void MainWindow::set_exit_callback(ExitCallback callback) noexcept {
    exit_callback_ = callback;
}


void MainWindow::add_document() {
    QString file_path = QFileDialog::getOpenFileName(this, "Add Document", QString(), "Text Files (*.txt);;All Files (*)");
     if(file_path.isEmpty()){
        return;
    }
    if(!add_document_file(file_path)){
        QMessageBox::warning(this, "Document Error", "Unable to add the selected document.");
    }
}

bool MainWindow::add_document_file(const QString& path){
//Opening a file dialog used to select a text file
    
    if(path.isEmpty()){
        return false;
    }
    //Retrieve the absolute file path and filename
    QFileInfo file_info(path);
    QString document_id = file_info.absoluteFilePath();
    QString document_title = file_info.fileName();
    //Opens the file selected
    QFile file(document_id);
    if(!file.open(QIODevice::ReadOnly)){
        return false;
    }
    //Reads entire file
    QByteArray file_data = file.readAll();
    if(file.error() != QFileDevice::NoError){
      return false;
    }
    file.close();
    //Constructign the document using the class from Milestone 2
    Document document(document_id.toUtf8().toStdString(), document_title.toUtf8().toStdString(), file_data.toStdString());
    try { //Add document to backend workspace
        workspace_.add_document(document);
        ++workspace_revision_;
    } catch (const std::exception& error){
        return false;
    }
    //Add title to Gui
    workspace_widget_->add_document(document_title);
    //Workspace changed
    invalidate_corpus();
    status_label_->setText(QString("Document added: %1").arg(document_title));
    return true;
}
void MainWindow::set_build_delay_ms(int ms_per_document){
    build_delay_ms_ = (ms_per_document >= 0) ? ms_per_document : 0;
}


void MainWindow::build_corpus() {
   if(build_in_progress_){ //Don't start another build while this one is running
    return;
   }
   if(workspace_.documents().empty()){
    return;
   }
   Workspace snap = workspace_;
   const std::size_t revision = workspace_revision_;
   const int delay_ms = build_delay_ms_;
   try{
    build_future_= std::async(std::launch::async, [snap = std::move(snap), delay_ms]()mutable{
        ProcessingCore new_core;
        for(const auto& document: snap.documents()){
            (void)document;
            if(delay_ms > 0){
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
        }
        new_core.rebuild(snap);
        return new_core;
    });

   } catch(const std::exception& error){
    corpus_current_ = false;
    workspace_widget_->set_corpus_status("Corpus: Failed");
    status_label_->setText(QString("Unable to start build: %1").arg(error.what()));
    update_action_states();
    return;
   }
   build_revision_ = revision;
   build_in_progress_ = true;
   invalidate_corpus();
   workspace_widget_->set_corpus_status("Corpus: Building...");
   status_label_->setText("Building corpus in background...");
   update_action_states();
   build_timer_->start();
}
void MainWindow::check_build_finished(){
    if(!build_in_progress_){
        return;
    }
    //Checks whether the background task is completed
    auto status = build_future_.wait_for(std::chrono::milliseconds(0));
    //Don't block GUI if worker is still running
    if(status != std::future_status::ready){
        return;
    }
    build_timer_->stop(); //background operation is finished
    build_in_progress_ = false;
    try{
        //Retrieving the completed ProcessingCore
        ProcessingCore completed_core = build_future_.get();
        //Checking whether Workspace has changed during building
        if(build_revision_ != workspace_revision_){
            invalidate_corpus();
            status_label_->setText("Workspace changed, Outdated corpus discarded.");
            return;
        }
        //accepting the valid corpus by transferring ownership
        processing_core_ = std::move(completed_core);
        corpus_current_ =true;
        clear_processing_outputs();
        //Displays the information for new corpus
        QString chunk_status_string = QString("Corpus: %1 chunks").arg(static_cast<qulonglong>(processing_core_.chunk_count()));
        workspace_widget_->set_corpus_status(chunk_status_string);
        status_label_->setText("Corpus built successfully.");
    } catch(const std::exception& error){
        corpus_current_ = false;
        workspace_widget_->set_corpus_status("Corpus: Failed");
        status_label_->setText("Corpus build failed.");
        QMessageBox::warning(this, "Corpus Build Failed", QString::fromUtf8(error.what()));

    } catch(...){
        corpus_current_ = false;
        workspace_widget_->set_corpus_status("Corpus: Failed");
        status_label_->setText("Corpus build failed.");
        QMessageBox::warning(this, "Corpus Build Failed", "An unknown error occurred during the corpus building.");
    }
    update_action_states();
}

void MainWindow::document_selected(int row) {
    //Grab documents stored in Workspace
    const auto& docs = workspace_.documents();
    //Handling invalid selection
    if( row < 0 || static_cast<std::size_t>(row) >= docs.size()){
        workspace_widget_->clear_selected_document();
        return;
    }
    //Retrieving selected document
    const Document& document = docs[row];
    //Converting string to Qstring for the display
    QString title_qstring = QString::fromStdString(document.title());
    QString text_qstring = QString::fromStdString(document.text());
    workspace_widget_->show_selected_document(title_qstring, text_qstring);
}

void MainWindow::search_requested(const QString& query, int k) {
    //to ensure current corpus and valid query
    if(!corpus_current_ || query.trimmed().isEmpty()){ 
        return;
    }
    //Converting QString to string
    std::string search_q = query.toUtf8().toStdString();
    //Performing the search using the backend from Milestone 2
    std::vector<SearchResult> results_vector = processing_core_.search(search_q, k);
    //displaying results in Search Widget
    search_widget_->set_results(std::move(results_vector));
    //More to do: clear the previous context display when the ContextWidget object is terminated
    context_widget_->clear_context();
    status_label_->setText("Search complete.");
}

void MainWindow::query_changed(const QString& query) {
    
    (void)query;  // To suppress unused-parameter warning.
    update_action_states(); //Updates the buttons based on the new query state
}

void MainWindow::build_context_requested(int token_budget) {
   //ensuring the corpus is current + query is valid
   if(!corpus_current_ || !search_widget_->has_query()){
    return;
   }
   //Retrieve the current search widget settings
   QString query = search_widget_->query();
   int k = search_widget_->result_count();
   //Converting the query into an string
   std::string query_string = query.toUtf8().toStdString();
   //Calling the Milestone 2 backend
   std::vector<ContextItem> items = processing_core_.build_context(query_string, k, static_cast<std::size_t>(token_budget));
   //displaying the returned context
   context_widget_->set_context(items);
   status_label_->setText("Context built successfully.");
}

void MainWindow::source_document_requested(const QString& document_id) {
    const auto& docs = workspace_.documents();
    for(std::size_t i = 0; i < docs.size(); ++i){
        QString current_id = QString::fromStdString(docs[i].id());
        if(current_id == document_id){
            //select document inside list
            workspace_widget_->select_document(static_cast<int>(i));
            document_selected(static_cast<int>(i));
            return;
        }
    }
}

void MainWindow::term_inspection_requested(const QString& term,
                                           const QString& chunk_id) {
    if(!corpus_current_ || term.trimmed().isEmpty()){ //ensuring a valid corpus
        return;
    }
    //converting  the QString to string
    std::string term_string = term.trimmed().toUtf8().toStdString();
   //Document freq doesn't require a selected chunk
   std::size_t df = processing_core_.document_frequency(term_string);
   bool has_chunk = !chunk_id.isEmpty(); //determines whether a chunk has been selected
   std::size_t tf = 0;
   //Calculating the tf only when a chunk is available
   if(has_chunk){
    std::string chunk_str = chunk_id.toUtf8().toStdString();
    tf = processing_core_.term_frequency(term_string, chunk_str);

   }
   //displaying stats
   term_inspector_widget_->set_statistics(df, has_chunk, tf);
   status_label_->setText("Term Inspected.");


}

void MainWindow::reset_application() {
    //Resetting all major GUI components inside the interface
    //clearing backend workspace
    workspace_.clear();
    ++workspace_revision_;
    //Marking the existing corpus as invalid 
    corpus_current_ = false;
    //Reset the gui components (polymorphically)
    Resettable* components[] = {workspace_widget_, search_widget_, term_inspector_widget_, context_widget_};
    for (Resettable* comp : components){
        comp->reset();
    }
    status_label_->setText("Application reset."); //Restore the required app status
    update_action_states(); //restore the appropriate button availability

}

void MainWindow::exit_application() {
    if(exit_callback_ != nullptr){
        exit_callback_(this);
    }
}

void MainWindow::update_action_states() {
    //The build corpus needs at least one doc
    bool docs_have = !workspace_.documents().empty();
    workspace_widget_->set_build_corpus_enabled(docs_have && !build_in_progress_);
    //Search needs current corpus and valid query
    bool search_avail = (corpus_current_ && search_widget_->has_query());
    search_widget_->set_search_enabled(search_avail);
    context_widget_->set_build_context_enabled(search_avail);
    term_inspector_widget_->set_inspection_enabled(corpus_current_); //INspect needs current corpus
    

}

void MainWindow::invalidate_corpus() {
    //Invalidates the existing corpus whenever the workspace changes as previous 
    //results and stats might no longer match the current doc.
    corpus_current_ = false;
    if(build_in_progress_){
        workspace_widget_->set_corpus_status("Corpus: Building...");
    } else {
        workspace_widget_->set_corpus_status("Corpus: Not built");
    }
    
    clear_processing_outputs();
    update_action_states();
}

void MainWindow::clear_processing_outputs() {
    //Clearing the results from the previous searches
    if(search_widget_ != nullptr){
        search_widget_->clear_results();
    }
    //Clearing selected chunk and term stats
    term_inspector_widget_ ->set_selected_chunk(QString());
    term_inspector_widget_->clear_statistics();
    //clear context widget
    context_widget_->clear_context();
}


}  // namespace aiws::gui
